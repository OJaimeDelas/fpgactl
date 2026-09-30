# fpgactl JTAG run script for ZynqMP (run with xsct).
#
# Replaces the FSBL in the JTAG flow: system reset, psu_init (clocks, MIO,
# DDR), then download and start the ELF on A53 core 0. Programs a bitstream
# first only if one is given (the ZCU104 design has an empty PL, so none
# is needed).
#
# argv: <elf> <psu_init_tcl> <hw_server_url> [cable_filter] [bitstream]

if {$argc < 3} {
    puts "ERROR: run_jtag.tcl needs <elf> <psu_init_tcl> <hw_server_url> \[cable_filter\] \[bitstream\]"
    exit 1
}

set elf     [lindex $argv 0]
set psu     [lindex $argv 1]
set url     [lindex $argv 2]
set cable   [lindex $argv 3]
set bit     [lindex $argv 4]

if {![file exists $elf]} { puts "ERROR: ELF not found: $elf"; exit 1 }
if {![file exists $psu]} { puts "ERROR: psu_init.tcl not found: $psu"; exit 1 }

# xsct requires the transport prefix in the URL (tcp:host:port)
if {[string first "tcp:" $url] != 0} {
    set url "tcp:$url"
}

# Use a running hw_server if there is one. Otherwise, when the URL is the
# default local one, start it: this script always runs on the machine the
# board is attached to (locally, or on BOARD_SERVER over ssh), and a bare
# 'connect' launches a local hw_server on port 3121 if none is running.
puts "Connecting to hw_server at $url"
if {[catch {connect -url $url} err]} {
    set is_local [regexp {^tcp:(localhost|127\.0\.0\.1)?(:3121)?$} $url]
    if {!$is_local} {
        puts "ERROR: could not connect to hw_server at $url: $err"
        puts "       (remote hw_server is not auto-started; start it on that host)"
        exit 1
    }
    puts "No hw_server running; starting one locally"
    if {[catch {connect} err2]} {
        puts "ERROR: could not start/connect to a local hw_server: $err2"
        exit 1
    }
}

# Select the JTAG cable if a filter was given (multi-cable hosts)
if {$cable ne ""} {
    if {[catch {jtag targets -set -filter [subst -nocommands {name =~ "$cable"}]} err]} {
        puts "ERROR: no JTAG cable matching '$cable': $err"
        exit 1
    }
}

# Whole-PS reset (like every Vitis 'Run')
targets -set -nocase -filter {name =~ "*PSU*"}
rst -system
after 1000

# Optional bitstream (empty PL designs need none)
if {$bit ne "" && [file exists $bit]} {
    puts "Programming bitstream $bit"
    fpga -file $bit
}

# PS initialization: clocks, MIO muxing (I2C1/UART1), DDR controller
targets -set -nocase -filter {name =~ "*PSU*"}
source $psu
psu_init
after 500
if {[info procs psu_ps_pl_isolation_removal] ne ""} { psu_ps_pl_isolation_removal }
if {[info procs psu_ps_pl_reset_config] ne ""}      { psu_ps_pl_reset_config }
catch {psu_post_config}

# Download and run on A53 core 0
targets -set -nocase -filter {name =~ "*A53*#0"}
rst -processor -clear-registers
dow $elf
con

puts "ELF running; console output on the board UART"
disconnect
exit 0
