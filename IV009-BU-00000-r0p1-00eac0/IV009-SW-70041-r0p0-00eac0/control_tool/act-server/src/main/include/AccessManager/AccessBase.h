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

#ifndef __ACCESS_BASE_H__
#define __ACCESS_BASE_H__

#include <ATL/ATLTypes.h>
#include <ATL/ATLTemplates.h>
#include <ATL/ATLLogger.h>

#include "AccessRequest.h"

#include <stdexcept>
#include <map>
#include <ARMCPP11/Chrono.h>
#include <ARMCPP11/Mutex.h>
#include <ARMCPP11/ConditionVariable.h>

namespace act {

    // this exeption is used when waiting timeout occures
    class TimeoutException : std::runtime_error {
    public:
        TimeoutException() : runtime_error("Timeout") {}
    };

    template<class T, int max_number = 100000> class TLockedList {
    public:
        typedef std::map<baseid,T> container_type;
        void push(T& item);                     // push the item into list
        T get(baseid id, UInt32 timeout_ms);  // get the item from list. timeout = 0 means wait infinite
        basesize GetSize() const {              // returns current number of elements in the list
            armcpp11::LockGuard<armcpp11::Mutex> lock(mtx);
            return items.size();
        }
    private:
        container_type items;           // container of items
        mutable armcpp11::Mutex mtx;         // mutex to protect the list
        armcpp11::ConditionVariable cv;     // condition variable to wake up waiting threads
    };

    class CAccessBase {    // base class for Access Classes
    public:
        virtual CATLError PostFWRequest(TSmartPtr<CFWRequest>);             // post FW (API, BUF) request
        virtual CATLError PostRegRequest(TSmartPtr<CRegRequest>);           // post Register and LUT request
        TSmartPtr<CFWRequest> GetFWResult(baseid id, UInt32 timeout_ms);  // get request result with this id
        TSmartPtr<CRegRequest> GetRegResult(baseid id, UInt32 timeout_ms);
        basesize GetRegSize() const {return reg_list.GetSize();}            // get number of available elements in the register result list
        basesize GetFWSize() const {return fw_list.GetSize();}              // get number of available elements in the FW result list
    protected:
        TLockedList<TSmartPtr<CRegRequest> > reg_list;                       // register request result list
        TLockedList<TSmartPtr<CFWRequest> > fw_list;                         // FW request result list
    };

    // Template functions implementation
    template<class T, int max_number> void TLockedList<T,max_number>::push(T& item)
    {
        armcpp11::UniqueLock<armcpp11::Mutex> lock(mtx);     // first lock the mutex
        if (items.size() >= max_number) {           // if number of items exceeds limit delete one
            items.erase(items.begin());             // delete the first
            atl::SysLog()->LogError(LOG_CONTEXT, "receiver queue limit exceeds: first item is deleted");
        }
        items.insert(typename container_type::value_type(item->id,item));    // insert new item
        lock.Unlock();              // now unlock the mutex
        cv.NotifyAll();            // notify everyone who is waiting
    }

    template<class T, int max_number> T TLockedList<T,max_number>::get(baseid id, UInt32 timeout_ms)
    {
        T res;    // here result will be stored
        armcpp11::CVStatus status = armcpp11::NoTimeOut;    // timeout status
        armcpp11::HighResolutionClock::timepoint t_finish = armcpp11::HighResolutionClock::Now();
        t_finish += armcpp11::DurationCast<armcpp11::HighResolutionClock::duration>(armcpp11::MilliSeconds(timeout_ms)); // time when we have to finish waiting
        armcpp11::UniqueLock<armcpp11::Mutex> lock(mtx);    // lock mutex first
        do {
            typename container_type::iterator it = items.find(id);
            if (it != items.end()){
                res = it->second;   // store result
                items.erase(it);    // remove from the list
                break;
            }
            else {    // if not found go to waiting
                if (timeout_ms) {   // if non-zero timeout
                    status = cv.WaitUntil(lock,t_finish);  // wait until finish time
                }
                else {
                    cv.Wait(lock);    // wait without timeout
                }
            }
        } while (status != armcpp11::TimeOut);    // do this until timeout
        if (status == armcpp11::TimeOut) {        // if not found throw the timeout exeption
            throw TimeoutException();
        }
        return res;    // return the result
    }

}

#endif // __ACCESS_BASE_H__
