#include "functions.h"

#include "board.h"
#include "fpga_console.h"
#include "pmic_app.h"
#include "vadj.h"
#include "cfg_enable_vadj.h"

void enable_vadj_default_cfg(enable_vadj_cfg_t *c)
{
    c->enable = ENABLE_DISABLE;
    c->verify = ENABLE_VADJ_VERIFY;
}

int enable_vadj_run(const enable_vadj_cfg_t *c)
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

    status = vadj_set(c->enable);
    if (status != FPGA_OK) {
        fpga_printf("FATAL: VADJ %s failed\r\n", c->enable ? "enable" : "disable");
        return status;
    }

    if (c->verify) {
        vadj_print_status();
    }

    if (c->enable) {
        fpga_printf("\r\nIMPORTANT: verify VADJ with a multimeter before connecting external hardware.\r\n");
    }

    return FPGA_OK;
}

int enable_vadj(void)
{
    enable_vadj_cfg_t cfg;

    enable_vadj_default_cfg(&cfg);
    return enable_vadj_run(&cfg);
}
