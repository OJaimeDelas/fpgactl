# CLI reference

Complete list of user-facing interfaces: make targets, make variables,
environment variables, and the board.mk variable contract.

## Make targets

| Target | Effect |
|---|---|
| `all` (default) | = `run` |
| `arch` | Build architecture, export XSA to `$(XSA)` (skipped if the file exists) |
| `sw` | Stage sources and compile the firmware ELF against `$(XSA)` |
| `run` | arch + sw + program via JTAG + capture UART to `$(OUTPUT)`; exit code from the firmware `STATUS:` line |
| `functions` | List available functions for `$(BOARD)`, marking board overrides |
| `board-list` | List boards (directories under `boards/` with a `board.mk`) |
| `info` | Print the resolved variables for `$(BOARD)` |
| `clean` | Remove `build/$(BOARD)` (`CLEAN_ALL=1`: remove all of `build/`) |

## Make variables

| Variable | Default | Meaning |
|---|---|---|
| `BOARD` | `zcu104` | Board name = directory under `boards/` |
| `FUNCTION` | — | Function to run (exclusive with `SCRIPT`; validated against the available list) |
| `SCRIPT` | `scripts/default_script.c` | Path (may be outside the repo) to a C file defining `int fpga_script(void)` |
| `OPTS` | — | Space-separated `NAME=value` config-macro overrides, e.g. `OPTS="CONFIG_VADJ_MV=1500"` |
| `XSA` | `build/$(BOARD)/arch/system_wrapper.xsa` | Architecture file: read from here if it exists, generated here if not |
| `FORCE_ARCH` | — | `1` = regenerate the XSA even if present |
| `FORCE_SW` | — | `1` = rebuild the firmware even if the ELF is up to date |
| `FULL_RUN` | — | `1` = shorthand for `FORCE_ARCH=1 FORCE_SW=1` (rebuild both steps) |
| `SKIP_ARCH` | — | `1` = never run Vivado; error if `$(XSA)` is missing |
| `SKIP_SW` | — | `1` = never run Vitis; reuse the existing ELF; error if missing |
| `ARCH_FAST` | `0` | `1` = export a pre-synthesis XSA without bitstream (faster arch step) |
| `PROGRAM_BIT` | `0` | `1` = extract the bitstream from the XSA and program it before running the ELF |
| `OUTPUT` | `build/$(BOARD)/fpga_config_output.txt` | UART capture destination |
| `TIMEOUT` | `120` | Seconds the capture waits for the firmware terminator line |
| `RUN_WRAPPER` | (empty) | Command prefix wrapping the JTAG run (e.g. a board-lock client) |
| `USE_NIX` | `1` | `1` = run host python steps inside `nix-shell`; `0` = run them directly |
| `CLEAN_ALL` | — | `1` = `make clean` removes all boards' build dirs |

Sticky per-user values: put them in the `local.mk` (auto-included)
or export them in the shell.

## Environment variables

| Variable | Meaning |
|---|---|
| `<PREFIX>_SERVER`, `<PREFIX>_USER` | Remote board host/login; the prefix is set per board in its `board.mk` (zcu104: `ZCU104_SERVER`/`ZCU104_USER`). Unset = board is local |
| `VIVADOPATH`, `VITISPATH` | Vivado/Vitis install paths; may name a version (`.../Vivado/2024.1`) or the versionless root (`.../Vivado/`, highest installed version is used); unset = use PATH |
| `VIVADO_SERVER`, `VIVADO_USER` | Run the arch step (Vivado) on this remote host; unset = local. `vivado` must be on the remote PATH |
| `VITIS_SERVER`, `VITIS_USER` | Run the sw step (Vitis) on this remote host; unset = local. `vitis` must be on the remote PATH |
| `BOARD_SSH_FLAGS`, `BOARD_SYNC_FLAGS` | Extra flags for `ssh`/`scp` / `rsync` in the remote board branch |
| `VIVADO_SSH_FLAGS`, `VIVADO_SYNC_FLAGS`, `VITIS_SSH_FLAGS`, `VITIS_SYNC_FLAGS` | Extra flags for `ssh`/`scp` / `rsync` in the remote tool branches |

## board.mk variables (board-author contract; definitions only, no rules)

| Variable | Required | Meaning |
|---|---|---|
| `BOARD_SERVER`, `BOARD_USER` | yes | Wire to `$(<PREFIX>_SERVER)` / `$(<PREFIX>_USER)` |
| `BOARD_SERIAL_PORT`, `BOARD_BAUD` | yes | Console serial port on the board host, baud rate |
| `BOARD_PART` | yes | FPGA part number (passed to the arch step) |
| `BOARD_PROC`, `BOARD_UART_STDIO` | yes | Target processor and BSP stdio UART instance |
| `BOARD_HW_SERVER` | no (`localhost:3121`) | hw_server URL for the JTAG connection |
| `BOARD_JTAG_CABLE_FILTER` | no (empty) | Cable name filter for multi-cable hosts |
