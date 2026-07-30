#include "vadj.h"

#include "board.h"
#include "fpga_console.h"
#include "pmbus.h"
#include "pmic_app.h"

static const pmic_desc_t *vadj_desc(void)
{
    const pmic_desc_t *d = board_vadj_pmic();

    if (d == 0) {
        fpga_printf("ERROR: this board declares no VADJ-owning PMIC\r\n");
    }

    return d;
}

static const char *vadj_page_name(const pmic_desc_t *d)
{
    int i;

    for (i = 0; i < d->num_pages; i++) {
        if (d->pages[i].page == (u8)d->vadj_page) {
            return d->pages[i].name;
        }
    }

    return "?";
}

static const char *vadj_page_rail(const pmic_desc_t *d)
{
    int i;

    for (i = 0; i < d->num_pages; i++) {
        if (d->pages[i].page == (u8)d->vadj_page) {
            return d->pages[i].rail;
        }
    }

    return "?";
}

static int check_vadj_target(const pmic_desc_t *d, unsigned int mv)
{
    int i;

    for (i = 0; i < d->vadj_mv_count; i++) {
        if (d->vadj_valid_mv[i] == mv) {
            return FPGA_OK;
        }
    }

    fpga_printf("ERROR: invalid VADJ target %u mV\r\n", mv);
    fpga_printf("Allowed values are:");
    for (i = 0; i < d->vadj_mv_count; i++) {
        fpga_printf(" %u", d->vadj_valid_mv[i]);
    }
    fpga_printf("\r\n");
    return FPGA_ERROR;
}

void vadj_print_status(void)
{
    const pmic_desc_t *d = vadj_desc();
    int status;
    int exp = -8;
    u8 mode;
    u8 op;
    u16 w;
    int mv;

    if (d == 0) {
        return;
    }

    fpga_printf("\r\nVADJ status (page 0x%02x)\r\n", (u8)d->vadj_page);

    status = pmic_select_page(d, (u8)d->vadj_page);
    if (status != FPGA_OK) {
        return;
    }

    status = pmic_read_byte(d, PMBUS_OPERATION, &op);
    if (status == FPGA_OK) {
        fpga_printf("OPERATION    = 0x%02x  output %s\r\n",
                    op, (op & PMBUS_OPERATION_ON) ? "ON" : "OFF");
        fpga_printf("RESULT: vadj_operation=0x%02x\r\n", op);
    } else {
        fpga_printf("OPERATION    = READ FAILED\r\n");
    }

    status = pmic_read_vout_mode_exp(d, &exp, &mode);
    if (status != FPGA_OK) {
        exp = -8;
    }

    status = pmic_read_word(d, PMBUS_VOUT_COMMAND, &w);
    if (status == FPGA_OK) {
        mv = linear16_to_milli(w, exp);
        fpga_printf("VOUT_COMMAND = 0x%04x  ", w);
        print_fixed_milli(mv, "V");
        fpga_printf("\r\n");
        fpga_printf("RESULT: vadj_vout_command_mv=%d\r\n", mv);
    } else {
        fpga_printf("VOUT_COMMAND = READ FAILED\r\n");
    }

    status = pmic_read_word(d, PMBUS_READ_VOUT, &w);
    if (status == FPGA_OK) {
        mv = linear16_to_milli(w, exp);
        fpga_printf("READ_VOUT    = 0x%04x  ", w);
        print_fixed_milli(mv, "V");
        fpga_printf("\r\n");
        fpga_printf("RESULT: vadj_read_vout_mv=%d\r\n", mv);
    } else {
        fpga_printf("READ_VOUT    = READ FAILED\r\n");
    }

    status = pmic_read_word(d, PMBUS_STATUS_WORD, &w);
    if (status == FPGA_OK) {
        fpga_printf("STATUS_WORD  = 0x%04x  output is %s, power is %s\r\n", w,
                    (w & PMBUS_STATUS_WORD_OFF) ? "OFF" : "ON",
                    (w & PMBUS_STATUS_WORD_PGN) ? "NOT GOOD" : "GOOD");
        fpga_printf("RESULT: vadj_status_word=0x%04x\r\n", w);
    } else {
        fpga_printf("STATUS_WORD  = READ FAILED\r\n");
    }
}

int vadj_set(int on)
{
    const pmic_desc_t *d = vadj_desc();
    int status;
    u8 op;
    u8 target = on ? PMBUS_OPERATION_ON : PMBUS_OPERATION_OFF;

    if (d == 0) {
        return FPGA_ERROR;
    }

    fpga_printf("\r\nVADJ OUTPUT %s\r\n", on ? "ENABLE" : "DISABLE");
    fpga_printf("Selecting PAGE 0x%02x (%s / %s)\r\n", (u8)d->vadj_page,
                vadj_page_name(d), vadj_page_rail(d));

    status = pmic_select_page(d, (u8)d->vadj_page);
    if (status != FPGA_OK) {
        return status;
    }

    status = pmic_read_byte(d, PMBUS_OPERATION, &op);
    if (status != FPGA_OK) {
        fpga_printf("ERROR: could not read OPERATION\r\n");
        return status;
    }

    if (on && (op & PMBUS_OPERATION_ON)) {
        fpga_printf("Output is already enabled (OPERATION = 0x%02x)\r\n", op);
        return FPGA_OK;
    }

    if (!on && (op & PMBUS_OPERATION_ON) == 0) {
        fpga_printf("Output is already disabled (OPERATION = 0x%02x)\r\n", op);
        return FPGA_OK;
    }

    fpga_printf("Output is %s (OPERATION = 0x%02x), writing OPERATION = 0x%02x\r\n",
                on ? "OFF" : "ON", op, target);

    status = pmic_write_byte(d, PMBUS_OPERATION, target);
    if (status != FPGA_OK) {
        fpga_printf("ERROR: OPERATION write failed\r\n");
        return status;
    }

    fpga_msleep(100);

    fpga_printf("%s command sent\r\n", on ? "Enable" : "Disable");
    return FPGA_OK;
}

int vadj_configure(unsigned int target_mv, int force_vout_mode, int also_enable)
{
    const pmic_desc_t *d = vadj_desc();
    int status;
    u8 mode;
    u8 op;
    int exp;
    u16 raw;
    u16 rb;
    int mv;

    if (d == 0) {
        return FPGA_ERROR;
    }

    status = check_vadj_target(d, target_mv);
    if (status != FPGA_OK) {
        return status;
    }

    fpga_printf("\r\nVADJ CONFIGURATION\r\n");
    fpga_printf("Requested VADJ target: %u mV\r\n", target_mv);
    fpga_printf("Selecting PAGE 0x%02x (%s / %s)\r\n", (u8)d->vadj_page,
                vadj_page_name(d), vadj_page_rail(d));

    status = pmic_select_page(d, (u8)d->vadj_page);
    if (status != FPGA_OK) {
        return status;
    }

    status = pmic_read_byte(d, PMBUS_OPERATION, &op);
    if (status == FPGA_OK && (op & PMBUS_OPERATION_ON) == 0) {
        fpga_printf("NOTE: the VADJ output is currently OFF (OPERATION = 0x%02x)\r\n", op);
        if (also_enable) {
            fpga_printf("It will be enabled after the new voltage is written\r\n");
        } else {
            fpga_printf("The new voltage only appears on the rail once the output is enabled:\r\n");
            fpga_printf("run FUNCTION=enable_vadj, or set CONFIG_VADJ_ALSO_ENABLE to 1\r\n");
        }
    }

    if (force_vout_mode) {
        fpga_printf("Forcing VOUT_MODE to 0x18 on page 0x%02x\r\n", (u8)d->vadj_page);
        status = pmic_write_byte(d, PMBUS_VOUT_MODE, 0x18);
        if (status != FPGA_OK) {
            fpga_printf("ERROR: failed to write VOUT_MODE\r\n");
            return status;
        }
    }

    status = pmic_read_vout_mode_exp(d, &exp, &mode);
    if (status != FPGA_OK) {
        fpga_printf("ERROR: failed to read VOUT_MODE\r\n");
        return status;
    }

    fpga_printf("Current VOUT_MODE: ");
    print_vout_mode(mode);
    fpga_printf("\r\n");

    if (!(mode == 0x18 || mode == 0x17 || mode == 0x14)) {
        fpga_printf("ERROR: unexpected VOUT_MODE. Refusing to write VOUT_COMMAND.\r\n");
        return FPGA_ERROR;
    }

    raw = millivolts_to_linear16_word(target_mv, exp);

    fpga_printf("Computed VOUT_COMMAND word: 0x%04x\r\n", raw);
    fpga_printf("Write order on the bus: low byte 0x%02x, high byte 0x%02x\r\n",
                raw & 0xFF, (raw >> 8) & 0xFF);

    status = pmic_read_word(d, PMBUS_VOUT_COMMAND, &rb);
    if (status == FPGA_OK) {
        fpga_printf("Before write, VOUT_COMMAND = 0x%04x  ", rb);
        mv = linear16_to_milli(rb, exp);
        print_fixed_milli(mv, "V");
        fpga_printf("\r\n");
    } else {
        fpga_printf("Warning: could not read VOUT_COMMAND before write\r\n");
    }

    fpga_printf("Writing VOUT_COMMAND now\r\n");
    status = pmic_write_word(d, PMBUS_VOUT_COMMAND, raw);
    if (status != FPGA_OK) {
        fpga_printf("ERROR: VOUT_COMMAND write failed\r\n");
        return status;
    }

    fpga_msleep(100);

    status = pmic_read_word(d, PMBUS_VOUT_COMMAND, &rb);
    if (status == FPGA_OK) {
        fpga_printf("After write, VOUT_COMMAND = 0x%04x  ", rb);
        mv = linear16_to_milli(rb, exp);
        print_fixed_milli(mv, "V");
        fpga_printf("\r\n");
    } else {
        fpga_printf("ERROR: could not read VOUT_COMMAND after write\r\n");
        return status;
    }

    status = pmic_read_word(d, PMBUS_READ_VOUT, &rb);
    if (status == FPGA_OK) {
        fpga_printf("After write, READ_VOUT    = 0x%04x  ", rb);
        mv = linear16_to_milli(rb, exp);
        print_fixed_milli(mv, "V");
        fpga_printf("\r\n");
    } else {
        fpga_printf("Warning: could not read READ_VOUT after write\r\n");
    }

    status = pmic_read_word(d, PMBUS_STATUS_WORD, &rb);
    if (status == FPGA_OK) {
        fpga_printf("STATUS_WORD = 0x%04x\r\n", rb);
    } else {
        fpga_printf("Warning: could not read STATUS_WORD\r\n");
    }

    if (also_enable) {
        status = vadj_set(1);
        if (status != FPGA_OK) {
            return status;
        }
        vadj_print_status();
    }

    fpga_printf("\r\nIMPORTANT: verify VADJ with a multimeter before connecting external hardware.\r\n");
    fpga_printf("Configuration complete\r\n");

    return FPGA_OK;
}
