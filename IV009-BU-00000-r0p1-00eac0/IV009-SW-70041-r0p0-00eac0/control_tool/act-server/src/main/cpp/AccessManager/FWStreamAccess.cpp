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

#define LOG_CONTEXT "FWStreamAccess"

#include <AccessManager/FWStreamAccess.h>
using namespace std;
using namespace atl;
using namespace act;

enum TransactionType {
    TransactionTypeRegRead = 1,
    TransactionTypeRegWrite,
    TransactionTypeRegMaskWrite,
    TransactionTypeLUTRead,
    TransactionTypeLUTWrite,

    TransactionTypeAPIRead = 10,
    TransactionTypeAPIWrite,

    TransactionTypeBUFRead = 20,
    TransactionTypeBUFWrite
};

static UInt16 get_transaction_type(EFWRequestType type)
{
    switch (type) {
    case APIRead:  return TransactionTypeAPIRead;
    case APIWrite: return TransactionTypeAPIWrite;
    case BUFRead:  return TransactionTypeBUFRead;
    case BUFWrite: return TransactionTypeBUFWrite;
    default:
        SysLog()->LogError(LOG_CONTEXT, "wrong firmware request type: %s", EFWRequestTypeStr(type));
        return 0xFFFF;
    }
}

static UInt16 get_transaction_type(ERegRequestType type)
{
    switch (type) {
    case RegRead:  return TransactionTypeRegRead;
    case RegWrite: return TransactionTypeRegWrite;
    case LUTRead:  return TransactionTypeLUTRead;
    case LUTWrite: return TransactionTypeLUTWrite;
    default:
        SysLog()->LogError(LOG_CONTEXT, "wrong firmware request type: %s", ERegRequestTypeStr(type));
        return 0xFFFF;
    }
}

enum ReplyStatus {
    Status_SUCCESS = 0,
    Status_NOT_IMPLEMENTED,
    Status_NOT_SUPPORTED,
    Status_NOT_PERMITTED,
    Status_NOT_EXISTS,
    Status_FAIL
};

namespace act {

    void CFWStreamAccess::Configure(TSmartPtr<CBusManager> bus_manager, EStreamType type, baseaddr buf_addr, UInt32 buf_size)
    {
        this->bus_manager = bus_manager;
        stream_type = type;
        if (stream_type == Buffer) {
            buf_mng.Configure(bus_manager, buf_addr, buf_size);
        }
    }

    void CFWStreamAccess::Start()
    {
        if (stream_type == Buffer) {
            buf_mng.Start();
        }
    }

    void CFWStreamAccess::Stop()
    {
        if (stream_type == Buffer) {
            buf_mng.Stop();
        }
    }

    CATLError CFWStreamAccess::PostFWRequest(TSmartPtr<CFWRequest> request)            // post FW (API, BUF) request
    {
        CATLError res;
        if (bus_manager && stream_type != Unknown) {
            TSmartPtr<CTransferPacket> pack;
            switch (request->type)
            {
            case APIRead:
                if (stream_type == Buffer) { // forward API request to buffer manager
                    armcpp11::SharedPtr<CBufferManager::CAPIRequest> req(new CBufferManager::CAPIRequest(request));
                    req->listener += &api_receiver;
                    return buf_mng.ProcessAPIRequest(req);
                }
                else {
                    pack = new CTransferPacket(); // prepare packet
                    pack->reg_id = request->id;
                    pack->listener += &fw_receiver;
                    pack->command = EPacketTransaction;
                    pack->data.resize(header_size + 8);
                    pack->SetValue(get_transaction_type(request->type),8);
                    pack->data[12] = request->cmd_type;
                    pack->data[13] = request->cmd;
                    pack->timeout_ms = request->timeout_ms;
                    pack->SetValue(request->value,16);                    // set Data
                    SysLog()->LogDebug(LOG_CONTEXT, "prepared packet for request %s id " FSIZE_T " size: " FSIZE_T, EFWRequestTypeStr(request->type), request->id, pack->data.size());
                    PushRequest(request); // push request to list, so response might come immediately
                    res = TransferPacket(pack);
                }
                break;
            case APIWrite:
                if (stream_type == Buffer) { // forward API request to buffer manager
                    armcpp11::SharedPtr<CBufferManager::CAPIRequest> req(new CBufferManager::CAPIRequest(request));
                    req->listener += &api_receiver;
                    return buf_mng.ProcessAPIRequest(req);
                }
                else {
                    pack = new CTransferPacket(); // prepare packet
                    pack->reg_id = request->id; pack->listener += &fw_receiver;
                    pack->command = EPacketTransaction;
                    pack->data.resize(header_size + 8);
                    pack->SetValue(get_transaction_type(request->type),8);
                    pack->data[12] = request->cmd_type;
                    pack->data[13] = request->cmd;
                    pack->timeout_ms = request->timeout_ms;
                    pack->SetValue(request->value,16);                    // set Data
                    SysLog()->LogDebug(LOG_CONTEXT, "prepared packet for request %s id " FSIZE_T " size: " FSIZE_T, EFWRequestTypeStr(request->type), request->id, pack->data.size());
                    PushRequest(request); // push request to list, so response might come immediately
                    res = TransferPacket(pack);
                }
                break;
            case BUFRead:
                pack = new CTransferPacket(); // prepare packet
                pack->reg_id = request->id; pack->listener += &fw_receiver;
                pack->command = EPacketTransaction;
                pack->data.resize(header_size + 8);
                pack->SetValue(get_transaction_type(request->type),8);
                pack->data[12] = request->cmd;      // buffer id
                pack->data[13] = request->cmd_type; // cmd_type is buffer class
                pack->timeout_ms = request->timeout_ms;
                pack->SetValue((UInt32)request->data.size(),16);    // set Size
                SysLog()->LogDebug(LOG_CONTEXT, "prepared packet for request %s id " FSIZE_T " size: " FSIZE_T, EFWRequestTypeStr(request->type), request->id, pack->data.size());
                PushRequest(request); // push request to list, so response might come immediately
                res = TransferPacket(pack);
                break;
            case BUFWrite:
                pack = new CTransferPacket(); // prepare packet
                pack->reg_id = request->id; pack->listener += &fw_receiver;
                pack->command = EPacketTransaction;
                pack->data.resize(header_size + 8 + request->data.size());
                pack->SetValue(get_transaction_type(request->type),8);
                pack->data[12] = request->cmd;      // buffer id
                pack->data[13] = request->cmd_type; // cmd_type is buffer class
                pack->timeout_ms = request->timeout_ms;
                pack->SetValue((UInt32)request->data.size(),16);    // set Size
                copy(request->data.begin(),request->data.end(),pack->data.begin()+20);
                SysLog()->LogDebug(LOG_CONTEXT, "prepared packet for request %s id " FSIZE_T " size: " FSIZE_T, EFWRequestTypeStr(request->type), request->id, pack->data.size());
                PushRequest(request); // push request to list, so response might come immediately
                res = TransferPacket(pack);
                break;
            default:
                res = EATLErrorInvalidParameters;
                SysLog()->LogError(LOG_CONTEXT, "wrong firmware request parameters");
            }
        }
        else {
            res = EATLErrorInvalidParameters;
            SysLog()->LogError(LOG_CONTEXT, "firmware stream access is not configured");
        }
        return res;
    }

    void CFWStreamAccess::ProcessReply(TSmartPtr<CFWRequest>& request, TSmartPtr<CTransferPacket>& pack)
    {
        request->state = Fail;
        if (pack->state == EPacketNoAnswer) {
            request->state = Timeout;
        }
        else if (pack->state == EPacketSuccess) {
            switch (request->type)
            {
            case APIWrite:
            case APIRead:
                if (pack->data.size() >= header_size + 8 && pack->GetValue16(8) == get_transaction_type(request->type)) {
                    request->status = pack->GetValue16(12);
                    request->value = pack->GetValue32(16);
                    request->state = Success;
                    SysLog()->LogDebug(LOG_CONTEXT, "successful processing for request %s id " FSIZE_T, EFWRequestTypeStr(request->type), request->id);
                }
                else {
                    SysLog()->LogError(LOG_CONTEXT, "malformed reply for %s firmware request id " FSIZE_T, EFWRequestTypeStr(request->type), request->id);
                }
                break;
            case BUFRead:
                if (pack->data.size() >= header_size + 4 && pack->GetValue16(8) == get_transaction_type(request->type)) {
                    request->status = pack->GetValue16(12);
                    if (request->status == Status_SUCCESS) { // copy data only if status is success
                        if (pack->data.size() >= header_size + 4 + request->data.size()) {
                            request->state = Success;
                            copy(pack->data.begin()+16,pack->data.begin()+16+request->data.size(),request->data.begin());
                        }
                        else {
                            SysLog()->LogError(LOG_CONTEXT, "wrong firmware reply size " FSIZE_T " for %s request id " FSIZE_T ": expected size " FSIZE_T, pack->data.size(), EFWRequestTypeStr(request->type), request->id, header_size + 4 + request->data.size());
                        }
                    }
                    else {
                        request->state = Success;
                    }
                    SysLog()->LogDebug(LOG_CONTEXT, "Successful processing for request %s id " FSIZE_T, EFWRequestTypeStr(request->type), request->id);
                }
                else {
                    SysLog()->LogError(LOG_CONTEXT, "wrong firmware reply for %s request id " FSIZE_T, EFWRequestTypeStr(request->type), request->id);
                }
                break;
            case BUFWrite:
                if (pack->data.size() >= header_size + 4 && pack->GetValue16(8) == get_transaction_type(request->type)) {
                    request->status = pack->GetValue16(12);
                    request->state = Success;
                    SysLog()->LogDebug(LOG_CONTEXT, "successful processing for request %s id " FSIZE_T, EFWRequestTypeStr(request->type), request->id);
                }
                else {
                    SysLog()->LogError(LOG_CONTEXT, "wrong firmware reply for %s request id " FSIZE_T, EFWRequestTypeStr(request->type), request->id);
                }
                break;
            default:
                SysLog()->LogError(LOG_CONTEXT, "wrong firmware request parameters");
            }
        }
        fw_list.push(request);
    }

    CATLError CFWStreamAccess::PostRegRequest(TSmartPtr<CRegRequest> request)        // post Register and LUT request
    {
        CATLError res;
        if (bus_manager && stream_type != Unknown) {
            switch (request->type)
            {
            case RegRead:
                if (!request->data.empty() && request->mask.empty()) {
                    TSmartPtr<CTransferPacket> pack = new CTransferPacket(); // prepare packet
                    pack->reg_id = request->id; pack->listener += &reg_receiver;
                    pack->command = EPacketTransaction;
                    pack->data.resize(header_size + 8);
                    pack->SetValue(get_transaction_type(request->type),8);
                    pack->SetValue(request->address,12);                    // set Address
                    pack->SetValue((UInt32)request->data.size(),16);    // set Size
                    SysLog()->LogDebug(LOG_CONTEXT, "prepared packet for request %s id " FSIZE_T " size: " FSIZE_T, ERegRequestTypeStr(request->type), request->id, pack->data.size());
                    PushRequest(request); // push request to list, so response might come immediately
                    res = TransferPacket(pack);
                }
                else {
                    res = EATLErrorInvalidParameters;
                    SysLog()->LogError(LOG_CONTEXT, "wrong read register request parameters");
                }
                break;
            case RegWrite:
                if (!request->data.empty() && (request->mask.empty() || request->mask.size() == request->data.size())) {
                    TSmartPtr<CTransferPacket> pack = new CTransferPacket(); // prepare packet
                    pack->reg_id = request->id; pack->listener += &reg_receiver;
                    pack->command = EPacketTransaction;
                    pack->data.resize(header_size + 8 + request->data.size() + request->mask.size());
                    pack->SetValue((UInt16)(get_transaction_type(request->type) + !request->mask.empty()),8);
                    pack->SetValue(request->address,12);                    // set Address
                    pack->SetValue((UInt32)request->data.size(),16);    // set Size
                    copy(request->data.begin(),request->data.end(),pack->data.begin()+20);
                    copy(request->mask.begin(),request->mask.end(),pack->data.begin()+20+request->data.size());
                    SysLog()->LogDebug(LOG_CONTEXT, "prepared packet for request %s id " FSIZE_T " size: " FSIZE_T, ERegRequestTypeStr(request->type), request->id, pack->data.size());
                    PushRequest(request); // push request to list, so response might come immediately
                    res = TransferPacket(pack);
                }
                else {
                    res = EATLErrorInvalidParameters;
                    SysLog()->LogError(LOG_CONTEXT, "wrong write register request parameters");
                }
                break;
            case LUTRead:
                if (!request->data.empty() && !(request->data.size() & 3) && !(request->address & 3)) { // LUT access is 4-bytes aligned
                    TSmartPtr<CTransferPacket> pack = new CTransferPacket(); // prepare packet
                    pack->reg_id = request->id; pack->listener += &reg_receiver;
                    pack->command = EPacketTransaction;
                    pack->data.resize(header_size + 8);
                    pack->SetValue(get_transaction_type(request->type),8);
                    pack->SetValue(request->address,12);                    // set Address
                    pack->SetValue((UInt32)request->data.size(),16);    // set Size
                    SysLog()->LogDebug(LOG_CONTEXT, "prepared packet for request %s id " FSIZE_T " size: " FSIZE_T, ERegRequestTypeStr(request->type), request->id, pack->data.size());
                    PushRequest(request); // push request to list, so response might come immediately
                    res = TransferPacket(pack);
                }
                else {
                    res = EATLErrorInvalidParameters;
                    SysLog()->LogError(LOG_CONTEXT, "wrong read array request parameters");
                }
                break;
            case LUTWrite:
                if (!request->data.empty() && request->mask.empty() && !(request->data.size() & 3) && !(request->address & 3)) { // LUT access is 4-bytes aligned
                    TSmartPtr<CTransferPacket> pack = new CTransferPacket(); // prepare packet
                    pack->reg_id = request->id; pack->listener += &reg_receiver;
                    pack->command = EPacketTransaction;
                    pack->data.resize(header_size + 8 + request->data.size());
                    pack->SetValue(get_transaction_type(request->type),8);
                    pack->SetValue(request->address,12);                    // set Address
                    pack->SetValue((UInt32)request->data.size(),16);    // set Size
                    copy(request->data.begin(),request->data.end(),pack->data.begin()+20);
                    SysLog()->LogDebug(LOG_CONTEXT, "prepared packet for request %s id " FSIZE_T " size: " FSIZE_T, ERegRequestTypeStr(request->type), request->id, pack->data.size());
                    PushRequest(request); // push request to list, so response might come immediately
                    res = TransferPacket(pack);
                }
                else {
                    res = EATLErrorInvalidParameters;
                    SysLog()->LogError(LOG_CONTEXT, "wrong write array request parameters");
                }
                break;
            default:
                res = EATLErrorInvalidParameters;
                SysLog()->LogError(LOG_CONTEXT, "wrong hardware request parameters");
            }
        }
        else {
            res = EATLErrorInvalidParameters;
            SysLog()->LogError(LOG_CONTEXT, "access to hardware through firmware stream is not configured");
        }
        return res;
    }

    void CFWStreamAccess::ProcessReply(TSmartPtr<CRegRequest>& request, TSmartPtr<CTransferPacket>& pack)
    {
        request->state = Fail;
        if (pack->state == EPacketNoAnswer) {
            request->state = Timeout;
        }
        else if (pack->state == EPacketSuccess) {
            switch (request->type)
            {
            case LUTRead:
            case RegRead:
                if (pack->data.size() >= header_size + 4 + request->data.size() && pack->GetValue16(8) == get_transaction_type(request->type) && pack->GetValue16(12) == Status_SUCCESS) {
                    copy(pack->data.begin()+16,pack->data.begin()+16+request->data.size(),request->data.begin());
                    request->state = Success;
                    SysLog()->LogDebug(LOG_CONTEXT, "successful processing for request %s id " FSIZE_T, ERegRequestTypeStr(request->type), request->id);
                }
                else {
                    SysLog()->LogError(LOG_CONTEXT, "wrong reply for %s hardware request id " FSIZE_T, ERegRequestTypeStr(request->type), request->id);
                }
                break;
            case RegWrite:
            case LUTWrite:
                if (pack->data.size() >= header_size + 4 && pack->GetValue16(8) == get_transaction_type(request->type) + !request->mask.empty() && pack->GetValue16(12) == Status_SUCCESS) {
                    request->state = Success;
                    SysLog()->LogDebug(LOG_CONTEXT, "successful processing for request %s id " FSIZE_T, ERegRequestTypeStr(request->type), request->id);
                }
                else {
                    SysLog()->LogError(LOG_CONTEXT, "wrong reply for %s hardware request id " FSIZE_T, ERegRequestTypeStr(request->type), request->id);
                }
                break;
            default:
                SysLog()->LogError(LOG_CONTEXT, "wrong hardware request parameters");
            }
        }
        reg_list.push(request);
    }

    CATLError CFWStreamAccess::TransferPacket(TSmartPtr<CTransferPacket>& pack)
    {
        switch (stream_type) {
            case Stream: return bus_manager->ProcessRequest(pack);
            case Buffer: return buf_mng.TransferPacket(pack);
            default: return EATLErrorInvalidParameters;
        }
    }

    void CFWStreamAccess::PushRequest(TSmartPtr<CRegRequest>& request)
    {
        armcpp11::LockGuard<armcpp11::Mutex> lock(reg_mutex);
        reg_req_list.insert(map<baseid,TSmartPtr<CRegRequest> >::value_type(request->id,request));    // insert new item
    }

    void CFWStreamAccess::PushRequest(TSmartPtr<CFWRequest>& request)
    {
        armcpp11::LockGuard<armcpp11::Mutex> lock(fw_mutex);
        fw_req_list.insert(map<baseid,TSmartPtr<CFWRequest> >::value_type(request->id,request));    // insert new item
    }

    void CFWStreamAccess::ProcessRegReply(void* ptr)
    {
        TSmartPtr<CTransferPacket> pack = static_cast<CTransferPacket*>(ptr);
        armcpp11::UniqueLock<armcpp11::Mutex> lock(reg_mutex);
        std::map<baseid,TSmartPtr<CRegRequest> >::iterator it = reg_req_list.find(pack->reg_id);
        if (it != reg_req_list.end()){
            TSmartPtr<CRegRequest> request = it->second;    // store result
            reg_req_list.erase(it);    // remove from the list
            lock.Unlock(); // unlock list
            SysLog()->LogDebug(LOG_CONTEXT, "received reply on packet for request %s id " FSIZE_T " size: " FSIZE_T, ERegRequestTypeStr(request->type), request->id, pack->data.size());
            ProcessReply(request,pack);
        }
        else {
            lock.Unlock(); // unlock list
            SysLog()->LogError(LOG_CONTEXT, "can't find request with id " FSIZE_T, pack->reg_id);
        }
    }

    void CFWStreamAccess::ProcessFWReply(void* ptr)
    {
        TSmartPtr<CTransferPacket> pack = static_cast<CTransferPacket*>(ptr);
        armcpp11::UniqueLock<armcpp11::Mutex> lock(fw_mutex);
        std::map<baseid,TSmartPtr<CFWRequest> >::iterator it = fw_req_list.find(pack->reg_id);
        if (it != fw_req_list.end()){
            TSmartPtr<CFWRequest> request = it->second;    // store result
            fw_req_list.erase(it);    // remove from the list
            lock.Unlock(); // unlock list
            SysLog()->LogDebug(LOG_CONTEXT, "received reply on packet for request %s id " FSIZE_T " size: " FSIZE_T, EFWRequestTypeStr(request->type), request->id, pack->data.size());
            ProcessReply(request,pack);
        }
        else {
            lock.Unlock(); // unlock list
            SysLog()->LogError(LOG_CONTEXT, "can't find request with id " FSIZE_T, pack->reg_id);
        }
    }

    void CFWStreamAccess::ProcessAPIReply(void* ptr)
    {
        TSmartPtr<CFWRequest> request = static_cast<CFWRequest*>(ptr);
        SysLog()->LogDebug(LOG_CONTEXT, "Received reply on for API request %s id " FSIZE_T, EFWRequestTypeStr(request->type), request->id);
        fw_list.push(request);
    }
}
