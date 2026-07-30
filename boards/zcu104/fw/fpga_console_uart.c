/*
 * ZCU104 implementation of the fpga_console virtual interface.
 *
 * The BSP routes stdout to PS UART1 (psu_uart_1, MIO20/21, 115200 8N1);
 * xil_printf writes there. fpga_printf pre-formats with vsnprintf so that
 * board-agnostic code gets full printf semantics (field widths, left
 * justification) regardless of xil_printf's limitations.
 */

#include "fpga_console.h"

#include <stdarg.h>
#include <stdio.h>

#include "xil_printf.h"

int fpga_console_init(void)
{
    /* Nothing to do: the BSP configures psu_uart_1 as stdout. */
    return 0;
}

void fpga_printf(const char *fmt, ...)
{
    static char buf[512];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    xil_printf("%s", buf);
}
