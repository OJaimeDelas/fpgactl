# fpgactl

Configure FPGA boards from the command line: read PMIC telemetry, set up and enable VADJ, and run user-defined configuration routines. Bare-metal firmware is built per board from a common function library; results are captured from the board's UART into `fpga_config_output.txt`.

## Requirements

- `make` and Nix. Every `make` target re-runs itself inside `nix-shell default.nix`, which provides python3, rsync, ssh and unzip (downloaded on the first run). To use system packages instead, install `make python3 rsync unzip openssh-client` and set `USE_NIX=0` (in `local.mk` or on the command line).
- Vivado and Vitis on the system (not through nix). Point `VIVADOPATH`/`VITISPATH` at the installs — either a specific version or the versionless root (the highest installed version is picked automatically):

  ```make
  VIVADOPATH = /opt/Xilinx/Vivado/ 
  VITISPATH  = /opt/Xilinx/Vitis/
  ```

  These (and every other user-site variable) can live in a `local.mk` at the repo root (auto-included), in your bashrc, or be exported in the shell.

To run a tool step on another machine instead, set `VIVADO_SERVER`/`VIVADO_USER` (arch step) and/or `VITIS_SERVER`/`VITIS_USER` (sw step): the inputs are rsync'd over, the tool runs there (`VIVADOPATH`/`VITISPATH` are resolved on the remote host, or its PATH if unset), and the resulting XSA/ELF is copied back. Unset them to run locally.

## Usage

```sh
make run BOARD=zcu104 FUNCTION=read_pmic          # run one function
make run BOARD=zcu104 SCRIPT=my_bringup.c         # run a user C script
make run BOARD=zcu104                             # run scripts/default_script.c
make run BOARD=zcu104 FUNCTION=config_vadj OPTS="CONFIG_VADJ_MV=1500"  # override config values
```

Each run has two build steps:

1. **arch** — Vivado builds the board architecture and exports the XSA. `XSA=<path>` names the file for both reading and writing (default `build/<board>/arch/system_wrapper.xsa`): if it exists the step is skipped, if not it is generated there. `FORCE_ARCH=1` regenerates; `SKIP_ARCH=1` forbids Vivado (errors if the XSA is missing).
2. **sw** — Vitis compiles the firmware (common code + board code + selected function/script) against the XSA. `SKIP_SW=1` reuses the existing ELF; `FORCE_SW=1` rebuilds it unconditionally.

`FULL_RUN=1` forces both steps (equivalent to `FORCE_ARCH=1 FORCE_SW=1`).

Then the ELF is loaded over JTAG, and the UART output is captured to `build/<board>/fpga_config_output.txt` (override with `OUTPUT=`). The run exits with the firmware's `STATUS: OK|FAIL` result.

Everything generated lives under `build/` by default. `BUILD_FOLDER=<path>` relocates all of it; `XSA=`/`ELF=`/`OUTPUT=` relocate individual artifacts. In all cases artifacts found at the given path are reused, missing ones are built there.

Sticky per-user values (`XSA=`, `BOARD=`, `OUTPUT=`, tool paths…) can be placed in the `local.mk`, auto-included by the Makefile.

Individual steps: `make arch`, `make sw`, `make run`. Inspection: `make board-list`, `make functions BOARD=<b>`, `make info BOARD=<b>`. Cleanup: `make clean`. The complete list of targets and variables is in `docs/CLI.md`; the available functions and their options are documented in `functions.md`.

### Remote boards

If the board is attached to another machine, export before running:

```sh
export ZCU104_SERVER=<host> ZCU104_USER=<user>    # env prefix defined per board in board.mk
```

The run bundle (ELF + JTAG script + capture script) is rsync'd to the host, executed there, and `fpga_config_output.txt` is copied back. The remote host needs only `xsct` on PATH, python3, the JTAG cable and the serial port. If no `hw_server` is running on the board host, `run_jtag.tcl` starts one (only for the default local URL `localhost:3121`; a `BOARD_HW_SERVER` on another host must already be running).

Vivado and/or Vitis can also run on remote machines: set `VIVADO_SERVER`/`VIVADO_USER` and/or `VITIS_SERVER`/`VITIS_USER` (in `local.mk`, bashrc, or exported).

## Writing scripts

A script is a C file (any path) defining `int fpga_script(void)`. It may call any function (declared in `common/src/functions.h`) and the `pmbus_*`/`fpga_i2c_*` primitives. Return 0 on success. Scripts are board-independent.

To configure a function before running it, use its `_cfg`/`_run` form:

```c
#include "functions.h"

int fpga_script(void) {
    config_vadj_cfg_t c;
    config_vadj_default_cfg(&c);   /* board defaults from cfg_config_vadj.h */
    c.mv = 1800;
    c.also_enable = 1;
    if (config_vadj_run(&c) != 0) return 1;
    return read_pmic();            /* plain call = defaults */
}
```

## Integrating into an existing workflow

`make run` exits 0 only if the firmware reports `STATUS: OK`, so it works as a gate step (e.g. enable VADJ before programming an FMC design):

```make
vadj-setup:
	$(MAKE) -C path/to/fpgactl run BOARD=zcu104 \
	        SCRIPT=$(abspath vadj_1v8.c) OUTPUT=$(abspath build/vadj.log)
```

First invocation builds the architecture and firmware; later invocations reuse the caches and reduce to JTAG load + UART capture. Parse `RESULT: key=value` lines from the output file if the outer flow needs measured values. Note: the run resets the PS over JTAG — order it before programming/starting your main design (PMIC state survives, board power cycles do not).

## Adding a function

1. Create `common/functions/<name>.c` defining the three-part contract from `functions.h`: `<name>_cfg_t`, `<name>_default_cfg()`, `<name>_run()`, and the CLI entry `int <name>(void)`. Print via `fpga_printf`; emit `RESULT: key=value` lines for measurements and return 0 on success. Use only the virtual interfaces (`fpga_i2c.h`, `fpga_console.h`, `board.h`) — never board or Xilinx headers.
2. Add its prototypes to `common/src/functions.h`.
3. Create `common/config/cfg_<name>.h` with the function's default options, each wrapped in `#ifndef` guards and preceded by `#include "fpga_opts.h"` (this is what makes `OPTS=` overrides work).
4. Optional: a board overrides values in `boards/<board>/config/cfg_<name>.h`, or the whole implementation in `boards/<board>/functions/<name>.c` (same filename, same signature; board file wins).

`make run FUNCTION=<name>` works immediately on every board.

## Adding a board

Create `boards/<name>/` with:

| File | Content |
|---|---|
| `board.mk` | Variable definitions only: `BOARD_SERVER=$(<PREFIX>_SERVER)`, `BOARD_USER=$(<PREFIX>_USER)`, `BOARD_SERIAL_PORT`, `BOARD_BAUD`, `BOARD_PART`, `BOARD_PROC`, `BOARD_UART_STDIO`, `BOARD_HW_SERVER` |
| `fw/board_io.h` | The board's IO constants: PMIC addresses, mux addresses/channels, page numbers |
| `fw/board_<name>.c` | The PMIC descriptor table (address, I2C route, page list, VADJ page and valid voltages per PMIC) + `board.h` accessors + `fpga_msleep` |
| `fw/*.c` | Implementations of `fpga_i2c.h` (I2C transport) and `fpga_console.h` (UART output) |
| `fw/lscript.ld` | Linker script, if the board needs specific memory placement |
| `config/cfg_*.h` | Option overrides for functions whose defaults don't fit the board |
| `vivado/gen_arch.tcl` (+ `ps_config.tcl`/constraints) | Batch script that builds the hardware design and exports the XSA (plus XDC/IP if I2C/UART go through PL pins) |
| `vitis/build_sw.py`, `vitis/run_jtag.tcl` | Vitis platform+app build against the XSA; JTAG load-and-run script |

Nothing in `common/`, `scripts/` or the top Makefile changes. Common behavior to reuse:

- Reaching a device through I2C muxes is data, not code: list the hops in the descriptor's route (e.g. zcu104 PMIC2 = `{0x74, 0x04}`).
- Adding another PMIC to an existing board = one more descriptor entry in the board's descriptor table.
