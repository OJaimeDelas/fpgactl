#ifndef CFG_CONFIG_VADJ_H
#define CFG_CONFIG_VADJ_H

#include "fpga_opts.h"  /* generated: OPTS= command-line overrides win */

/* Target VADJ voltage in millivolts; the board validates allowed values. */
#ifndef CONFIG_VADJ_MV
#define CONFIG_VADJ_MV              1800
#endif

/* 1: print the full PMIC dump before and after the voltage change. */
#ifndef CONFIG_VADJ_DUMP_ALL_PAGES
#define CONFIG_VADJ_DUMP_ALL_PAGES  1
#endif

/* 1: if the VADJ output is off, turn it on after writing the new voltage. */
#ifndef CONFIG_VADJ_ALSO_ENABLE
#define CONFIG_VADJ_ALSO_ENABLE     0
#endif

/* 1: write VOUT_MODE = 0x18 (Linear16, exponent -8) before the voltage. */
#ifndef CONFIG_FORCE_VOUT_MODE_0X18
#define CONFIG_FORCE_VOUT_MODE_0X18 0
#endif

#endif /* CFG_CONFIG_VADJ_H */
