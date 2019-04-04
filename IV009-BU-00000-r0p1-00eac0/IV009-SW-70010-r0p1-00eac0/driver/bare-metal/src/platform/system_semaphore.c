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

#include "acamera_types.h"
#include "system_semaphore.h"

int32_t system_semaphore_init( semaphore_t *sem )
{
    return -1;
}

int32_t system_semaphore_raise( semaphore_t sem )
{
    return -1;
}

int32_t system_semaphore_wait( semaphore_t sem, uint32_t timeout_ms )
{
    return -1;
}

int32_t system_semaphore_destroy( semaphore_t sem )
{
    return -1;
}
