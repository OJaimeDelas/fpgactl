# ZCU104 board definitions. Variable definitions ONLY - no rules here.
# Remote board host comes from the user's environment (unset = board is local).

BOARD_SERVER=$(ZCU104_SERVER)
BOARD_USER=$(ZCU104_USER)
BOARD_SERIAL_PORT?=/dev/serial/by-id/usb-Xilinx_JTAG+3Serial_87957-if02-port0
BOARD_BAUD?=115200
BOARD_PART=xczu7ev-ffvc1156-2-e
BOARD_PROC=psu_cortexa53_0
BOARD_UART_STDIO=psu_uart_1
BOARD_HW_SERVER?=localhost:3121
BOARD_JTAG_CABLE_FILTER?=
