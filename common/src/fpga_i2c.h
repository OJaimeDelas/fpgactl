#ifndef FPGA_I2C_H
#define FPGA_I2C_H

/*
 * Virtual I2C transport interface.
 *
 * Board-agnostic code only ever calls these three functions; each board
 * provides exactly one implementation in boards/<board>/fw/ (PS I2C
 * controller, PL AXI IIC core, ...).
 *
 * bus selects the I2C controller on boards that have more than one root
 * bus; boards with a single controller ignore it. The value comes from the
 * pmic_desc_t of the device being addressed.
 */

#include "fpga_types.h"

/* Initialize the board's I2C transport (all buses). */
int fpga_i2c_init(void);

/* Plain master write: START, addr+W, data bytes, STOP. */
int fpga_i2c_write(u8 bus, u8 slave_addr, const u8 *data, int len);

/*
 * SMBus/PMBus-style command read:
 * START, addr+W, command byte, REPEATED START, addr+R, data bytes, STOP.
 */
int fpga_i2c_read_cmd(u8 bus, u8 slave_addr, u8 cmd, u8 *data, int len);

#endif /* FPGA_I2C_H */
