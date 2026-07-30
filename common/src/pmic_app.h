#ifndef PMIC_APP_H
#define PMIC_APP_H

/*
 * Generic PMIC operations over a pmic_desc_t: bus routing, page selection,
 * full telemetry dumps, and fault clearing. Board knowledge comes entirely
 * from the descriptor.
 */

#include "fpga_types.h"
#include "board.h"

/* Walk a mux-hop route (generic; empty routes are a no-op). */
int i2c_route(u8 bus, const i2c_hop_t *hops, int num_hops);

/* Route the root bus to the PMIC (mux writes from the descriptor). */
int pmic_select(const pmic_desc_t *d);

/* Select which PMIC page subsequent commands address. */
int pmic_select_page(const pmic_desc_t *d, u8 page);

/* Read VOUT_MODE on the current page and derive its Linear16 exponent. */
int pmic_read_vout_mode_exp(const pmic_desc_t *d, int *exp_out, u8 *mode_out);

/* Convenience transfers against a descriptor (current page). */
int pmic_send_byte(const pmic_desc_t *d, u8 cmd);
int pmic_write_byte(const pmic_desc_t *d, u8 cmd, u8 value);
int pmic_write_word(const pmic_desc_t *d, u8 cmd, u16 value);
int pmic_read_byte(const pmic_desc_t *d, u8 cmd, u8 *value);
int pmic_read_word(const pmic_desc_t *d, u8 cmd, u16 *value);

/* Read and print every telemetry command on all pages of the PMIC. */
void pmic_dump_all_pages(const pmic_desc_t *d);

/*
 * Send CLEAR_FAULTS, either on one page (all_pages = 0, page = which) or on
 * every page (all_pages = 1). verify = read and print STATUS_WORD before
 * and after each clear.
 */
int pmic_clear_faults(const pmic_desc_t *d, int all_pages, u8 page, int verify);

#endif /* PMIC_APP_H */
