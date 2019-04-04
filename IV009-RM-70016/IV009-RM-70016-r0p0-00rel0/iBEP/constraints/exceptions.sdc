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
#  File Name		: exceptions.sdc
#  Release Package	: iv009_r0p0_00rel0
#-----------------------------------------------------------------------------

# Disable erroneous two port memory CLKA to CLKB timing violations due to global clock gating.
set_disable_timing [get_lib_cells *RF_2P_H*] -from CLKA -to CLKB
set_disable_timing [get_lib_cells *RF_2P_H*] -from CLKB -to CLKA
