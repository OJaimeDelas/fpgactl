/*
 * Default script: runs when neither FUNCTION= nor SCRIPT= is given.
 * Reads and prints the PMBus telemetry of every PMIC on the board
 * (read-only, safe on any board).
 *
 * A script may call any function from functions.h, either with defaults:
 *
 *     return read_pmic();
 *
 * or with per-call configuration:
 *
 *     config_vadj_cfg_t c;
 *     config_vadj_default_cfg(&c);
 *     c.mv = 1800;
 *     c.also_enable = 1;
 *     return config_vadj_run(&c);
 */

#include "functions.h"

int fpga_script(void)
{
    return read_pmic();
}
