#ifndef VADJ_H
#define VADJ_H

/*
 * VADJ rail operations against the board's VADJ-owning PMIC descriptor
 * (board_vadj_pmic()). The output is gated by the PMBus OPERATION command,
 * so setting a voltage and turning the output on are separate actions.
 */

/* Turn the VADJ output on (on = 1) or off (on = 0) via OPERATION. */
int vadj_set(int on);

/*
 * Write a new VADJ target voltage (VOUT_COMMAND) in millivolts.
 * force_vout_mode = write VOUT_MODE 0x18 (Linear16, exp -8) before writing.
 * also_enable = turn the output on after writing the new voltage.
 */
int vadj_configure(unsigned int target_mv, int force_vout_mode, int also_enable);

/*
 * Read and print the VADJ page status summary (OPERATION, VOUT_COMMAND,
 * READ_VOUT, STATUS_WORD) with RESULT: lines for machine parsing.
 */
void vadj_print_status(void);

#endif /* VADJ_H */
