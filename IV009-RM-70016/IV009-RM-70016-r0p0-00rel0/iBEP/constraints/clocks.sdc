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
#  File Name		: clocks.sdc
#  Release Package	: iv009_r0p0_00rel0
#-----------------------------------------------------------------------------

set clock_names_list [list vclk aclk]

set vclk {
  port                                          vclk
  virtual_io_clock_name                         v_vclk
  period                                        1.666
  period_jitter                                 0.025
  dutycycle_jitter                              0.0125
  synthesis_skew_estimate                       0.150
  synthesis_network_latency_estimate            1.2
  prects_skew_estimate                          0.150
  prects_network_latency_estimate               0.0
  sta_network_latency_estimate			0.700
  max_transition_clock_path_factor              0.07
  max_transition_data_path_factor               0.12
  clock_group_name                              clock_group_vclk
}

set aclk {
  port                                          aclk
  virtual_io_clock_name                         v_aclk
  period                                        1.250
  period_jitter                                 0.025
  dutycycle_jitter                              0.0125
  synthesis_skew_estimate                       0.150
  synthesis_network_latency_estimate            1.2
  prects_skew_estimate                          0.150
  prects_network_latency_estimate               0.0
  sta_network_latency_estimate			0.470
  max_transition_clock_path_factor              0.08
  max_transition_data_path_factor               0.16
  clock_group_name                              clock_group_aclk
}

