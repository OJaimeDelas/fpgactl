#ifndef FUNCTIONS_H
#define FUNCTIONS_H

/*
 * Standard function library.
 *
 * Every function <name> follows a three-part contract:
 *   <name>_cfg_t          - one field per option
 *   <name>_default_cfg()  - fills the struct from cfg_<name>.h macros
 *   <name>_run(&cfg)      - the actual implementation
 *   <name>()              - CLI entry (FUNCTION=<name>): default cfg + run
 *
 * User scripts (SCRIPT=file.c defining int fpga_script(void)) may call the
 * void forms for defaults or the _cfg/_run forms for per-call control.
 * All functions print through fpga_printf and return 0 on success.
 */

/* ---- read_pmic: dump PMBus telemetry of the board's PMIC(s) ---- */
typedef struct {
    int target;   /* index into the board PMIC table, or -1 for all PMICs */
} read_pmic_cfg_t;

void read_pmic_default_cfg(read_pmic_cfg_t *c);
int  read_pmic_run(const read_pmic_cfg_t *c);
int  read_pmic(void);

/* ---- enable_vadj: switch the VADJ output on or off ---- */
typedef struct {
    int enable;   /* 1 = turn output ON (OPERATION 0x80), 0 = OFF (0x00) */
    int verify;   /* 1 = read back and print the VADJ page status after */
} enable_vadj_cfg_t;

void enable_vadj_default_cfg(enable_vadj_cfg_t *c);
int  enable_vadj_run(const enable_vadj_cfg_t *c);
int  enable_vadj(void);

/* ---- config_vadj: set the VADJ output voltage ---- */
typedef struct {
    unsigned int mv;      /* target voltage in millivolts (board validates) */
    int dump_all;         /* 1 = full PMIC dump before and after the change */
    int also_enable;      /* 1 = turn the output on after writing the voltage */
    int force_vout_mode;  /* 1 = write VOUT_MODE 0x18 before the voltage */
} config_vadj_cfg_t;

void config_vadj_default_cfg(config_vadj_cfg_t *c);
int  config_vadj_run(const config_vadj_cfg_t *c);
int  config_vadj(void);

/* ---- clear_faults: send PMBus CLEAR_FAULTS ---- */
typedef struct {
    int all_pages;  /* 1 = clear every page; 0 = only the VADJ page */
    int verify;     /* 1 = print STATUS_WORD before and after each clear */
} clear_faults_cfg_t;

void clear_faults_default_cfg(clear_faults_cfg_t *c);
int  clear_faults_run(const clear_faults_cfg_t *c);
int  clear_faults(void);

/* User-script entry point (SCRIPT mode). */
int fpga_script(void);

#endif /* FUNCTIONS_H */
