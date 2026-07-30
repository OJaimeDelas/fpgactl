# ZCU104 PS configuration, applied to a freshly instantiated zynq_ultra_ps_e
# bd cell: I2C1 on MIO16/17, UART1 on MIO20/21 @ 115200, DDR low region only,
# no PS-PL AXI interfaces, no other peripherals.

set_property -dict [list \
    CONFIG.PSU__I2C1__PERIPHERAL__ENABLE {1} \
    CONFIG.PSU__I2C1__PERIPHERAL__IO {MIO 16 .. 17} \
    CONFIG.PSU__UART1__PERIPHERAL__ENABLE {1} \
    CONFIG.PSU__UART1__PERIPHERAL__IO {MIO 20 .. 21} \
    CONFIG.PSU__UART1__BAUD_RATE {115200} \
    CONFIG.PSU__UART1__MODEM__ENABLE {0} \
    CONFIG.PSU__USE__M_AXI_GP2 {0} \
    CONFIG.PSU__DDR_HIGH_ADDRESS_GUI_ENABLE {0} \
] [get_bd_cells zynq_ultra_ps_e_0]
