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

#ifndef SHARED_PTR
#define SHARED_PTR

#include <cstddef>
#include <new>
#include <ARMCPP11/Mutex.h>

namespace armcpp11
{

template <class T>
class SharedPtr
{
public:
    SharedPtr();
    SharedPtr(T *ptr);
    SharedPtr<T>(const SharedPtr<T>&);
    ~SharedPtr();

    void Reset();
    void Reset(T *ptr);

    T* Get();
    long UseCount();

    SharedPtr<T>& operator=(const SharedPtr<T>& other);

    T& operator*() const;
    T* operator->() const;
private:

    inline void SetRefCount(long value);
    inline void IncRefCount();
    inline void DecRefCount();

    Mutex   mRefMutex;
    T *     mBasePtr;
    long*   mRefCount;
};

template <class T>
SharedPtr<T>::SharedPtr() : mBasePtr(NULL), mRefCount(new long)
{
    SetRefCount(0);
}

template <class T>
SharedPtr<T>::SharedPtr(T* ptr) : mBasePtr(ptr), mRefCount(new long)
{
    SetRefCount(1);
}

template <class T>
SharedPtr<T>::SharedPtr(const SharedPtr<T>& other)
    : mBasePtr(other.mBasePtr), mRefCount(other.mRefCount)
{
    ++*mRefCount;
}

template <class T>
SharedPtr<T>::~SharedPtr()
{
    DecRefCount();
    if (UseCount() == 0) {
        delete mBasePtr;
        delete mRefCount;
    }
}

template <class T>
SharedPtr<T>& SharedPtr<T>::operator=(const SharedPtr<T>& other)
{
    if (this != &other) {
        DecRefCount();
        if (mRefCount == 0) {
            delete mBasePtr;
        }

        mBasePtr = other.mBasePtr;
        mRefCount = other.mRefCount;
        IncRefCount();
    }
    return *this;
}

template <class T>
void SharedPtr<T>::Reset()
{
    delete mBasePtr;
    SetRefCount(0);
}

template <class T>
void SharedPtr<T>::Reset(T* ptr)
{
    delete mBasePtr;
    mBasePtr = ptr;
    SetRefCount(1);
}

template <class T>
T* SharedPtr<T>::Get()
{
    return mBasePtr;
}

template <class T>
void SharedPtr<T>::SetRefCount(long value)
{
    LockGuard<Mutex> lock(mRefMutex);
    *mRefCount = value;
}

template <class T>
void SharedPtr<T>::IncRefCount()
{
    LockGuard<Mutex> lock(mRefMutex);
    ++*mRefCount;
}

template <class T>
void SharedPtr<T>::DecRefCount()
{
    LockGuard<Mutex> lock(mRefMutex);
    --*mRefCount;
}

template <class T>
long SharedPtr<T>::UseCount()
{
    LockGuard<Mutex> lock(mRefMutex);
    return *mRefCount;
}

template <class T>
T& SharedPtr<T>::operator*() const
{
    return *mBasePtr;
}

template <class T>
T* SharedPtr<T>::operator->() const
{
    return mBasePtr;
}

}

#endif
