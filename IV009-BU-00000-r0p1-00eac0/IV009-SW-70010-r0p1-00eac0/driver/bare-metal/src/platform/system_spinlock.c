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
#include "system_spinlock.h"
#include <stdlib.h>


int system_spinlock_init( sys_spinlock *lock )
{
    return -1;
}

unsigned long system_spinlock_lock( sys_spinlock lock )
{

    return 0;
}

void system_spinlock_unlock( sys_spinlock lock, unsigned long flags )
{
}

void system_spinlock_destroy( sys_spinlock lock )
{
}
