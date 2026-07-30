#ifndef BOARD_H
#define BOARD_H

/*
 * Virtual board-topology contract.
 *
 * Each board implements this interface in boards/<board>/fw/. All board
 * knowledge (PMIC addresses, I2C routing, page maps, valid VADJ voltages)
 * is declarative data exposed through pmic_desc_t descriptors; common code
 * walks the tables and never hard-codes an address.
 */

#include "fpga_types.h"

/* One mux write on the path from the root I2C bus to a device. */
typedef struct {
    u8 mux_addr;   /* 7-bit I2C address of the mux */
    u8 ctrl_byte;  /* control byte that selects the wanted channel */
} i2c_hop_t;

/* One PMBus page of a PMIC and the rail it drives. */
typedef struct {
    u8 page;
    const char *name;  /* e.g. "SW-D" */
    const char *rail;  /* e.g. "VADJ_FMC" */
} pmic_page_t;

/* One PMBus-controlled PMIC on the board. */
typedef struct {
    const char *name;               /* e.g. "PMIC2 (U180, IRPS5401)" */
    u8  bus;                        /* root I2C bus index (0 on single-bus boards) */
    u8  pmbus_addr;                 /* 7-bit PMBus address */
    const i2c_hop_t *route;         /* mux hops from the root bus to the device */
    int num_hops;                   /* 0 = directly on the root bus */
    const pmic_page_t *pages;
    int num_pages;
    int vadj_page;                  /* page driving VADJ, or -1 if not this PMIC */
    const unsigned int *vadj_valid_mv;
    int vadj_mv_count;
} pmic_desc_t;

/* One-time board setup (transport init). Returns FPGA_OK on success. */
int board_init(void);

/* Board name for the banner. */
const char *board_name(void);

/* PMIC descriptor table. */
int board_num_pmics(void);
const pmic_desc_t *board_pmic(int idx);

/* The descriptor whose vadj_page >= 0, or 0 (NULL) if the board has none. */
const pmic_desc_t *board_vadj_pmic(void);

/* Millisecond delay (board provides the timing source). */
void fpga_msleep(unsigned int ms);

#endif /* BOARD_H */
