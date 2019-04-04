//----------------------------------------------------------------------------
//   The confidential and proprietary information contained in this file may
//   only be used by a person authorised under and to the extent permitted
//   by a subsisting licensing agreement from ARM Limited or its affiliates.
//
//          (C) COPYRIGHT [2018] ARM Limited or its affiliates.
//              ALL RIGHTS RESERVED
//
//   This entire notice must be reproduced on all copies of this file
//   and copies of this file may only be made by a person if such person is
//   permitted to do so under the terms of a subsisting license agreement
//   from ARM Limited or its affiliates.
//----------------------------------------------------------------------------

#include "acamera_firmware_config.h"


#if defined( ACAMERA_ISP_PROFILING ) && ( ACAMERA_ISP_PROFILING == 1 )
#include "acamera_profiler.h"
#include "acamera_fw.h"

uint64_t time_diff( struct timespec start, struct timespec end )
{
    return 0;
}

int32_t cpu_get_freq( void )
{
    return 0;
}


void cpu_start_clocks( void )
{
}

void cpu_start_isr_clocks( void )
{
}

uint64_t cpu_stop_clocks( void )
{
    return 0;
}

uint64_t cpu_stop_isr_clocks( void )
{
    return 0;
}

void cpu_init_profiler( void )
{
}

#endif
