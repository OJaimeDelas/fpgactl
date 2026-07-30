#ifndef FPGA_CONSOLE_H
#define FPGA_CONSOLE_H

/*
 * Virtual console interface (target half of the UART virtualization).
 *
 * Board-agnostic code prints only through fpga_printf; each board provides
 * the implementation in boards/<board>/fw/ (which UART, which print
 * backend). The host half is scripts/console_capture.py, which captures
 * the board's serial output into fpga_config_output.txt.
 */

int fpga_console_init(void);

#if defined(__GNUC__)
void fpga_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
#else
void fpga_printf(const char *fmt, ...);
#endif

#endif /* FPGA_CONSOLE_H */
