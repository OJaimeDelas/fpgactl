#include "board.h"

#include "board_io.h"
#include "fpga_i2c.h"
#include "sleep.h"

/* Route from PS I2C1 to the PMIC branch: one hop through U34 */
static const i2c_hop_t pmic_route[] = {
    {U34_I2C_ADDR, U34_SELECT_CH2}
};

static const pmic_page_t pmic2_pages[] = {
    {PMIC2_PAGE_UTIL_3V3,  "SW-A", "UTIL_3V3"},
    {PMIC2_PAGE_UTIL_1V13, "SW-B", "UTIL_1V13"},
    {PMIC2_PAGE_UTIL_5V0,  "SW-C", "UTIL_5V0"},
    {PMIC2_PAGE_VADJ_FMC,  "SW-D", "VADJ_FMC"},
    {PMIC2_PAGE_MGTRAVCC,  "LDO",  "MGTRAVCC"}
};

static const unsigned int vadj_valid_mv[] = {1200, 1500, 1800};

static const pmic_desc_t pmics[] = {
    {
        .name = "PMIC2 (U180, IRPS5401)",
        .bus = ZCU104_I2C_BUS,
        .pmbus_addr = PMIC2_PMBUS_ADDR,
        .route = pmic_route,
        .num_hops = 1,
        .pages = pmic2_pages,
        .num_pages = 5,
        .vadj_page = PMIC2_PAGE_VADJ_FMC,
        .vadj_valid_mv = vadj_valid_mv,
        .vadj_mv_count = 3
    }
};

int board_init(void)
{
    return fpga_i2c_init();
}

const char *board_name(void)
{
    return "ZCU104";
}

int board_num_pmics(void)
{
    return (int)(sizeof(pmics) / sizeof(pmics[0]));
}

const pmic_desc_t *board_pmic(int idx)
{
    if (idx < 0 || idx >= board_num_pmics()) {
        return 0;
    }

    return &pmics[idx];
}

const pmic_desc_t *board_vadj_pmic(void)
{
    int i;

    for (i = 0; i < board_num_pmics(); i++) {
        if (pmics[i].vadj_page >= 0) {
            return &pmics[i];
        }
    }

    return 0;
}

void fpga_msleep(unsigned int ms)
{
    usleep((unsigned long)ms * 1000UL);
}
