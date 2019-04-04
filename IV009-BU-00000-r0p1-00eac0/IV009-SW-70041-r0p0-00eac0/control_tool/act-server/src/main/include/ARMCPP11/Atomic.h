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

#ifndef _ATOMIC_H
#define _ATOMIC_H

#ifdef _WIN32
#include <windows.h>
#endif

namespace armcpp11
{

template <class T>
class Atomic
{
public:
    Atomic();
    Atomic(T value);
    inline T operator=(T value);

    inline T Load();
    inline void Store(T value);
    inline T FetchAdd(T value);
    inline T FetchSub(T value);

    inline operator T() { return Load(); }

    inline T operator++();
    inline T operator++(int);
    inline T operator--();
    inline T operator--(int);

private:
    Atomic(const Atomic&);
    Atomic& operator=(const Atomic&);

#ifndef _WIN32
    T       mValue;
#else
    LONGLONG volatile      mValue;
#endif
};

template <>
class Atomic<bool>
{
public:
    Atomic() : mValue(false) { }
    Atomic(bool value) : mValue(value) { }
    inline bool operator=(bool value)
    {
#ifndef _WIN32
        static_cast<void>(__sync_lock_test_and_set(&mValue, value));
#else
        InterlockedExchange(&mValue, value);
#endif
        return value;
    }

    inline bool Load()
    {
#ifndef _WIN32
        return __sync_or_and_fetch(&mValue, 0);
#else
        return static_cast<bool>(InterlockedOr(&mValue, 0));
#endif
    }

    inline void Store(bool value)
    {
#ifndef _WIN32
        static_cast<void>(__sync_lock_test_and_set(&mValue, value));
#else
        InterlockedExchange(&mValue, value);
#endif
    }

    inline operator bool() { return Load(); }

private:
    Atomic(const Atomic&);
    Atomic& operator=(const Atomic&);

#ifndef _WIN32
    bool       mValue;
#else
    ULONGLONG volatile       mValue;
#endif
};

template <class T>
Atomic<T>::Atomic()
{ }

template <class T>
Atomic<T>::Atomic(T value) : mValue(value)
{ }

template <class T>
T Atomic<T>::operator=(T value)
{
#ifndef _WIN32
        static_cast<void>(__sync_lock_test_and_set(&mValue, value));
#else
        InterlockedExchange64(&mValue, value);
#endif
        return Load();
}

template <class T>
T Atomic<T>::Load()
{
#ifndef _WIN32
    return __sync_or_and_fetch(&mValue, 0);
#else
    return static_cast<T>(InterlockedOr64(&mValue, 0));
#endif
}

template <class T>
void Atomic<T>::Store(T value)
{
#ifndef _WIN32
    static_cast<void>(__sync_lock_test_and_set(&mValue, value));
#else
    InterlockedExchange64(&mValue, value);
#endif
}

template <class T>
T Atomic<T>::FetchAdd(T value)
{
#ifndef _WIN32
    return __sync_fetch_and_add(&mValue, value);
#else
    T currentValue = Load();
    return static_cast<T>(InterlockedExchange64(&mValue,
                                                currentValue + value));
#endif
}

template <class T>
T Atomic<T>::FetchSub(T value)
{
#ifndef _WIN32
    return __sync_fetch_and_sub(&mValue, value);
#else
    T currentValue = Load();
    return static_cast<T>(InterlockedExchange64(&mValue,
                                                currentValue - value));
#endif
}

template <class T>
T Atomic<T>::operator++()
{
#ifndef _WIN32
    return __sync_add_and_fetch(&mValue, 1);
#else
    return static_cast<T>(InterlockedIncrement64(&mValue));
#endif
}

template <class T>
T Atomic<T>::operator++(int)
{
    return FetchAdd(1);
}

template <class T>
T Atomic<T>::operator--()
{
#ifndef _WIN32
    return __sync_sub_and_fetch(&mValue, 1);
#else
    return static_cast<T>(InterlockedDecrement64(&mValue));
#endif
}

template <class T>
T Atomic<T>::operator--(int)
{
    return FetchSub(1);
}

}

#endif
