#ifndef CFG_CLEAR_FAULTS_H
#define CFG_CLEAR_FAULTS_H

#include "fpga_opts.h"  /* generated: OPTS= command-line overrides win */

/* 1: clear the faults of every PMIC page; 0: clear only the VADJ page. */
#ifndef CLEAR_FAULTS_ALL_PAGES
#define CLEAR_FAULTS_ALL_PAGES  0
#endif

/* 1: read and print STATUS_WORD before and after clearing each page. */
#ifndef CLEAR_FAULTS_VERIFY
#define CLEAR_FAULTS_VERIFY     1
#endif

#endif /* CFG_CLEAR_FAULTS_H */
