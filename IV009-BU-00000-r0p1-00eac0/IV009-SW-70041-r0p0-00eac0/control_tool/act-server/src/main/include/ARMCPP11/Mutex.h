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

#ifndef MUTEX_H
#define MUTEX_H

#include <pthread.h>
#include <cstdio>
#include <stdexcept>

namespace armcpp11
{

class Mutex
{
    typedef pthread_mutex_t*     NativeHandleType;

public:
    Mutex();
    ~Mutex();

    void Lock();
    bool TryLock();
    void Unlock();

    NativeHandleType NativeHandle();

private:
    Mutex(const Mutex&);
    Mutex& operator=(Mutex&);

    pthread_mutexattr_t     mAttr;
    pthread_mutex_t         mMutex;
};

class RecursiveMutex
{
    typedef pthread_mutex_t*     NativeHandleType;

public:
    RecursiveMutex();
    ~RecursiveMutex();

    void Lock();
    bool TryLock();
    void Unlock();

    NativeHandleType NativeHandle();

private:
    RecursiveMutex(const RecursiveMutex&);
    RecursiveMutex& operator=(RecursiveMutex&);

    pthread_mutexattr_t     mAttr;
    pthread_mutex_t         mMutex;
};

template <class MutexType>
class UniqueLock
{
public:
    UniqueLock(MutexType& mutex);
    ~UniqueLock();

    bool OwnsLock();
    void Lock();
    bool TryLock();
    void Unlock();
    MutexType& Mutex();

private:
    UniqueLock(const MutexType&);
    UniqueLock& operator=(MutexType&);

    MutexType&  mMutex;
    bool        mOwnsLock;
};

template <class MutexType>
UniqueLock<MutexType>::UniqueLock(MutexType &mutex)
    : mMutex(mutex), mOwnsLock(false)
{
    mMutex.Lock();
    mOwnsLock = true;
}

template <class MutexType>
UniqueLock<MutexType>::~UniqueLock()
{
    if (mOwnsLock) {
        mMutex.Unlock();
    }
}

template <class MutexType>
void UniqueLock<MutexType>::Lock()
{
    mMutex.Lock();
    mOwnsLock = true;
}

template <class MutexType>
void UniqueLock<MutexType>::Unlock()
{
    mMutex.Unlock();
    mOwnsLock = false;
}

template <class MutexType>
MutexType& UniqueLock<MutexType>::Mutex()
{
    return mMutex;
}

template <class MutexType>
class LockGuard
{
public:
    LockGuard(MutexType& mutex);
    ~LockGuard();

private:
    LockGuard(const LockGuard&);
    LockGuard& operator=(const LockGuard&);

    MutexType& mMutex;
};

template <class MutexType>
LockGuard<MutexType>::LockGuard(MutexType &mutex)
    : mMutex(mutex)
{
    mMutex.Lock();
}

template <class MutexType>
LockGuard<MutexType>::~LockGuard()
{
    mMutex.Unlock();
}

}

#endif // MUTEX_H
