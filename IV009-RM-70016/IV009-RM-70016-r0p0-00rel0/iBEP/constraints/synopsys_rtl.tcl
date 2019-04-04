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
#  File Name		: synopsys_rtl.tcl
#  Release Package	: iv009_r0p0_00rel0
#-----------------------------------------------------------------------------

set mali_iv009_rtl ../../../logical

# ------------------------------------------------------------------------------
# create search path
# ------------------------------------------------------------------------------

set mali_iv009_configure   ${mali_iv009_rtl}/config
set mali_iv009_includes    ${mali_iv009_rtl}/mali_iv009_core/clear_text/include

set search_path [ concat ${search_path} \
		      ${mali_iv009_configure} \
		      ${mali_iv009_includes} \
		     ]

# -----------------------------------------------------------------------------
# Create lists of target RTL from the provided filelist
# -----------------------------------------------------------------------------

## VHDL RTL source file list
set VHDL_FH [open ${mali_iv009_rtl}/mali_iv009_synopsys_vhdl.list r]

set rtl_vhdl_file_list [list ]

while { [gets $VHDL_FH line] >= 0 } {
    lappend rtl_vhdl_file_list ${mali_iv009_rtl}/$line
}

close $VHDL_FH

## Verilog RTL source file list
set VERILOG_FH [open ${mali_iv009_rtl}/mali_iv009_synopsys_verilog.list r]

set rtl_verilog_file_list [list ]

while { [gets $VERILOG_FH line] >= 0 } {
    lappend rtl_verilog_file_list ${mali_iv009_rtl}/$line
}

close $VERILOG_FH

## Replace generic clock gate with technology cell
regsub -all {models/clkgate} $rtl_verilog_file_list {models/tsmc_cln16fpll001_clkgate} rtl_verilog_file_list


## Replace the generic Memories integration wrapper
lappend rtl_verilog_file_list ${mali_iv009_rtl}/models/tsmc_cln16fpll001_modules/mali_iv009_isp_core_mem.v


# -----------------------------------------------------------------------------
# Define RTL image
# -----------------------------------------------------------------------------

set rtl_image [concat \
		   $rtl_vhdl_file_list \
		   $rtl_verilog_file_list \
		  ]



