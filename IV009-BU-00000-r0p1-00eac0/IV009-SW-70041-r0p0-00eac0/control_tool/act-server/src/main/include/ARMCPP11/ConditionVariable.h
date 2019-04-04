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

#ifndef CONDITIONVARIABLE_H
#define CONDITIONVARIABLE_H

#include <pthread.h>
#include <ARMCPP11/Mutex.h>
#include <ARMCPP11/Chrono.h>
#include <stdexcept>
#include <errno.h>

namespace  armcpp11
{

enum CVStatus
{
    NoTimeOut,
    TimeOut
};

class ConditionVariable
{
public:
    ConditionVariable();
    ~ConditionVariable();

    void Wait(UniqueLock<Mutex>& lock);
    template <class Clock, class Duration>
    CVStatus WaitUntil(UniqueLock<Mutex>& lock,
                   TimePoint<Clock, Duration>& timeout);

    void Notify();
    void NotifyAll();
private:
    pthread_cond_t      mCondVar;
};

template <class Clock, class Duration>
CVStatus ConditionVariable::WaitUntil(UniqueLock<Mutex>& lock,
                                  TimePoint<Clock, Duration>& timeout)
{
    Duration d = DurationCast<NanoSeconds>(timeout.TimeSinceEpoch());

    timespec ts;

    ts.tv_sec = static_cast<long>(DurationCast<Seconds>(d).Count());
    ts.tv_nsec = static_cast<long>(
                    (NanoSeconds((UIntMax)ts.tv_sec * 1000000000L) - d)
                .Count());

    pthread_mutex_t *mutex = lock.Mutex().NativeHandle();
    int rc = pthread_cond_timedwait(&mCondVar, mutex, &ts);

    if (rc == ETIMEDOUT) {
        return TimeOut;
    }
    else if (rc) {
        char buffer[256];
        sprintf(buffer, "Condition variable WaitUntil failed %d", rc);
        throw std::runtime_error(buffer);
    }

    return NoTimeOut;
}

}

#endif // CONDITIONVARIABLE_H
