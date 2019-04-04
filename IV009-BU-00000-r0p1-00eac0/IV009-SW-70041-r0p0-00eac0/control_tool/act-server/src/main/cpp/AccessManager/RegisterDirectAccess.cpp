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

#include <ATL/ATLLogger.h>

#include <AccessManager/AccessRequest.h>

#define LOG_CONTEXT "RegisterDirectAccess"

#include <AccessManager/RegisterDirectAccess.h>

using namespace std;
using namespace atl;

namespace act {
    const basesize CRegisterDirectAccess::max_request_size = 1024;

    CATLError CRegisterDirectAccess::PostRegRequest(TSmartPtr<CRegRequest> request) {
        CATLError res;
        switch (request->type) {
        case RegRead:
            if (!request->data.empty() && request->mask.empty()) {
                basesize n = (request->data.size()+max_request_size-1)/max_request_size;
                CTransaction tr(request,n);                             // prepare transaction packets
                for (basesize i = 0; i < n; i++) {
                    TSmartPtr<CTransferPacket>& p = tr.packets[i];
                    p->command = EPacketRead;
                    p->address = request->address + (baseaddr)(i*max_request_size);
                    p->data.resize(min(request->data.size()-i*max_request_size,max_request_size));
                    p->listener += &receiver;
                }
                SysLog()->LogDebug(LOG_CONTEXT, "prepared %s request id " FSIZE_T " packet number " FSIZE_T, ERegRequestTypeStr(request->type), request->id, n);
                SendTransaction(tr);                                    // send it
            } else {
                res = EATLErrorInvalidParameters;
                SysLog()->LogError(LOG_CONTEXT, "wrong hardware request parameters");
            }
            break;
        case RegWrite:
            if (!request->data.empty() && (request->mask.empty() || request->mask.size() == request->data.size())) {
                basesize n = (request->data.size()+max_request_size-1)/max_request_size;
                CTransaction tr(request,n);                             // prepare transaction packet
                vector<UInt8>::iterator it = tr.request->data.begin();
                vector<UInt8>::iterator itm = tr.request->mask.begin();
                for (basesize i = 0; i < n; i++) {
                    TSmartPtr<CTransferPacket>& p = tr.packets[i];
                    basesize pack_size = min(request->data.size()-i*max_request_size,max_request_size);
                    p->command = EPacketWrite;
                    p->address = request->address + (baseaddr)(i*max_request_size);
                    copy(it,it+pack_size,back_inserter(p->data));
                    it += pack_size;
                    if (!request->mask.empty()){
                        copy(itm,itm+pack_size,back_inserter(p->mask));
                        itm += pack_size;
                    }
                    p->listener += &receiver;
                }
                SysLog()->LogDebug(LOG_CONTEXT, "prepared %s request id " FSIZE_T " packet number " FSIZE_T, ERegRequestTypeStr(request->type), request->id, n);
                SendTransaction(tr);                                    // send it
            } else {
                res = EATLErrorInvalidParameters;
                SysLog()->LogError(LOG_CONTEXT, "wrong hardware request parameters");
            }
            break;
        case LUTRead:
            if (!request->data.empty() && !(request->data.size() & 3) && !(request->address & 3)) { // LUT access is 4-bytes aligned
                CTransaction tr(request,request->data.size()/4*2);      // prepare enough packets for LUT request
                for (basesize i = 0; i < request->data.size()/4; i++){  // fill indexes and request values
                    TSmartPtr<CTransferPacket>& p_inx = tr.packets[2*i];
                    TSmartPtr<CTransferPacket>& p_data = tr.packets[2*i+1];
                    p_inx->command = EPacketWrite;
                    p_inx->address = request->address;
                    p_inx->data.resize(4);
                    p_inx->data[0] = (UInt8)(i&0xFF); p_inx->data[1] = (UInt8)((i>>8)&0xFF); p_inx->data[2] = (UInt8)((i>>16)&0xFF); p_inx->data[3] = (UInt8)((i>>24)&0xFF);
                    p_inx->listener += &receiver;
                    p_data->command = EPacketRead;
                    p_data->address = request->address + 8;             // read addres equals base + 8
                    p_data->data.resize(4);
                    p_data->listener += &receiver;
                }
                SysLog()->LogDebug(LOG_CONTEXT, "prepared %s request id " FSIZE_T " packet number " FSIZE_T, ERegRequestTypeStr(request->type), request->id, tr.packets.size());
                SendTransaction(tr);                                    // send them all
            } else {
                res = EATLErrorInvalidParameters;
                SysLog()->LogError(LOG_CONTEXT, "wrong hardware request parameters");
            }
            break;
        case LUTWrite:
            if (!request->data.empty() && request->mask.empty() && !(request->data.size() & 3) && !(request->address & 3)) { // LUT access is 4-bytes aligned
                CTransaction tr(request,request->data.size()/4);        // prepare enough packets for request
                vector<UInt8>::iterator it = tr.request->data.begin();
                for (basesize i = 0; i < request->data.size()/4; i++){  // fill indexes and data
                    TSmartPtr<CTransferPacket>& p = tr.packets[i];
                    p->command = EPacketWrite;
                    p->address = request->address;
                    p->data.resize(8);
                    p->data[0] = (UInt8)(i&0xFF);  p->data[1] = (UInt8)((i>>8)&0xFF); p->data[2] = (UInt8)((i>>16)&0xFF); p->data[3] = (UInt8)((i>>24)&0xFF);
                    copy(it,it+4,p->data.begin()+4);
                    p->listener += &receiver;
                    it+=4;
                }
                SysLog()->LogDebug(LOG_CONTEXT, "prepared %s request id " FSIZE_T " packet number " FSIZE_T, ERegRequestTypeStr(request->type), request->id, tr.packets.size());
                SendTransaction(tr);                                    // send them all
            } else {
                res = EATLErrorInvalidParameters;
                SysLog()->LogError(LOG_CONTEXT, "wrong hardware request parameters");
            }
            break;
        default:
            res = EATLErrorUnsupported;
            SysLog()->LogError(LOG_CONTEXT, "unsupported hardware request type '%s'", ERegRequestTypeStr(request->type));
        }
        return res;
    }

    void CRegisterDirectAccess::Configure(TSmartPtr<CBusManager> bus_manager) {
        this->bus_manager = bus_manager;                                // just set correct bus manager
    }

    void CRegisterDirectAccess::PushTransaction(const CTransaction& transaction) {
        armcpp11::LockGuard<armcpp11::Mutex> lock(transactions_mutex);                     // push transaction in the container
        transactions.insert(transactions_container::value_type(transaction.request->id,transaction));
    }

    void CRegisterDirectAccess::SendTransaction(const CTransaction& transaction) {
        PushTransaction(transaction);                                   // first push transaction (request might be thrown immediately)
        for (std::vector<TSmartPtr<CTransferPacket> >::const_iterator it = transaction.packets.begin();
             it != transaction.packets.end(); ++it) {
            if (bus_manager->ProcessRequest(*it).IsError()) {          // send all packets
                break;
            }
        }
    }

    void CRegisterDirectAccess::ProcessTransaction(CTransaction& tr) {
        for (std::vector<TSmartPtr<CTransferPacket> >::const_iterator it = tr.packets.begin();
             it != tr.packets.end(); ++it) { // check that all packets are OK
            if ((*it)->state != EPacketSuccess) {
                tr.request->state = ((*it)->state == EPacketNoAck) ? NoAnswer : Fail;
                SysLog()->LogError(LOG_CONTEXT, "tr id " FSIZE_T " has errors in packet %d: state is %s", tr.request->id, (*it)->state, ERequestStateStr(tr.request->state));
                reg_list.push(tr.request);
                return;
            }
        }
        tr.request->state = Success;                     // transaction is complete
        switch (tr.request->type){
        case RegRead: {                                // for read transaction copy data
            vector<UInt8>::iterator it = tr.request->data.begin();
            for (std::vector<TSmartPtr<CTransferPacket> >::iterator pktIt = tr.packets.begin();
                 pktIt != tr.packets.end(); ++pktIt) {                  // check that all packets are OK
                copy((*pktIt)->data.begin(),(*pktIt)->data.end(),it);
                it += (*pktIt)->data.size();
            }
            break;
        }
        case LUTRead: {                                // for LUT read copy pieces of data
            vector<UInt8>::iterator it = tr.request->data.begin();
            for (basesize i = 0; i < tr.request->data.size()/4; i++){
                vector<UInt8>& data = tr.packets[2*i+1]->data;
                copy(data.begin(),data.end(),it);
                it += 4;
            }
            break;
        }
        default:
            break;
        }
        reg_list.push(tr.request);                                      // push request in output list
        SysLog()->LogDebug(LOG_CONTEXT, "successfully processed transaction id " FSIZE_T, tr.request->id);
    }

    void CRegisterDirectAccess::ProcessReply(void* ptr) {
        TSmartPtr<CTransferPacket> pack = static_cast<CTransferPacket*>(ptr);
        SysLog()->LogDebug(LOG_CONTEXT, "received packet id " FSIZE_T " ref " FSIZE_T, pack->id, pack->reg_id);
        armcpp11::LockGuard<armcpp11::Mutex> lock(transactions_mutex);                     // lock transaction list
        transactions_container::iterator it = transactions.find(pack->reg_id);
        if (it != transactions.end()) {                                 // transaction is found
            bool all_processed = true;                                  // check that all packets in the transaction are processed
            pack->reg_id = 0;                                           // clear reg_id so we can understand that packet is received
            for (std::vector<TSmartPtr<CTransferPacket> >::iterator pktIt = it->second.packets.begin();
                 pktIt != it->second.packets.end(); ++pktIt) {
                if ((*pktIt)->reg_id || (*pktIt)->state == EPacketProcessing) {
                    all_processed = false;
                    break;
                }
            }
            SysLog()->LogDebug(LOG_CONTEXT, "transaction found: %s", all_processed ? "all processed" : "waiting for next packet");
            if (all_processed) {
                ProcessTransaction(it->second);                         // if all packets are processed complete transaction
                transactions.erase(it);                                 // delete transaction from processing transaction list
            }
        } else {    // packet is not found - something strange
            SysLog()->LogWarning(LOG_CONTEXT, "received unwanted packet id " FSIZE_T " ref %u", pack->id, pack->reg_id);
        }
    }
}
