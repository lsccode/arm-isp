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

#include <ARMCPP11/Thread.h>

using namespace armcpp11;

Thread::Thread() : mJoinable(false), mThread(-1),mFunc(NULL)
{

}

Thread::Thread(Function func, ArgType arg) : mJoinable(true)
{
    int retCode = pthread_create(&mThread, NULL, func, arg);

    if (retCode != 0) {
        char buffer[256];
        sprintf(buffer, "Creating thread failed with error %d", retCode);
        throw std::runtime_error(buffer);
    }
}

Thread::~Thread()
{
    if (Joinable()) {
        Join();
    }
}

Thread::Thread(const Thread& other)
{
    this->mThread = other.mThread;
    this->mJoinable = other.mJoinable;
    this->mFunc = other.mFunc;

    Thread* th = const_cast<Thread*>(&other);
    th->mThread = -1;
    th->mJoinable = false;
    th->mFunc = NULL;
}

Thread& Thread::operator=(const Thread& other)
{
    if (this != &other) {
        this->mThread = other.mThread;
        this->mJoinable = other.mJoinable;
        this->mFunc = other.mFunc;
        Thread* th = const_cast<Thread*>(&other);
        th->mThread = -1;
        th->mJoinable = false;
        th->mFunc = NULL;
    }
    return *this;
}

void Thread::Join()
{
    int retCode = pthread_join(mThread, NULL);

    if (retCode != 0) {
        char buffer[256];
        sprintf(buffer, "Joining thread %llu failed with error %d",
                static_cast<unsigned long long>(mThread), retCode);
        throw std::runtime_error(buffer);
    }

    mJoinable = false;
}

bool Thread::Joinable()
{
    return mJoinable;
}

void Thread::Detach()
{
    int retCode = 0;
    if (Joinable()) {
        retCode = pthread_detach(mThread);
    }
    else {
        retCode = EINVAL;
    }

    if (retCode != 0) {
        char buffer[256];
        sprintf(buffer, "Joining thread %llu failed with error %d",
                static_cast<unsigned long long>(mThread), retCode);
        throw std::runtime_error(buffer);
    }

    mJoinable = false;
}

void Thread::Swap(Thread& other)
{
    Thread th = other;
    other = *this;
    *this = th;
}

const ThisThread::Id ThisThread::GetId()
{
    return pthread_self();
}

void ThisThread::YieldThread(void)
{
#ifdef _WIN32
   SwitchToThread();
#else
    sched_yield();
#endif
}

template <>
void ThisThread::SleepFor(const NanoSeconds& ns)
{
#ifndef _WIN32
    timespec ts;

    ts.tv_sec = static_cast<long>(DurationCast<Seconds>(ns).Count());
    ts.tv_nsec = static_cast<long>(
                    (NanoSeconds((UIntMax)ts.tv_sec * 1000000000L) - ns)
                .Count());
    nanosleep(&ts, NULL);
#else
    UIntMax ms = DurationCast<MilliSeconds>(ns).Count();
    SleepEx(ms, FALSE);
#endif
}

