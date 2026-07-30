#include "functions.h"

#include "board.h"
#include "fpga_console.h"
#include "pmic_app.h"
#include "cfg_clear_faults.h"

void clear_faults_default_cfg(clear_faults_cfg_t *c)
{
    c->all_pages = CLEAR_FAULTS_ALL_PAGES;
    c->verify = CLEAR_FAULTS_VERIFY;
}

int clear_faults_run(const clear_faults_cfg_t *c)
{
    const pmic_desc_t *d = board_vadj_pmic();
    int status;

    if (d == 0) {
        /* No VADJ PMIC: fall back to the first PMIC in the table */
        if (board_num_pmics() == 0) {
            fpga_printf("ERROR: this board declares no PMICs\r\n");
            return FPGA_ERROR;
        }
        d = board_pmic(0);
    }

    status = pmic_select(d);
    if (status != FPGA_OK) {
        return status;
    }

    status = pmic_clear_faults(d, c->all_pages,
                               (d->vadj_page >= 0) ? (u8)d->vadj_page : d->pages[0].page,
                               c->verify);
    if (status != FPGA_OK) {
        fpga_printf("FATAL: CLEAR_FAULTS failed\r\n");
        return status;
    }

    return FPGA_OK;
}

int clear_faults(void)
{
    clear_faults_cfg_t cfg;

    clear_faults_default_cfg(&cfg);
    return clear_faults_run(&cfg);
}
