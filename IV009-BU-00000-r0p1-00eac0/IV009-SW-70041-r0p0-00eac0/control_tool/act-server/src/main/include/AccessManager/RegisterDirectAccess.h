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

#ifndef __REGISTER_DIRECT_ACCESS_H__
#define __REGISTER_DIRECT_ACCESS_H__

#include <vector>
#include <map>

#include <ARMCPP11/Mutex.h>

#include "AccessBase.h"
#include <BusManager/BaseDriverInterface.h>
#include <BusManager/BusManager.h>

namespace act {

    class CRegisterDirectAccess : public CAccessBase {
        static const basesize max_request_size;

    public:
        CRegisterDirectAccess() : receiver(this, &CRegisterDirectAccess::ProcessReply) {}   // initialize receiver
        CATLError PostRegRequest(TSmartPtr<CRegRequest> request);                           // register and LUT request
        void Configure(TSmartPtr<CBusManager> bus_manager);                                 // configuration

    private:
        TSmartPtr<CBusManager> bus_manager;                                                 // used bus manager
        struct CTransaction {                                                               // each request produces this transaction
            TSmartPtr<CRegRequest> request;                                                 // keep request here
            std::vector<TSmartPtr<CTransferPacket> > packets;                                // packets vector
            CTransaction(TSmartPtr<CRegRequest> req, basesize n) : request(req) {           // constructor
                packets.resize(n);                                                          // allocate required amount of packets
                for (basesize i = 0; i < n; i++) {
                    packets[i] = new CTransferPacket();                                     // create packet
                    packets[i]->reg_id = req->id;                                           // set request id
                }
            }
        };

        typedef std::map<baseid,CTransaction> transactions_container;
        transactions_container transactions;                        // list of processing transactions
        armcpp11::Mutex transactions_mutex;                              // mutex
        void PushTransaction(const CTransaction& transaction);      // push transaction into the queue
        void SendTransaction(const CTransaction& transaction);      // send all packets from transaction
        void ProcessTransaction(CTransaction& tr);                  // Complete transaction

        TATLObserver<CRegisterDirectAccess> receiver;               // the receiver object
        void ProcessReply(void* ptr);                               // reply processing function
    };
}

#endif // __REGISTER_DIRECT_ACCESS_H__
