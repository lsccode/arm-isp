//----------------------------------------------------------------------------
//   The confidential and proprietary information contained in this file may
//   only be used by a person authorised under and to the extent permitted
//   by a subsisting licensing agreement from ARM Limited or its affiliates.
//
//          (C) COPYRIGHT 2018 ARM Limited or its affiliates
//              ALL RIGHTS RESERVED
//
//   This entire notice must be reproduced on all copies of this file
//   and copies of this file may only be made by a person if such person is
//   permitted to do so under the terms of a subsisting license agreement
//   from ARM Limited or its affiliates.
//----------------------------------------------------------------------------

#include <ARMCPP11/ConditionVariable.h>

using namespace armcpp11;

ConditionVariable::ConditionVariable()
{
    pthread_cond_init(&mCondVar, NULL);
}

ConditionVariable::~ConditionVariable()
{
    NotifyAll();
    pthread_cond_destroy(&mCondVar);
}

void ConditionVariable::Wait(UniqueLock<Mutex> &lock)
{
    pthread_mutex_t *mutex = lock.Mutex().NativeHandle();

    pthread_cond_wait(&mCondVar, mutex);
}

void ConditionVariable::Notify()
{
    pthread_cond_signal(&mCondVar);
}

void ConditionVariable::NotifyAll()
{
    pthread_cond_broadcast(&mCondVar);
}
