#include "pmbus.h"

#include "fpga_i2c.h"
#include "fpga_console.h"

int pmbus_send_byte(u8 bus, u8 addr, u8 cmd)
{
    /* SMBus Send Byte: the command code alone, with no data byte */
    return fpga_i2c_write(bus, addr, &cmd, 1);
}

int pmbus_write_byte(u8 bus, u8 addr, u8 cmd, u8 value)
{
    u8 tx[2];

    tx[0] = cmd;
    tx[1] = value;

    return fpga_i2c_write(bus, addr, tx, 2);
}

int pmbus_write_word(u8 bus, u8 addr, u8 cmd, u16 value)
{
    u8 tx[3];

    /* PMBus/SMBus words travel low byte first */
    tx[0] = cmd;
    tx[1] = (u8)(value & 0xFF);
    tx[2] = (u8)((value >> 8) & 0xFF);

    return fpga_i2c_write(bus, addr, tx, 3);
}

int pmbus_read_byte(u8 bus, u8 addr, u8 cmd, u8 *value)
{
    return fpga_i2c_read_cmd(bus, addr, cmd, value, 1);
}

int pmbus_read_word(u8 bus, u8 addr, u8 cmd, u16 *value)
{
    u8 rx[2];
    int status;

    status = fpga_i2c_read_cmd(bus, addr, cmd, rx, 2);
    if (status != FPGA_OK) {
        return status;
    }

    *value = ((u16)rx[1] << 8) | rx[0];
    return FPGA_OK;
}

int sign_extend(unsigned int value, unsigned int bits)
{
    unsigned int mask = 1U << (bits - 1U);
    return (int)((value ^ mask) - mask);
}

int vout_mode_to_exponent(u8 mode)
{
    return sign_extend(mode & 0x1F, 5);
}

static int div_round_s64(s64 numerator, s64 denominator)
{
    if (denominator == 0) {
        return 0;
    }

    if (numerator >= 0) {
        return (int)((numerator + denominator / 2) / denominator);
    }

    return (int)((numerator - denominator / 2) / denominator);
}

int linear16_to_milli(u16 raw, int exponent)
{
    s64 value = (s64)raw * 1000LL;

    if (exponent >= 0) {
        value <<= exponent;
    } else {
        value = div_round_s64(value, 1LL << (-exponent));
    }

    return (int)value;
}

int linear11_to_milli(u16 raw)
{
    int exponent = sign_extend((raw >> 11) & 0x1F, 5);
    int mantissa = sign_extend(raw & 0x07FF, 11);
    s64 value = (s64)mantissa * 1000LL;

    if (exponent >= 0) {
        value <<= exponent;
    } else {
        value = div_round_s64(value, 1LL << (-exponent));
    }

    return (int)value;
}

u16 millivolts_to_linear16_word(unsigned int mv, int exponent)
{
    u64 raw;

    if (exponent < 0) {
        raw = ((u64)mv * (1ULL << (-exponent)) + 500ULL) / 1000ULL;
    } else {
        u64 denom = 1000ULL * (1ULL << exponent);
        raw = ((u64)mv + denom / 2ULL) / denom;
    }

    if (raw > 0xFFFFULL) {
        raw = 0xFFFFULL;
    }

    return (u16)raw;
}

static int abs_i32(int v)
{
    return (v < 0) ? -v : v;
}

void print_fixed_milli(int milli, const char *unit)
{
    int neg = (milli < 0);
    int absval = abs_i32(milli);
    int integer = absval / 1000;
    int frac = absval % 1000;

    if (neg) {
        fpga_printf("-");
    }

    fpga_printf("%d.%03d %s", integer, frac, unit);
}

void print_vout_mode(u8 mode)
{
    int exp = vout_mode_to_exponent(mode);
    fpga_printf("0x%02x", mode);
    fpga_printf("  Linear16 exponent %d", exp);
}

void print_word_value(value_fmt_t fmt, u16 raw, int vout_exp, const char *unit)
{
    int milli;

    if (fmt == FMT_RAW16) {
        fpga_printf("0x%04x  %u", raw, raw);
        return;
    }

    if (fmt == FMT_VOUT_LINEAR16) {
        milli = linear16_to_milli(raw, vout_exp);
        fpga_printf("0x%04x  ", raw);
        print_fixed_milli(milli, unit);
        return;
    }

    if (fmt == FMT_LINEAR11) {
        milli = linear11_to_milli(raw);
        fpga_printf("0x%04x  ", raw);
        print_fixed_milli(milli, unit);
        return;
    }

    fpga_printf("0x%04x  %u", raw, raw);
}
