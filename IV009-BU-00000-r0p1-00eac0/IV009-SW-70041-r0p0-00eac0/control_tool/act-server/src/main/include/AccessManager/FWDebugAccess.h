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

#ifndef __FW_DEBUG_ACCESS_H__
#define __FW_DEBUG_ACCESS_H__

#include <vector>
#include <queue>
#include <ARMCPP11/ConditionVariable.h>
#include <ARMCPP11/Atomic.h>
#include <ARMCPP11/Mutex.h>

#include <ATL/ATLThread.h>
#include "AccessBase.h"
#include <BusManager/BaseDriverInterface.h>
#include <BusManager/BusManager.h>

// this is directions for set and get
enum {
    COMMAND_SET = 0,
    COMMAND_GET = 1
};

namespace act {

    class CFWDebugAccess : public CAccessBase {
    public:
        CFWDebugAccess() : state(WaitRequest),
                           buf_inx(0),
                           receiver(this, &CFWDebugAccess::ProcessReply)      // initialize the receiver
        { }
        ~CFWDebugAccess() {stop_thread();}

        void Configure(TSmartPtr<CBusManager> bus_manager, baseaddr dbg_addr);  // set bus manager and debug address
        inline void Start() {
            start_thread();
        }
        inline void Stop() {
            stop_thread();
        }
        virtual CATLError PostFWRequest(TSmartPtr<CFWRequest> request);         // override post FW request function
    private:
        static const basesize max_request_num = 100;    // maximum queue size for requests
        static const basesize max_wait_packets = 100;        // maximum resending for status packets
        TSmartPtr<CBusManager> bus_manager;             // bus manager
        baseaddr dbg_addr;                              // debug registers offset

        // state machine
        armcpp11::Atomic<bool> stop_flag;                    // stop flag for running thread
        atl::thread working_thread;                     // the thread itself
        std::deque<TSmartPtr<CTransferPacket> > received_packets;    // queue of received packets
        std::deque<TSmartPtr<CFWRequest> > reqest_queue;             // queue of requests
        armcpp11::Mutex mtx;                                 // mutex for lock everything
        armcpp11::ConditionVariable cv;                     // condition variable to wake up working thread
        static void * work_wrapper(void * context);     // atl thread wrapper
        void work();                                    // working thread itself
        void stop_thread();
        void start_thread();

        // local functions used by the working thread
        bool send_data(UInt8 data, baseid id);
        bool send_buf_inx(UInt16 inx, const UInt8* data, baseid id);
        bool send_inx(UInt16 inx, baseid id);
        bool send_value(UInt32 val, baseid id);
        bool send_full_api(UInt8 type, UInt8 cmd, UInt32 value, baseid id);
        bool send_command(UInt8 cmd, baseid id);
        bool request_status(baseid id, basesize num = 1, basesize buffer_size = max_wait_packets);
        void drop_transaction(ERequestState tr_state = Fail);

        // state machine itself
        enum ESM {WaitRequest, Reset, SetType, SetId, SetDir, SetValue, Run, BufSize, BufGet, BufSet, BufApply} state;
        basesize cnt_timeout;                           // timeout counter
        UInt16 buf_inx;                           // buffer index counter
        TSmartPtr<CFWRequest> request;                  // currently processing request

        TATLObserver<CFWDebugAccess> receiver;          // the receiver object
        void ProcessReply(void* ptr);                   // receiver function
    };
}

#endif // __FW_DEBUG_ACCESS_H__
