/*
 * ZCU104 implementation of the fpga_i2c virtual transport: Xilinx XIicPs
 * polled driver on the PS I2C1 controller (MIO16 = SCL, MIO17 = SDA).
 * The board has a single root bus, so the bus parameter is ignored.
 */

#include "fpga_i2c.h"

#include "board_io.h"
#include "xparameters.h"
#include "xil_printf.h"
#include "xstatus.h"
#include "xiicps.h"

/*
 * The first enabled XIicPs instance in the hardware platform.
 * On this design that is the PS I2C1 controller at 0xFF030000.
 */
#ifndef SDT
#define IIC_DEVICE_ID XPAR_XIICPS_0_DEVICE_ID
#else
#define IIC_BASEADDR XPAR_XIICPS_0_BASEADDR
#endif

static XIicPs Iic;

static int wait_bus_free(void)
{
    int timeout = 1000000;

    while (XIicPs_BusIsBusy(&Iic) && timeout > 0) {
        timeout--;
    }

    if (timeout == 0) {
        return XST_FAILURE;
    }

    return XST_SUCCESS;
}

int fpga_i2c_init(void)
{
    XIicPs_Config *cfg;
    int status;

#ifndef SDT
    cfg = XIicPs_LookupConfig(IIC_DEVICE_ID);
#else
    cfg = XIicPs_LookupConfig(IIC_BASEADDR);
#endif

    if (cfg == 0) {
        xil_printf("ERROR: XIicPs_LookupConfig failed\r\n");
        return XST_FAILURE;
    }

    status = XIicPs_CfgInitialize(&Iic, cfg, cfg->BaseAddress);
    if (status != XST_SUCCESS) {
        xil_printf("ERROR: XIicPs_CfgInitialize failed\r\n");
        return status;
    }

    status = XIicPs_SetSClk(&Iic, ZCU104_I2C_SCLK_HZ);
    if (status != XST_SUCCESS) {
        xil_printf("ERROR: XIicPs_SetSClk failed\r\n");
        return status;
    }

    return XST_SUCCESS;
}

int fpga_i2c_write(u8 bus, u8 slave_addr, const u8 *data, int len)
{
    int status;

    (void)bus;  /* single root bus on this board */

    status = wait_bus_free();
    if (status != XST_SUCCESS) {
        return status;
    }

    status = XIicPs_MasterSendPolled(&Iic, (u8 *)data, len, slave_addr);
    if (status != XST_SUCCESS) {
        return status;
    }

    return wait_bus_free();
}

int fpga_i2c_read_cmd(u8 bus, u8 slave_addr, u8 cmd, u8 *data, int len)
{
    int status;

    (void)bus;  /* single root bus on this board */

    status = wait_bus_free();
    if (status != XST_SUCCESS) {
        return status;
    }

    status = XIicPs_SetOptions(&Iic, XIICPS_REP_START_OPTION);
    if (status != XST_SUCCESS) {
        return status;
    }

    status = XIicPs_MasterSendPolled(&Iic, &cmd, 1, slave_addr);
    if (status != XST_SUCCESS) {
        XIicPs_ClearOptions(&Iic, XIICPS_REP_START_OPTION);
        return status;
    }

    /*
     * The send phase is held open by the repeated-start option.
     * Clearing the option here lets the receive phase finish with STOP.
     */
    XIicPs_ClearOptions(&Iic, XIICPS_REP_START_OPTION);

    status = XIicPs_MasterRecvPolled(&Iic, data, len, slave_addr);
    if (status != XST_SUCCESS) {
        return status;
    }

    return wait_bus_free();
}
