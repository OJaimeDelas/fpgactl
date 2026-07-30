/*
 * Enable the VADJ rail, then configure it to 1.8 V.
 *
 * Run with:  make run SCRIPT=scripts/smartchip_config_vadj.c
 */

#include "functions.h"

int fpga_script(void)
{
    enable_vadj_cfg_t enable;
    config_vadj_cfg_t config;

    enable_vadj_default_cfg(&enable);
    enable.enable = 1;
    enable.verify = 1;
    if (enable_vadj_run(&enable) != 0) {
        return 1;
    }

    config_vadj_default_cfg(&config);
    config.mv = 1800;
    config.dump_all = 0;
    config.also_enable = 1;   /* re-assert ON after the voltage write */
    if (config_vadj_run(&config) != 0) {
        return 1;
    }

    return 0;
}
