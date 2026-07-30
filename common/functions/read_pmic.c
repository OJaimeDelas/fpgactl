#include "functions.h"

#include "board.h"
#include "fpga_console.h"
#include "pmic_app.h"
#include "cfg_read_pmic.h"

void read_pmic_default_cfg(read_pmic_cfg_t *c)
{
    c->target = PMIC_READ_TARGET;
}

int read_pmic_run(const read_pmic_cfg_t *c)
{
    int i;
    int status;
    int num = board_num_pmics();

    if (c->target >= num) {
        fpga_printf("ERROR: PMIC index %d out of range (board has %d)\r\n",
                    c->target, num);
        return FPGA_ERROR;
    }

    for (i = 0; i < num; i++) {
        const pmic_desc_t *d = board_pmic(i);

        if (c->target >= 0 && i != c->target) {
            continue;
        }

        status = pmic_select(d);
        if (status != FPGA_OK) {
            return status;
        }

        pmic_dump_all_pages(d);
    }

    return FPGA_OK;
}

int read_pmic(void)
{
    read_pmic_cfg_t cfg;

    read_pmic_default_cfg(&cfg);
    return read_pmic_run(&cfg);
}
