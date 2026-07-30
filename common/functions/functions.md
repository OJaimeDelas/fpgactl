# Functions

Run with `make run FUNCTION=<name>`; override options with `OPTS="NAME=value ..."`
(defaults live in `common/config/cfg_<name>.h`, board overrides in
`boards/<board>/config/`).

## read_pmic

Reads and prints the full PMBus telemetry (all pages) of the board's PMIC(s).

| Option | Meaning | Values |
|---|---|---|
| `PMIC_READ_TARGET` | Which PMIC of the board table to dump | index (`0`..N-1), `-1` = all *(default)* |

## enable_vadj

Switches the VADJ rail output on or off via the PMBus OPERATION command.

| Option | Meaning | Values |
|---|---|---|
| `ENABLE_DISABLE` | Turn the output ON or OFF | `1` = ON *(default)*, `0` = OFF |
| `ENABLE_VADJ_VERIFY` | Read back and print the VADJ page status after switching | `1` = yes *(default)*, `0` = no |

## config_vadj

Sets the VADJ rail voltage (VOUT_COMMAND) with readback verification.

| Option | Meaning | Values |
|---|---|---|
| `CONFIG_VADJ_MV` | Target voltage in millivolts | board-validated; zcu104: `1200`, `1500`, `1800` *(default)* |
| `CONFIG_VADJ_DUMP_ALL_PAGES` | Full PMIC dump before and after the change | `1` = yes *(default)*, `0` = no |
| `CONFIG_VADJ_ALSO_ENABLE` | Turn the output on after writing the voltage | `1` = yes, `0` = no *(default)* |
| `CONFIG_FORCE_VOUT_MODE_0X18` | Write VOUT_MODE 0x18 (Linear16, exp -8) before the voltage | `1` = yes, `0` = no *(default)* |

## clear_faults

Sends PMBus CLEAR_FAULTS to reset latched fault/status bits.

| Option | Meaning | Values |
|---|---|---|
| `CLEAR_FAULTS_ALL_PAGES` | Clear every page instead of only the VADJ page | `1` = all, `0` = VADJ only *(default)* |
| `CLEAR_FAULTS_VERIFY` | Print STATUS_WORD before and after each clear | `1` = yes *(default)*, `0` = no |
