#include "board.h"
#include "fpga_console.h"
#include "run_selection.h"

int main(void)
{
    int status;

    fpga_console_init();

    fpga_printf("\r\n\r\n");
    fpga_printf("FPGA-Configurator | board: %s | entry: %s\r\n",
                board_name(), FPGA_ENTRY_NAME);

    status = board_init();
    if (status != FPGA_OK) {
        fpga_printf("FATAL: board init failed\r\n");
    } else {
        status = FPGA_ENTRY();
    }

    fpga_printf("\r\nSTATUS: %s\r\n", (status == FPGA_OK) ? "OK" : "FAIL");
    fpga_printf("Application finished\r\n");

    while (1) {
        /* idle; the run is over once "Application finished" is printed */
    }

    return status;
}
