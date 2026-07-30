# ZCU104 board notes

## VADJ starts OFF in the JTAG flow

The JTAG run replaces the FSBL with `psu_init.tcl`, which programs clocks,
MIO muxing and the DDR controller but **not** the FSBL's board-specific
PMBus write that enables VADJ_FMC. So after every power cycle VADJ_FMC is
OFF — this is expected, not a failure, and is exactly why
`FUNCTION=enable_vadj` exists. A JTAG re-run does not power-cycle the
PMICs, so an enabled/configured VADJ survives re-runs (but not power
cycles).

Typical disabled state on page 3: `OPERATION=0x00`, `ON_OFF_CONFIG=0x1A`,
`STATUS_WORD=0x0841`, `READ_VOUT` near 0 V. DS8 ON indicates VADJ power
good — but always verify with a multimeter before connecting FMC hardware.

## This particular board's DDR4 is faulty

Address bit 14 is dead (locations 16 KB apart alias to the same cells) and
the low 16 data bits corrupt sporadically. The firmware is therefore linked
entirely into the 256 KB OCM (`fw/lscript.ld`, ORIGIN 0xFFFC0000) and must
stay under 256 KB. Anything needing DDR (Linux, U-Boot, large apps) will
not run until the SODIMM is reseated/replaced.

## Serial ports

The FT4232H exposes four channels; the PS UART1 console is channel C:
`/dev/serial/by-id/usb-Xilinx_JTAG+3Serial_87957-if02-port0` (usually
`ttyUSB2`). 115200 8N1, no flow control. A stale `screen` session holding
the port makes the capture fail with "Device or resource busy" — close it
with `screen -X -S <pid> quit`.

## PMBus topology

PS I2C1 (MIO16 SCL / MIO17 SDA) → U34 TCA9548A mux @0x74 (channel 2 =
control byte 0x04) → PMIC2 U180 IRPS5401 @0x44. Pages: 0 SW-A UTIL_3V3,
1 SW-B UTIL_1V13, 2 SW-C UTIL_5V0, **3 SW-D VADJ_FMC**, 4 LDO MGTRAVCC.
Valid VADJ targets: 1200/1500/1800 mV. VADJ changes are live-only (no NVM
write), which is intentional.
