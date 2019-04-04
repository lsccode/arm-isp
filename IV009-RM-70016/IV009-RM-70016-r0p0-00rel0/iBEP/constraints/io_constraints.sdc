# -----------------------------------------------------------------------------
# The confidential and proprietary information contained in this file may
# only be used by a person authorised under and to the extent permitted
# by a subsisting licensing agreement from Arm Limited or its affiliates.
#
# (C) COPYRIGHT 2005-2018 Arm Limited or its affiliates.
# ALL RIGHTS RESERVED
#
# This entire notice must be reproduced on all copies of this file
# and copies of this file may only be made by a person if such person is
# permitted to do so under the terms of a subsisting license agreement
# from Arm Limited or its affiliates.
#-----------------------------------------------------------------------------
#  Version and Release Control Information:
#
#  File Name		: io_constraints.sdc
#  Release Package	: iv009_r0p0_00rel0
#-----------------------------------------------------------------------------

# -----------------------------------------------------------------------------
# Define $cycle percentage expressions
# -----------------------------------------------------------------------------

## Variables for this file come from the clocks.sdc file
## Temporary variables used during constraint generation e.g. $VCLKcycle20, $VCLKcycle50.

set virtual_io_clock_cycle_percentages [list ]
  
foreach clock_name ${clock_names_list} {
  eval "array set _clock $${clock_name}"
  if {[info exists _clock(virtual_io_clock_name)]} {
    set virtual_io_clock_name $_clock(virtual_io_clock_name)
  } elseif {![info exists _clock(port)] && ![info exists _clock(master_clock_name)]} {
    set virtual_io_clock_name $clock_name
  } else {
    continue
  }
  for {set i 0} {$i <= 100} {incr i 5} {
    set ${virtual_io_clock_name}cycle[format %#02d $i] [expr 0.01 * ${i} * $_clock(period)]
    lappend virtual_io_clock_cycle_percentages ${virtual_io_clock_name}cycle[format %#02d $i]
  }
  unset _clock
}

# -----------------------------------------------------------------------------
# Constraining the ports clocked by vclk
# -----------------------------------------------------------------------------
set_output_delay $v_vclkcycle40 -max -clock v_vclk [get_ports "interrupt"]
set_output_delay 0.0 -min -clock v_vclk [get_ports "interrupt"]

set_output_delay $v_vclkcycle40 -max -clock v_vclk [get_ports "isp_multictx_frame_req"]
set_output_delay 0.0 -min -clock v_vclk [get_ports "isp_multictx_frame_req"]

set_input_delay $v_vclkcycle60 -max -clock v_vclk [get_ports "h*" -filter {port_direction == in}]
set_input_delay 0.0 -min -clock v_vclk [get_ports "h*" -filter {port_direction == in}]

set_output_delay $v_vclkcycle40 -max -clock v_vclk [get_ports "h*" -filter {port_direction == out}]
set_output_delay 0.0 -min -clock v_vclk [get_ports "h*" -filter {port_direction == out}]

set_input_delay $v_vclkcycle60 -max -clock v_vclk [get_ports "v*" -filter {port_direction == in}]
set_input_delay 0.0 -min -clock v_vclk [get_ports "vd*" -filter {port_direction == in}]

set_input_delay $v_vclkcycle60 -max -clock v_vclk [get_ports "v*" -filter {port_direction == in}]
set_input_delay 0.0 -min -clock v_vclk [get_ports "vv*" -filter {port_direction == in}]

set_input_delay $v_vclkcycle50 -max -clock v_vclk [get_ports "vcke"]
set_input_delay 0.0 -min -clock v_vclk [get_ports "vcke"]

set_output_delay $v_vclkcycle40 -max -clock v_vclk [get_ports "fr_*out" -filter {port_direction == out}]
set_output_delay 0.0 -min -clock v_vclk [get_ports "fr_*out" -filter {port_direction == out}]

set_output_delay $v_vclkcycle40 -max -clock v_vclk [get_ports "ds_*out" -filter {port_direction == out}]
set_output_delay 0.0 -min -clock v_vclk [get_ports "ds_*out" -filter {port_direction == out}]

set_output_delay $v_vclkcycle40 -max -clock v_vclk [get_ports "ir*" -filter {port_direction == out}]
set_output_delay 0.0 -min -clock v_vclk [get_ports "ir*" -filter {port_direction == out}]

set_input_delay $v_vclkcycle60 -max -clock v_vclk [get_ports "max_addr_dly_line_ds*"]
set_input_delay 0.0 -min -clock v_vclk [get_ports "max_addr_dly_line_ds*"]

set_input_delay $v_vclkcycle60 -max -clock v_vclk [get_ports "max_addr_dly_line_fr*"]
set_input_delay 0.0 -min -clock v_vclk [get_ports "max_addr_dly_line_fr*"]

# -----------------------------------------------------------------------------
# Constraining the ports clocked by aclk
# -----------------------------------------------------------------------------

set_input_delay $v_aclkcycle60 -max -clock v_aclk [get_ports "fr_video_out*" -filter {port_direction == in}]
set_input_delay 0.0 -min -clock v_aclk [get_ports "fr_video_out*" -filter {port_direction == in}]

set_output_delay $v_aclkcycle40 -max -clock v_aclk [get_ports "fr_video_out*" -filter {port_direction == out}]
set_output_delay 0.0 -min -clock v_aclk [get_ports "fr_video_out*" -filter {port_direction == out}]

set_input_delay $v_aclkcycle60 -max -clock v_aclk [get_ports "ds_video_out*" -filter {port_direction == in}]
set_input_delay 0.0 -min -clock v_aclk [get_ports "ds_video_out*" -filter {port_direction == in}]

set_output_delay $v_aclkcycle40 -max -clock v_aclk [get_ports "ds_video_out*" -filter {port_direction == out}]
set_output_delay 0.0 -min -clock v_aclk [get_ports "ds_video_out*" -filter {port_direction == out}]

set_input_delay $v_aclkcycle60 -max -clock v_aclk [get_ports "temper*" -filter {port_direction == in}]
set_input_delay 0.0 -min -clock v_aclk [get_ports "temper*" -filter {port_direction == in}]

set_output_delay $v_aclkcycle40 -max -clock v_aclk [get_ports "temper*" -filter {port_direction == out}]
set_output_delay 0.0 -min -clock v_aclk [get_ports "temper*" -filter {port_direction == out}]

set_output_delay $v_aclkcycle40 -max -clock v_aclk [get_ports "fr_*dma*" -filter {port_direction == out}]
set_output_delay 0.0 -min -clock v_aclk [get_ports "fr_*dma*" -filter {port_direction == out}]

set_output_delay $v_aclkcycle40 -max -clock v_aclk [get_ports "ds_*dma*" -filter {port_direction == out}]
set_output_delay 0.0 -min -clock v_aclk [get_ports "ds_*dma*" -filter {port_direction == out}]

set_output_delay $v_aclkcycle40 -max -clock v_aclk [get_ports "monitor_*" -filter {port_direction == out}]
set_output_delay 0.0 -min -clock v_aclk [get_ports "monitor_*" -filter {port_direction == out}]

# -----------------------------------------------------------------------------
# Constrain DFT ports
# -----------------------------------------------------------------------------

set_multicycle_path 6   -setup -end -from [get_ports DFTSI*] -to [all_registers -clock vclk]
set_multicycle_path 5   -hold  -end -from [get_ports DFTSI*] -to [all_registers -clock vclk]

set_multicycle_path 6   -setup -end -to  [get_ports DFTSO*] -from [all_registers -clock vclk]
set_multicycle_path 5   -hold  -end -to  [get_ports DFTSO*] -from [all_registers -clock vclk]

set_multicycle_path 6   -setup -end -from [get_ports DFTSE] -to [all_registers -clock vclk]
set_multicycle_path 5   -hold  -end -from [get_ports DFTSE] -to [all_registers -clock vclk] 

set_multicycle_path 6   -setup -end -from [get_ports DFTCGEN] -to [all_registers -clock vclk]
set_multicycle_path 5   -hold  -end -from [get_ports DFTCGEN] -to [all_registers -clock vclk]

set_multicycle_path 6   -setup -end -from [get_ports DFTRAMBYP] -to [all_registers -clock vclk]
set_multicycle_path 5   -hold  -end -from [get_ports DFTRAMBYP] -to [all_registers -clock vclk]

set_multicycle_path 3   -setup -end -from [get_ports DFTSI*] -to [all_registers -clock aclk]
set_multicycle_path 2   -hold  -end -from [get_ports DFTSI*] -to [all_registers -clock aclk]

set_multicycle_path 3   -setup -end -to  [get_ports DFTSO*] -from [all_registers -clock aclk]
set_multicycle_path 2   -hold  -end -to  [get_ports DFTSO*] -from [all_registers -clock aclk]

set_multicycle_path 3   -setup -end -from [get_ports DFTSE] -to [all_registers -clock aclk]
set_multicycle_path 2   -hold  -end -from [get_ports DFTSE] -to [all_registers -clock aclk] 

set_multicycle_path 3   -setup -end -from [get_ports DFTCGEN] -to [all_registers -clock aclk]
set_multicycle_path 2   -hold  -end -from [get_ports DFTCGEN] -to [all_registers -clock aclk]

set_multicycle_path 3   -setup -end -from [get_ports DFTRAMBYP] -to [all_registers -clock aclk]
set_multicycle_path 2   -hold  -end -from [get_ports DFTRAMBYP] -to [all_registers -clock aclk]

set_input_delay  $v_vclkcycle50   -max -clock v_vclk [get_ports DFTSI*]
set_input_delay  0.0        -min -clock v_vclk [get_ports DFTSI*]

set_output_delay $v_vclkcycle50   -max -clock v_vclk [get_ports DFTSO*]
set_output_delay 0.0        -min -clock v_vclk [get_ports DFTSO*]

set_input_delay  $v_vclkcycle50   -max -clock v_vclk [get_ports DFTSE]
set_input_delay  0.0        -min -clock v_vclk [get_ports DFTSE]

set_input_delay  $v_vclkcycle50   -max -clock v_vclk [get_ports DFTCGEN]
set_input_delay  0.0        -min -clock v_vclk [get_ports DFTCGEN]

set_input_delay  $v_vclkcycle50   -max -clock v_vclk [get_ports DFTRAMBYP]
set_input_delay  0.0        -min -clock v_vclk [get_ports DFTRAMBYP]

set_input_delay  $v_aclkcycle50   -max -clock v_aclk [get_ports DFTSI*]
set_input_delay  0.0        -min -clock v_aclk [get_ports DFTSI*]

set_output_delay $v_aclkcycle50   -max -clock v_aclk [get_ports DFTSO*]
set_output_delay 0.0        -min -clock v_aclk [get_ports DFTSO*]

set_input_delay  $v_aclkcycle50   -max -clock v_aclk [get_ports DFTSE]
set_input_delay  0.0        -min -clock v_aclk [get_ports DFTSE]

set_input_delay  $v_aclkcycle50   -max -clock v_aclk [get_ports DFTCGEN]
set_input_delay  0.0        -min -clock v_aclk [get_ports DFTCGEN]

set_input_delay  $v_aclkcycle50   -max -clock v_aclk [get_ports DFTRAMBYP]
set_input_delay  0.0        -min -clock v_aclk [get_ports DFTRAMBYP]

set_input_delay  $v_aclkcycle50   -max -clock v_aclk [get_ports scantest_enable]
set_input_delay  0.0        -min -clock v_aclk [get_ports scantest_enable]

set_input_delay  $v_vclkcycle40   -max -clock v_vclk [get_ports scantest_enable]
set_input_delay  0.0        -min -clock v_vclk [get_ports scantest_enable]

# -----------------------------------------------------------------------------
# Constraining reset ports
# -----------------------------------------------------------------------------

set_input_delay  $v_vclkcycle20   -max -clock v_vclk [get_ports rstn]
set_input_delay  0.0        -min -clock v_vclk [get_ports rstn]

set_input_delay  $v_aclkcycle20   -max -clock v_aclk [get_ports aresetn]
set_input_delay  0.0        -min -clock v_aclk [get_ports aresetn]

# -----------------------------------------------------------------------------
# Inputs ports driving cells
# -----------------------------------------------------------------------------

# Clock pins
set clock_driving_cell        BUFH_X16N_${vtl-cell}
set clock_driving_from_pin    A
set clock_driving_pin        Y
 
# All other inputs
set driving_cell        BUF_X4N_${vtl-cell}
set driving_from_pin        A
set driving_pin            Y

# Drive input ports with a standard driving cell and input transition
set_driving_cell -library $target_library_name($slow_corner) \
                 -from_pin ${driving_from_pin} \
                 -input_transition_rise $max_transition($slow_corner) \
                 -input_transition_fall $max_transition($slow_corner) \
                 -lib_cell ${driving_cell} \
                 -pin ${driving_pin} \
                 [remove_from_collection [all_inputs] ${design_clock_names} ]

foreach clock_port $design_clock_names {
	set_driving_cell -library $target_library_name($slow_corner) \
                 -from_pin ${clock_driving_from_pin} \
                 -input_transition_rise $max_clock_transition($slow_corner) \
                 -input_transition_fall $max_clock_transition($slow_corner) \
                 -lib_cell ${clock_driving_cell} \
                 -pin ${clock_driving_pin} \
                 ${clock_port}
}

# -----------------------------------------------------------------------------
# Define output load
# -----------------------------------------------------------------------------
 
# Load appied to all output pins (in pF)
set output_load 0.015
 
# Load all outputs with suitable capacitance
set_load $output_load [all_outputs]

# -----------------------------------------------------------------------------
# Unset used local variables
# -----------------------------------------------------------------------------
 
unset clock_driving_cell clock_driving_from_pin clock_driving_pin driving_cell driving_from_pin driving_pin
 
foreach virtual_io_clock_cycle_percentage ${virtual_io_clock_cycle_percentages} {
  unset $virtual_io_clock_cycle_percentage
}
unset virtual_io_clock_cycle_percentages
