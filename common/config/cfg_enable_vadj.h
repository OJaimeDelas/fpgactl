#ifndef CFG_ENABLE_VADJ_H
#define CFG_ENABLE_VADJ_H

#include "fpga_opts.h"  /* generated: OPTS= command-line overrides win */

/* 1: turn the VADJ output ON (OPERATION = 0x80); 0: turn it OFF (0x00). */
#ifndef ENABLE_DISABLE
#define ENABLE_DISABLE      1
#endif

/* 1: read back and print the VADJ page status after switching the output. */
#ifndef ENABLE_VADJ_VERIFY
#define ENABLE_VADJ_VERIFY  1
#endif

#endif /* CFG_ENABLE_VADJ_H */
