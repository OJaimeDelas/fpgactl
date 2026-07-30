#ifndef BOARD_IO_H
#define BOARD_IO_H

/*
 * ZCU104 board IO definitions: everything address/topology-specific lives
 * here. The descriptor tables built from these constants are in
 * board_zcu104.c.
 *
 * PMBus path: PS I2C1 (MIO16 SCL / MIO17 SDA) -> U34 TCA9548A mux channel 2
 * -> PMIC2 (U180, Infineon IRPS5401).
 */

/* Root I2C bus index (the ZCU104 uses a single controller: PS I2C1) */
#define ZCU104_I2C_BUS       0

/* Standard-mode I2C clock for the PS I2C controller */
#define ZCU104_I2C_SCLK_HZ   100000U

/* U34 TCA9548A I2C multiplexer, 7-bit address */
#define U34_I2C_ADDR         0x74
/* U34 control byte: bit 2 set enables downstream channel 2 (PMIC branch) */
#define U34_SELECT_CH2       0x04

/* PMIC2 (U180), 7-bit PMBus address */
#define PMIC2_PMBUS_ADDR     0x44

/* PMIC2 page numbers */
#define PMIC2_PAGE_UTIL_3V3  0x00  /* SW-A */
#define PMIC2_PAGE_UTIL_1V13 0x01  /* SW-B */
#define PMIC2_PAGE_UTIL_5V0  0x02  /* SW-C */
#define PMIC2_PAGE_VADJ_FMC  0x03  /* SW-D = VADJ_FMC */
#define PMIC2_PAGE_MGTRAVCC  0x04  /* LDO  */

#endif /* BOARD_IO_H */
