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

#ifndef __FW_STREAM_ACCESS_H__
#define __FW_STREAM_ACCESS_H__

#include <vector>
#include <list>
#include <map>

#include <ARMCPP11/Mutex.h>

#include "AccessBase.h"
#include "BufferManager.h"
#include <BusManager/BaseDriverInterface.h>
#include <BusManager/BusManager.h>

namespace act {

    enum EStreamType {
        Unknown,        // not initialized
        Buffer,         // transactions are going through buffer
        Stream          // direct stream channel
    };

    class CFWStreamAccess : public CAccessBase {
    public:
        CFWStreamAccess() : stream_type(Unknown), reg_receiver(this, &CFWStreamAccess::ProcessRegReply), fw_receiver(this, &CFWStreamAccess::ProcessFWReply), api_receiver(this, &CFWStreamAccess::ProcessAPIReply) {}    // initialize receivers

        void Configure(TSmartPtr<CBusManager> bus_manager, EStreamType type, baseaddr buf_addr = 0, UInt32 buf_size = 0);
        void Start();
        void Stop();

        virtual CATLError PostFWRequest(TSmartPtr<CFWRequest> request);     // post FW (API, BUF) request
        virtual CATLError PostRegRequest(TSmartPtr<CRegRequest> request);   // post Register and LUT request

    private:
        static const basesize header_size = 12;                             // size of header in packet

        TSmartPtr<CBusManager> bus_manager;                                 // manager to work with
        EStreamType stream_type;                                            // type of the stream (buffered or true stream)
        CBufferManager buf_mng;                                             // buffer manager

        CATLError TransferPacket(TSmartPtr<CTransferPacket>& pack);
        void ProcessReply(TSmartPtr<CRegRequest>& request, TSmartPtr<CTransferPacket>& pack);
        void ProcessReply(TSmartPtr<CFWRequest>& request, TSmartPtr<CTransferPacket>& pack);

        // Registers access part
        armcpp11::Mutex reg_mutex;
        std::map<baseid,TSmartPtr<CRegRequest> > reg_req_list;
        void PushRequest(TSmartPtr<CRegRequest>& request);

        TATLObserver<CFWStreamAccess> reg_receiver;     // the receiver object
        void ProcessRegReply(void* ptr);                // receiver processing

        // FW access part
        armcpp11::Mutex fw_mutex;
        std::map<baseid,TSmartPtr<CFWRequest> > fw_req_list;
        void PushRequest(TSmartPtr<CFWRequest>& request);

        TATLObserver<CFWStreamAccess> fw_receiver;      // the receiver object
        void ProcessFWReply(void* ptr);                 // receiver processing

        TATLObserver<CFWStreamAccess> api_receiver;     // the receiver object
        void ProcessAPIReply(void* ptr);                // receiver processing
    };
}

#endif // __FW_STREAM_ACCESS_H__
