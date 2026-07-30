#ifndef PMBUS_H
#define PMBUS_H

/*
 * PMBus protocol layer: command transfers, Linear11/Linear16 value codecs,
 * and print helpers for decoded values.
 *
 * All transfer functions take the root bus index and the target's 7-bit
 * PMBus address so that any PMBus device on any bus can be addressed.
 * Board-independent: sits directly on the fpga_i2c virtual transport.
 */

#include "fpga_types.h"

/* PMBus command codes */
#define PMBUS_PAGE              0x00
#define PMBUS_OPERATION         0x01
#define PMBUS_ON_OFF_CONFIG     0x02
#define PMBUS_CLEAR_FAULTS      0x03
#define PMBUS_WRITE_PROTECT     0x10
#define PMBUS_CAPABILITY        0x19
#define PMBUS_VOUT_MODE         0x20
#define PMBUS_VOUT_COMMAND      0x21
#define PMBUS_VOUT_MAX          0x24
#define PMBUS_POWER_GOOD_ON     0x5E
#define PMBUS_POWER_GOOD_OFF    0x5F
#define PMBUS_STATUS_BYTE       0x78
#define PMBUS_STATUS_WORD       0x79
#define PMBUS_STATUS_VOUT       0x7A
#define PMBUS_STATUS_IOUT       0x7B
#define PMBUS_STATUS_INPUT      0x7C
#define PMBUS_STATUS_TEMP       0x7D
#define PMBUS_STATUS_CML        0x7E
#define PMBUS_READ_VIN          0x88
#define PMBUS_READ_IIN          0x89
#define PMBUS_READ_VOUT         0x8B
#define PMBUS_READ_IOUT         0x8C
#define PMBUS_READ_TEMP1        0x8D
#define PMBUS_READ_POUT         0x96
#define PMBUS_READ_PIN          0x97

/* OPERATION command: output-on bit */
#define PMBUS_OPERATION_ON      0x80
/* OPERATION command: immediate off (no bits set) */
#define PMBUS_OPERATION_OFF     0x00

/* STATUS_WORD bits */
#define PMBUS_STATUS_WORD_OFF   0x0040  /* Unit is not providing power to the output */
#define PMBUS_STATUS_WORD_PGN   0x0800  /* POWER_GOOD is negated (power not good) */

/* Encoding of a value returned by a PMBus read */
typedef enum {
    FMT_RAW8,           /* One byte, printed as hex */
    FMT_RAW16,          /* Two bytes, printed as hex */
    FMT_VOUT_LINEAR16,  /* Linear16 voltage, exponent taken from VOUT_MODE */
    FMT_LINEAR11        /* Linear11 value with embedded exponent */
} value_fmt_t;

/* Command transfers (bus = root I2C bus, addr = device's 7-bit address) */
int pmbus_send_byte(u8 bus, u8 addr, u8 cmd);
int pmbus_write_byte(u8 bus, u8 addr, u8 cmd, u8 value);
int pmbus_write_word(u8 bus, u8 addr, u8 cmd, u16 value);
int pmbus_read_byte(u8 bus, u8 addr, u8 cmd, u8 *value);
int pmbus_read_word(u8 bus, u8 addr, u8 cmd, u16 *value);

/* Value codecs */
int sign_extend(unsigned int value, unsigned int bits);
int vout_mode_to_exponent(u8 mode);
int linear16_to_milli(u16 raw, int exponent);
int linear11_to_milli(u16 raw);
u16 millivolts_to_linear16_word(unsigned int mv, int exponent);

/* Print helpers */
void print_fixed_milli(int milli, const char *unit);
void print_vout_mode(u8 mode);
void print_word_value(value_fmt_t fmt, u16 raw, int vout_exp, const char *unit);

#endif /* PMBUS_H */
