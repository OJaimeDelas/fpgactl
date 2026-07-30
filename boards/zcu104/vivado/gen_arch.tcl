# fpgactl architecture generation for the ZCU104 (batch mode).
#
# Builds the PS-only design (single zynq_ultra_ps_e, empty PL) and exports
# the XSA hardware platform.
#
# tclargs: <out_xsa> <part> <fast 0|1> <ps_config_tcl>
#   out_xsa       absolute path of the XSA to write
#   part          FPGA part (xczu7ev-ffvc1156-2-e)
#   fast          1 = skip implementation, export a pre-synth XSA (no bitstream)
#   ps_config_tcl absolute path of the PS property script to apply

if {$argc < 4} {
    puts "ERROR: gen_arch.tcl needs <out_xsa> <part> <fast> <ps_config_tcl>"
    exit 1
}

set out_xsa   [lindex $argv 0]
set part      [lindex $argv 1]
set fast      [lindex $argv 2]
set ps_config [lindex $argv 3]

set proj_dir [file join [file dirname $out_xsa] vivado_proj]

file delete -force $proj_dir
create_project arch $proj_dir -part $part -force

create_bd_design "system"
create_bd_cell -type ip -vlnv xilinx.com:ip:zynq_ultra_ps_e zynq_ultra_ps_e_0

source $ps_config

validate_bd_design
save_bd_design

set bd_file [get_files system.bd]
make_wrapper -files $bd_file -top
set wrapper [file join $proj_dir arch.gen sources_1 bd system hdl system_wrapper.v]
if {![file exists $wrapper]} {
    # project layout without .gen split (older flows)
    set wrapper [file join $proj_dir arch.srcs sources_1 bd system hdl system_wrapper.v]
}
add_files -norecurse $wrapper
set_property top system_wrapper [current_fileset]
update_compile_order -fileset sources_1

generate_target all $bd_file

if {$fast == 1} {
    puts "ARCH_FAST=1: exporting pre-synthesis platform (no bitstream)"
    write_hw_platform -fixed -force $out_xsa
} else {
    launch_runs impl_1 -to_step write_bitstream -jobs 4
    wait_on_run impl_1
    if {[get_property PROGRESS [get_runs impl_1]] != "100%"} {
        puts "ERROR: implementation failed"
        exit 1
    }
    write_hw_platform -fixed -include_bit -force $out_xsa
}

close_project

if {![file exists $out_xsa]} {
    puts "ERROR: XSA was not written"
    exit 1
}

puts "Architecture exported: $out_xsa"
exit 0
