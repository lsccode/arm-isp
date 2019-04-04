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

#ifndef __BUFFER_MANAGER_H__
#define __BUFFER_MANAGER_H__

#include <ARMCPP11/Chrono.h>
#include <map>
#include <ARMCPP11/ConditionVariable.h>
#include <ARMCPP11/Atomic.h>
#include <ARMCPP11/Mutex.h>
#include <ATL/ATLThread.h>
#include <BusManager/BusManager.h>
#include <ARMCPP11/SharedPtr.h>
#include "AccessRequest.h"

namespace act {

    class CBufferManager {
    public:
        CBufferManager():
            buf_size(0),
            buf_data_size(0),
            thread_stop(false),
            receiver(this, &CBufferManager::ProcessRxReply),
            transmitter(this, &CBufferManager::ProcessTxReply),
            api_listener(this, &CBufferManager::ProcessAPIReply) {}
        ~CBufferManager();

        void Configure(TSmartPtr<CBusManager> bus_manager, baseaddr buf_addr = 0, UInt32 buf_size = 0);
        void Start();
        void Stop();

        CATLError TransferPacket(TSmartPtr<CTransferPacket>& packet);

        struct CAPIRequest {
            const armcpp11::HighResolutionClock::timepoint sent_time;    // here we keep time when transaction has been put to list (for timeouts)
            TSmartPtr<CFWRequest> request;
            CEventListener<> listener;
            CAPIRequest(const TSmartPtr<CFWRequest>& req) : sent_time(armcpp11::HighResolutionClock::Now()), request(req) {}
            bool timeout() const {return armcpp11::DurationCast<armcpp11::MilliSeconds>(armcpp11::HighResolutionClock::Now() - sent_time).Count() > request->timeout_ms;}
        };

        CATLError ProcessAPIRequest(const armcpp11::SharedPtr<CAPIRequest>& request);

    private:
        static const basesize max_request_num = 100;        // maximum queue size for requests
        static const UInt32 api_size = 12;                // offset of state register in the memory
        static const UInt32 state_offset = 0;             // offset of state register in the memory
        static const UInt32 write_inx_offset = 4;         // offset of write index register in the memory
        static const UInt32 read_inx_offset = 8;          // offset of read index register in the memory
        static const UInt32 header_size = 12;             // actual data offset in the memory
        static const UInt32 data_offset = header_size;    // actual data offset in the memory

        static const UInt32 tx_cmd_offset = api_size - 1; // actual data offset in the memory

        TSmartPtr<CBusManager> bus_manager; // manager to work with
        baseaddr buf_tx_addr;               // buffer base TX address
        baseaddr buf_rx_addr;               // buffer base TX address
        baseaddr api_tx_addr;               // buffer base TX address
        baseaddr api_rx_addr;               // buffer base TX address
        UInt32 buf_size;                    // buffer size
        UInt32 buf_data_size;               // buffer data size

        struct CTransaction {
            const armcpp11::HighResolutionClock::timepoint sent_time; // here we keep time when transaction has been put to list (for timeouts)
            const UInt32 full_size;
            const UInt32 id;
            bool transmitted;
            TSmartPtr<CTransferPacket> packet;                              // the packet itself
            CTransaction(TSmartPtr<CTransferPacket>& pack) : sent_time(armcpp11::HighResolutionClock::Now()), full_size((UInt32)((pack->data.size()+3)&~3)), id((UInt32)(pack->id&0xFFFFFFFF)), transmitted(false), packet(pack) {}
            bool timeout() const {return armcpp11::DurationCast<armcpp11::MilliSeconds>(armcpp11::HighResolutionClock::Now() - sent_time).Count() > packet->timeout_ms;}
        };
        typedef std::deque<armcpp11::SharedPtr<CTransaction> > tx_list_type;
        typedef std::map<UInt32,armcpp11::SharedPtr<CTransaction> > rx_list_type;
        typedef std::map<UInt32,armcpp11::SharedPtr<CAPIRequest> > api_list_type;

        // TX part
        atl::thread tx_thread;                  // transmitter thread
        std::deque<TSmartPtr<CTransferPacket> > tx_received_packets;         // queue of received packets
        tx_list_type tx_reqest_queue;           // queue of requests
        armcpp11::Mutex tx_mutex;                    // mutex for lock everything
        armcpp11::ConditionVariable tx_cv;          // condition variable to wake up working thread
        static void * tx_thread_processing_wrapper(void* context); // wrapper for atl::thread
        void tx_thread_processing();            // trasmitter thread itself

        // RX part
        armcpp11::Mutex rx_mutex;                    // mutex for lock everything
        armcpp11::ConditionVariable rx_cv;          // condition variable to wake up working thread
        std::deque<TSmartPtr<CTransferPacket> > rx_received_packets;         // queue of received packets
        rx_list_type rx_reqest_list;            // queue of requests
        static void * rx_thread_processing_wrapper(void* context); // wrapper for atl::thread
        void rx_thread_processing();            // receiver thread itself

        // API part
        atl::thread api_thread;                 // API managment thread
        std::deque<TSmartPtr<CTransferPacket> > api_received_packets;        // queue of received packets
        std::deque<armcpp11::SharedPtr<CAPIRequest> > api_reqest_queue;          // queue of requests
        armcpp11::Mutex api_mutex;                   // mutex for lock everything
        armcpp11::ConditionVariable api_cv;         // condition variable to wake up working thread
        static void * api_thread_processing_wrapper(void* context); // wrapper for atl::thread
        void api_thread_processing();           // trasmitter thread itself

        // common part
        armcpp11::Atomic<bool> thread_stop;          // flag to stop all threads
        void create_threads();                  // create receiver thread
        void stop_threads();                    // stop receiver thread

        // helpers
        void send_data(UInt8* data, basesize size, UInt32 offset, baseid id, TATLObserver<CBufferManager>& listener);
        void set_value(UInt32 value, UInt32 offset, baseid id, TATLObserver<CBufferManager>& listener);
        void request_status(UInt32 offset, baseid id, TATLObserver<CBufferManager>& listener);
        void request_data(UInt32 offset, basesize size, baseid id, TATLObserver<CBufferManager>& listener);
        void request_api(UInt32 offset, UInt32 size);
        void reset_api_rx();

        TATLObserver<CBufferManager> receiver;      // the receiver object
        void ProcessRxReply(void* ptr);             // receiver processing
        TATLObserver<CBufferManager> transmitter;   // the receiver object
        void ProcessTxReply(void* ptr);             // receiver processing
        TATLObserver<CBufferManager> api_listener;  // the receiver object
        void ProcessAPIReply(void* ptr);            // receiver processing
    };
}

#endif // __BUFFER_MANAGER_H__
