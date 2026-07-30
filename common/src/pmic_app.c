#include "pmic_app.h"

#include "fpga_i2c.h"
#include "fpga_console.h"
#include "pmbus.h"

typedef struct {
    u8 cmd;
    const char *name;
    const char *caption;
    value_fmt_t fmt;
    const char *unit;   /* Physical unit of the decoded value; "" when the value is printed as raw hex */
} read_cmd_t;

static const read_cmd_t byte_reads[] = {
    {PMBUS_PAGE,          "PAGE",          "currently selected page", FMT_RAW8, ""},
    {PMBUS_OPERATION,     "OPERATION",     "operation state", FMT_RAW8, ""},
    {PMBUS_ON_OFF_CONFIG, "ON_OFF_CONFIG", "on/off configuration", FMT_RAW8, ""},
    {PMBUS_WRITE_PROTECT, "WRITE_PROTECT", "PMBus write protection", FMT_RAW8, ""},
    {PMBUS_CAPABILITY,    "CAPABILITY",    "PMBus capability byte", FMT_RAW8, ""},
    {PMBUS_VOUT_MODE,     "VOUT_MODE",     "voltage format and exponent", FMT_RAW8, ""},
    {PMBUS_STATUS_BYTE,   "STATUS_BYTE",   "summary status", FMT_RAW8, ""},
    {PMBUS_STATUS_VOUT,   "STATUS_VOUT",   "VOUT status", FMT_RAW8, ""},
    {PMBUS_STATUS_IOUT,   "STATUS_IOUT",   "IOUT status", FMT_RAW8, ""},
    {PMBUS_STATUS_INPUT,  "STATUS_INPUT",  "input status", FMT_RAW8, ""},
    {PMBUS_STATUS_TEMP,   "STATUS_TEMP",   "temperature status", FMT_RAW8, ""},
    {PMBUS_STATUS_CML,    "STATUS_CML",    "communication/memory/logic status", FMT_RAW8, ""}
};

static const read_cmd_t word_reads[] = {
    {PMBUS_STATUS_WORD,    "STATUS_WORD",    "full status word", FMT_RAW16, ""},
    {PMBUS_VOUT_COMMAND,   "VOUT_COMMAND",   "commanded target output voltage", FMT_VOUT_LINEAR16, "V"},
    {PMBUS_VOUT_MAX,       "VOUT_MAX",       "maximum allowed commanded output voltage", FMT_VOUT_LINEAR16, "V"},
    {PMBUS_POWER_GOOD_ON,  "POWER_GOOD_ON",  "power-good on threshold", FMT_VOUT_LINEAR16, "V"},
    {PMBUS_POWER_GOOD_OFF, "POWER_GOOD_OFF", "power-good off threshold", FMT_VOUT_LINEAR16, "V"},
    {PMBUS_READ_VIN,       "READ_VIN",       "input voltage", FMT_LINEAR11, "V"},
    {PMBUS_READ_IIN,       "READ_IIN",       "input current", FMT_LINEAR11, "A"},
    {PMBUS_READ_VOUT,      "READ_VOUT",      "reported output voltage/reference", FMT_VOUT_LINEAR16, "V"},
    {PMBUS_READ_IOUT,      "READ_IOUT",      "output current", FMT_LINEAR11, "A"},
    {PMBUS_READ_TEMP1,     "READ_TEMPERATURE_1", "temperature", FMT_LINEAR11, "C"},
    {PMBUS_READ_POUT,      "READ_POUT",      "output power", FMT_LINEAR11, "W"},
    {PMBUS_READ_PIN,       "READ_PIN",       "input power", FMT_LINEAR11, "W"}
};

int i2c_route(u8 bus, const i2c_hop_t *hops, int num_hops)
{
    int i;
    int status;

    for (i = 0; i < num_hops; i++) {
        fpga_printf("Selecting I2C mux 0x%02x channel byte 0x%02x\r\n",
                    hops[i].mux_addr, hops[i].ctrl_byte);

        status = fpga_i2c_write(bus, hops[i].mux_addr, &hops[i].ctrl_byte, 1);
        if (status != FPGA_OK) {
            fpga_printf("ERROR: failed to select I2C mux 0x%02x\r\n",
                        hops[i].mux_addr);
            return status;
        }
    }

    return FPGA_OK;
}

int pmic_select(const pmic_desc_t *d)
{
    int status;

    status = i2c_route(d->bus, d->route, d->num_hops);
    if (status != FPGA_OK) {
        fpga_printf("ERROR: bus routing to %s failed\r\n", d->name);
    }

    return status;
}

int pmic_send_byte(const pmic_desc_t *d, u8 cmd)
{
    return pmbus_send_byte(d->bus, d->pmbus_addr, cmd);
}

int pmic_write_byte(const pmic_desc_t *d, u8 cmd, u8 value)
{
    return pmbus_write_byte(d->bus, d->pmbus_addr, cmd, value);
}

int pmic_write_word(const pmic_desc_t *d, u8 cmd, u16 value)
{
    return pmbus_write_word(d->bus, d->pmbus_addr, cmd, value);
}

int pmic_read_byte(const pmic_desc_t *d, u8 cmd, u8 *value)
{
    return pmbus_read_byte(d->bus, d->pmbus_addr, cmd, value);
}

int pmic_read_word(const pmic_desc_t *d, u8 cmd, u16 *value)
{
    return pmbus_read_word(d->bus, d->pmbus_addr, cmd, value);
}

int pmic_select_page(const pmic_desc_t *d, u8 page)
{
    int status;

    status = pmic_write_byte(d, PMBUS_PAGE, page);
    if (status != FPGA_OK) {
        fpga_printf("ERROR: failed to write PAGE = 0x%02x\r\n", page);
    }

    fpga_msleep(1);
    return status;
}

int pmic_read_vout_mode_exp(const pmic_desc_t *d, int *exp_out, u8 *mode_out)
{
    u8 mode;
    int status;

    status = pmic_read_byte(d, PMBUS_VOUT_MODE, &mode);
    if (status != FPGA_OK) {
        return status;
    }

    *mode_out = mode;
    *exp_out = vout_mode_to_exponent(mode);

    return FPGA_OK;
}

static void dump_one_page(const pmic_desc_t *d, const pmic_page_t *page)
{
    int status;
    int i;
    int vout_exp = -8;
    u8 mode = 0;
    u8 b;
    u16 w;

    fpga_printf("\r\n============================================================\r\n");
    fpga_printf("PAGE 0x%02x  %s  rail: %s\r\n", page->page, page->name, page->rail);
    fpga_printf("============================================================\r\n");

    status = pmic_select_page(d, page->page);
    if (status != FPGA_OK) {
        fpga_printf("Skipping page because PAGE write failed\r\n");
        return;
    }

    status = pmic_read_vout_mode_exp(d, &vout_exp, &mode);
    if (status == FPGA_OK) {
        fpga_printf("%-20s %-35s : ", "VOUT_MODE", "voltage format and exponent");
        print_vout_mode(mode);
        fpga_printf("\r\n");
    } else {
        fpga_printf("%-20s %-35s : READ FAILED\r\n", "VOUT_MODE", "voltage format and exponent");
        fpga_printf("Using exponent -8 only for formatting other voltage words on this page\r\n");
        vout_exp = -8;
    }

    for (i = 0; i < (int)(sizeof(byte_reads) / sizeof(byte_reads[0])); i++) {
        if (byte_reads[i].cmd == PMBUS_VOUT_MODE) {
            continue;
        }

        status = pmic_read_byte(d, byte_reads[i].cmd, &b);
        fpga_printf("%-20s %-35s : ", byte_reads[i].name, byte_reads[i].caption);

        if (status == FPGA_OK) {
            fpga_printf("0x%02x  %u\r\n", b, b);
        } else {
            fpga_printf("READ FAILED\r\n");
        }
    }

    for (i = 0; i < (int)(sizeof(word_reads) / sizeof(word_reads[0])); i++) {
        status = pmic_read_word(d, word_reads[i].cmd, &w);
        fpga_printf("%-20s %-35s : ", word_reads[i].name, word_reads[i].caption);

        if (status == FPGA_OK) {
            print_word_value(word_reads[i].fmt, w, vout_exp, word_reads[i].unit);
            fpga_printf("\r\n");
        } else {
            fpga_printf("READ FAILED\r\n");
        }
    }
}

void pmic_dump_all_pages(const pmic_desc_t *d)
{
    int i;

    fpga_printf("\r\n%s full page read\r\n", d->name);
    fpga_printf("Target PMBus address: 0x%02x\r\n", d->pmbus_addr);
    fpga_printf("Pages read:");
    for (i = 0; i < d->num_pages; i++) {
        fpga_printf(" %s%s", d->pages[i].name, (i + 1 < d->num_pages) ? "," : "");
    }
    fpga_printf("\r\n");

    for (i = 0; i < d->num_pages; i++) {
        dump_one_page(d, &d->pages[i]);
    }

    fpga_printf("\r\nPage read complete\r\n");
}

static int clear_faults_one_page(const pmic_desc_t *d, const pmic_page_t *page, int verify)
{
    int status;
    u16 w;

    fpga_printf("\r\nPAGE 0x%02x  %s  rail: %s\r\n", page->page, page->name, page->rail);

    status = pmic_select_page(d, page->page);
    if (status != FPGA_OK) {
        return status;
    }

    if (verify) {
        status = pmic_read_word(d, PMBUS_STATUS_WORD, &w);
        if (status == FPGA_OK) {
            fpga_printf("STATUS_WORD before clear = 0x%04x\r\n", w);
        } else {
            fpga_printf("STATUS_WORD before clear = READ FAILED\r\n");
        }
    }

    status = pmic_send_byte(d, PMBUS_CLEAR_FAULTS);
    if (status != FPGA_OK) {
        fpga_printf("ERROR: CLEAR_FAULTS send failed\r\n");
        return status;
    }

    fpga_msleep(1);

    if (verify) {
        status = pmic_read_word(d, PMBUS_STATUS_WORD, &w);
        if (status == FPGA_OK) {
            fpga_printf("STATUS_WORD after clear  = 0x%04x\r\n", w);
            if (w != 0) {
                fpga_printf("Bits still set indicate a fault condition that is still present\r\n");
            }
        } else {
            fpga_printf("STATUS_WORD after clear  = READ FAILED\r\n");
        }
    }

    return FPGA_OK;
}

int pmic_clear_faults(const pmic_desc_t *d, int all_pages, u8 page, int verify)
{
    int status;
    int i;

    fpga_printf("\r\n%s CLEAR_FAULTS\r\n", d->name);
    fpga_printf("Sending command 0x%02x to PMBus address 0x%02x\r\n",
                PMBUS_CLEAR_FAULTS, d->pmbus_addr);

    for (i = 0; i < d->num_pages; i++) {
        if (!all_pages && d->pages[i].page != page) {
            continue;
        }

        status = clear_faults_one_page(d, &d->pages[i], verify);
        if (status != FPGA_OK) {
            return status;
        }
    }

    fpga_printf("\r\nClear faults complete\r\n");
    return FPGA_OK;
}
