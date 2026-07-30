#include "functions.h"

#include "board.h"
#include "fpga_console.h"
#include "pmic_app.h"
#include "vadj.h"
#include "cfg_config_vadj.h"

void config_vadj_default_cfg(config_vadj_cfg_t *c)
{
    c->mv = CONFIG_VADJ_MV;
    c->dump_all = CONFIG_VADJ_DUMP_ALL_PAGES;
    c->also_enable = CONFIG_VADJ_ALSO_ENABLE;
    c->force_vout_mode = CONFIG_FORCE_VOUT_MODE_0X18;
}

int config_vadj_run(const config_vadj_cfg_t *c)
{
    const pmic_desc_t *d = board_vadj_pmic();
    int status;

    if (d == 0) {
        fpga_printf("ERROR: this board declares no VADJ-owning PMIC\r\n");
        return FPGA_ERROR;
    }

    status = pmic_select(d);
    if (status != FPGA_OK) {
        return status;
    }

    if (c->dump_all) {
        pmic_dump_all_pages(d);
    }

    status = vadj_configure(c->mv, c->force_vout_mode, c->also_enable);
    if (status != FPGA_OK) {
        fpga_printf("FATAL: VADJ configuration failed\r\n");
        return status;
    }

    if (c->dump_all) {
        pmic_dump_all_pages(d);
    }

    return FPGA_OK;
}

int config_vadj(void)
{
    config_vadj_cfg_t cfg;

    config_vadj_default_cfg(&cfg);
    return config_vadj_run(&cfg);
}
