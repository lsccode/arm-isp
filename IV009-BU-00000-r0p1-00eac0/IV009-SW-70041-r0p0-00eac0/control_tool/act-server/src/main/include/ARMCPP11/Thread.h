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

#ifndef THREAD_H
#define THREAD_H

#ifdef _WIN32
#include <windows.h>
#endif

#include <ARMCPP11/Chrono.h>
#include <pthread.h>
#include <errno.h>
#include <sched.h>
#include <cstdio>
#include <cstdarg>
#include <string>
#include <stdexcept>

namespace armcpp11
{

namespace ThisThread
{
    typedef pthread_t Id;

    const Id GetId();
    void YieldThread(void);

    template <class Duration>
    void SleepFor(const Duration& d)
    {
        SleepFor(DurationCast<NanoSeconds>(d));
    }

    template <>
    void SleepFor(const NanoSeconds& ns);
}

template <class FuncType, class ArgType>
class Function
{
public:
    Function(FuncType func, ArgType arg)
    {
        mFunc = func;
        mArg = arg;
        this->operator()();
    }

private:
    static void* Callable(void *arg)
    {
        mFunc(*static_cast<ArgType*>(arg));
        return NULL;
    }

    static FuncType mFunc;
    static ArgType  mArg;
};

template <class FuncType, class ArgType>
FuncType Function<FuncType, ArgType>::mFunc = NULL;
template <class FuncType, class ArgType>
ArgType Function<FuncType, ArgType>::mArg;

template <class FuncType>
class Function<FuncType, void>
{
public:
    Function(FuncType func)
    {
        mFunc = func;
    }

private:
    static void* Callable(void *arg)
    {
        mFunc();
        return NULL;
    }

    static FuncType mFunc;
};
template <class FuncType>
FuncType Function<FuncType, void>::mFunc = NULL;

class Thread
{
public:

    typedef void* ArgType;
    typedef ArgType (*Function)(ArgType);

private:
    bool                mJoinable;
    ThisThread::Id      mThread;
    Function            mFunc;


public:
    Thread();
    Thread(Function func, ArgType arg = NULL);
    virtual ~Thread();

    Thread& operator=(const Thread&);
    Thread(const Thread&);

    void Join();
    bool Joinable();
    void Detach();
    void Swap(Thread& other);
};

}

#endif // THREAD_H
