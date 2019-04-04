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

#define LOG_CONTEXT "FWDebugAccess"

#include <AccessManager/FWDebugAccess.h>

using namespace std;
using namespace atl;

// this is commands for FW
enum {
    COMMAND_RESET     = 0,
    COMMAND_SET_TYPE  = 1,
    COMMAND_SET_ID    = 2,
    COMMAND_SET_DIR   = 3,
    COMMAND_SET_VALUE = 4,
    COMMAND_RUN       = 5,
    COMMAND_DONE      = 6,
    COMMAND_BUF_SIZE  = 7,
    COMMAND_BUF_ERR   = 8,
    COMMAND_BUF_SET   = 9,
    COMMAND_BUF_GET   = 10,
    COMMAND_BUF_APPLY = 11,
    COMMAND_API_GET   = 12,
    COMMAND_API_SET   = 13
};

const char* fwCommandToString (const UInt8& command) {
    switch (command) {
        case COMMAND_RESET:     return "RESET";
        case COMMAND_SET_TYPE:  return "SET SECTION";
        case COMMAND_SET_ID:    return "SET COMMAND";
        case COMMAND_SET_DIR:   return "SET DIRECTION";
        case COMMAND_SET_VALUE: return "SET VALUE";
        case COMMAND_RUN:       return "RUN";
        case COMMAND_DONE:      return "DONE";
        case COMMAND_BUF_SIZE:  return "BUF SIZE";
        case COMMAND_BUF_ERR:   return "BUF ERROR";
        case COMMAND_BUF_SET:   return "BUF SET";
        case COMMAND_BUF_GET:   return "BUF GET";
        case COMMAND_BUF_APPLY: return "BUF APPLY";
        case COMMAND_API_GET:   return "API GET";
        case COMMAND_API_SET:   return "API SET";
        default: return "unknown";
    }
}

enum {
    STATUS_SUCCESS = 0,
    STATUS_NOT_IMPLEMENTED,
    STATUS_NOT_SUPPORTED,
    STATUS_NOT_PERMITTED,
    STATUS_FAIL
};

namespace act {

    void CFWDebugAccess::start_thread()
    {
        stop_thread();
        stop_flag = false;                  // initialy running flag
        working_thread = atl::thread(&CFWDebugAccess::work_wrapper, this); // create the working thread
    }

    void CFWDebugAccess::stop_thread()
    {
        if (working_thread.Joinable()) {    // close working thread on destuction if required
            stop_flag = true;
            cv.NotifyAll();
            working_thread.Join();
        }
    }

    void CFWDebugAccess::Configure(TSmartPtr<CBusManager> bus_manager, baseaddr dbg_addr)
    {
        if (working_thread.Joinable()) {    // close working thread on reconfiguring
            SysLog()->LogWarning(LOG_CONTEXT, "reconfiguring already configured module");
            stop_thread();
        }
        this->bus_manager = bus_manager;    // set up bus manager
        this->dbg_addr = dbg_addr;          // set up debug address
    }

    CATLError CFWDebugAccess::PostFWRequest(TSmartPtr<CFWRequest> request)
    {
        CATLError res;
        if (!bus_manager || request->type == FWUnknown) { // validate
            res = EATLErrorInvalidParameters;
            SysLog()->LogError(LOG_CONTEXT, "wrong firmware access request: type %s", EFWRequestTypeStr(request->type));
        } else {
            if (!request->data.empty()) {    // if data is not empty reserve some memory cause all transactions are 4-bytes
                request->data.reserve((request->data.size()+3)&~3); // don't need to worry about memory for the end of buffer
            }
            bool request_posted = false;
            while (!request_posted) {
                armcpp11::UniqueLock<armcpp11::Mutex> lock(mtx); // lock mutex to put request in the queue
                if (reqest_queue.size() >= max_request_num) {    // if no space, try later
                    lock.Unlock();
                    SysLog()->LogWarning(LOG_CONTEXT, "too many requests in queue: waiting some time...");
                    armcpp11::ThisThread::SleepFor(armcpp11::MilliSeconds(10)); // 10 ms
                } else {
                    reqest_queue.push_back(request);    // push request in the queue
                    lock.Unlock();          // now can unlock the mutex
                    cv.NotifyAll();        // notify working thread
                    request_posted = true;
                    SysLog()->LogDebug(LOG_CONTEXT, "request " FSIZE_T " has been put into queue", request->id);
                }
            }
        }
        return res;
    }

    // set data to debug register 1
    bool CFWDebugAccess::send_data(UInt8 data, baseid id)
    {
        SysLog()->LogDebug(LOG_CONTEXT, "sending data %d", data);
        TSmartPtr<CTransferPacket> pack = new CTransferPacket;
        pack->address = dbg_addr + 1;
        pack->data = vector<UInt8>(1);
        pack->data[0] = data;
        pack->command = EPacketWrite;
        pack->listener += &receiver;
        pack->reg_id = id;
        if (bus_manager->ProcessRequest(pack).IsError()) {
            SysLog()->LogError(LOG_CONTEXT, "firmware communication error");
            drop_transaction();
            return false;
        }
        return true;
    }

    // set index and buffer value
    bool CFWDebugAccess::send_buf_inx(UInt16 inx, const UInt8* data, baseid id)
    {
        SysLog()->LogDebug(LOG_CONTEXT, "sending buffer %02X%02X%02X%02X inx %d", data[3], data[2], data[1], data[0], inx);
        TSmartPtr<CTransferPacket> pack = new CTransferPacket;
        pack->address = dbg_addr + 1;
        pack->data = vector<UInt8>(6);
        pack->data[0] = data[3];
        pack->data[1] = data[2];
        pack->data[2] = data[1];
        pack->data[3] = data[0];
        pack->data[4] = (UInt8)((inx>>8)&0xFF);
        pack->data[5] = (UInt8)(inx&0xFF);
        pack->command = EPacketWrite;
        pack->listener += &receiver;
        pack->reg_id = id;
        if (bus_manager->ProcessRequest(pack).IsError()) {
            SysLog()->LogError(LOG_CONTEXT, "firmware communication error");
            drop_transaction();
            return false;
        }
        return true;
    }

    // set index only
    bool CFWDebugAccess::send_inx(UInt16 inx, baseid id)
    {
        SysLog()->LogDebug(LOG_CONTEXT, "sending index %u", inx);
        TSmartPtr<CTransferPacket> pack = new CTransferPacket;
        pack->address = dbg_addr + 1 + 4;
        pack->data = vector<UInt8>(2);
        pack->data[0] = (UInt8)((inx>>8)&0xFF);
        pack->data[1] = (UInt8)(inx&0xFF);
        pack->command = EPacketWrite;
        pack->listener += &receiver;
        pack->reg_id = id;
        if (bus_manager->ProcessRequest(pack).IsError()) {
            SysLog()->LogError(LOG_CONTEXT, "firmware communication error");
            drop_transaction();
            return false;
        }
        return true;
    }

    // set value
    bool CFWDebugAccess::send_value(UInt32 val, baseid id)
    {
        SysLog()->LogDebug(LOG_CONTEXT, "sending value %u", val);
        TSmartPtr<CTransferPacket> pack = new CTransferPacket;
        pack->address = dbg_addr + 1;
        pack->data = vector<UInt8>(4);
        pack->data[0] = (UInt8)((val>>24)&0xFF);
        pack->data[1] = (UInt8)((val>>16)&0xFF);
        pack->data[2] = (UInt8)((val>>8)&0xFF);
        pack->data[3] = (UInt8)(val&0xFF);
        pack->command = EPacketWrite;
        pack->listener += &receiver;
        pack->reg_id = id;
        if (bus_manager->ProcessRequest(pack).IsError()) {
            SysLog()->LogError(LOG_CONTEXT, "firmware communication error");
            drop_transaction();
            return false;
        }
        return true;
    }

    // send full API
    bool CFWDebugAccess::send_full_api(UInt8 type, UInt8 cmd, UInt32 value, baseid id)
    {
        SysLog()->LogDebug(LOG_CONTEXT, "sending full request type (section): %u id (command): %u value: %u", type, cmd, value);
        TSmartPtr<CTransferPacket> pack = new CTransferPacket;
        pack->address = dbg_addr + 1;
        pack->data = vector<UInt8>(6);
        pack->data[0] = (UInt8)((value>>24)&0xFF);
        pack->data[1] = (UInt8)((value>>16)&0xFF);
        pack->data[2] = (UInt8)((value>>8)&0xFF);
        pack->data[3] = (UInt8)(value&0xFF);
        pack->data[4] = type;   // section
        pack->data[5] = cmd;    // command
        pack->command = EPacketWrite;
        pack->listener += &receiver;
        pack->reg_id = id;
        if (bus_manager->ProcessRequest(pack).IsError()) {
            SysLog()->LogError(LOG_CONTEXT, "firmware communication error");
            drop_transaction();
            return false;
        }
        return true;
    }

    // set command
    bool CFWDebugAccess::send_command(UInt8 cmd, baseid id)
    {
        SysLog()->LogDebug(LOG_CONTEXT, "sending command %s", fwCommandToString(cmd));
        TSmartPtr<CTransferPacket> pack = new CTransferPacket;
        pack->address = dbg_addr;
        pack->data = vector<UInt8>(1);
        pack->data[0] = cmd;
        pack->command = EPacketWrite;
        pack->listener += &receiver;
        pack->reg_id = id;
        if (bus_manager->ProcessRequest(pack).IsError()) {
            SysLog()->LogError(LOG_CONTEXT, "firmware communication error");
            drop_transaction();
            return false;
        }
        return true;
    }

    // request status
    bool CFWDebugAccess::request_status(baseid id, basesize num, basesize buffer_size)
    {
        cnt_timeout = buffer_size;
        SysLog()->LogDebug(LOG_CONTEXT, "request status buffer size: %u", num);
        TSmartPtr<CTransferPacket> pack = new CTransferPacket;
        pack->address = dbg_addr;
        pack->data = vector<UInt8>(num);
        pack->command = EPacketRead;
        pack->listener += &receiver;
        pack->reg_id = id;
        if (bus_manager->ProcessRequest(pack).IsError()) {
            SysLog()->LogError(LOG_CONTEXT, "firmware communication error");
            drop_transaction();
            return false;
        }
        return true;
    }

    // drop current transaction
    void CFWDebugAccess::drop_transaction(ERequestState tr_state)
    {
        request->state = tr_state;
        fw_list.push(request);
        state = WaitRequest;
        cnt_timeout = max_wait_packets;
        buf_inx = 0;
    }

    void * CFWDebugAccess::work_wrapper(void * context) {
        ((CFWDebugAccess*)context)->work();
        return NULL;
    }

    // working thread
    void CFWDebugAccess::work()
    {
        enum APIAccess {Unchecked, Slow, Fast} api_acc = Unchecked;
        state = WaitRequest;        // initialize all required variables
        cnt_timeout = max_wait_packets;
        buf_inx = 0;
        TSmartPtr<CTransferPacket> pack;
        SysLog()->LogDebug(LOG_CONTEXT, "working thread has started");
        while(!stop_flag)    // do your job until we will stop you
        {
            armcpp11::UniqueLock<armcpp11::Mutex> lock(mtx);    // lock the mutex every time
            if (!stop_flag && ((state == WaitRequest && reqest_queue.empty()) || (state != WaitRequest && received_packets.empty()))) { // nothing to do, wait for activities
                cv.Wait(lock);    // wait for activities
            }
            if (!stop_flag) {   // if still need to work
                bool command_responded = false;
                while (!stop_flag && state != WaitRequest && !received_packets.empty()) {    // process responses
                    pack = received_packets.front();
                    received_packets.pop_front();
                    if (pack->state != EPacketSuccess) {    // check status of all income packets
                        SysLog()->LogDebug(LOG_CONTEXT, "error in response from firmware");
                        drop_transaction();
                    } else if (pack->reg_id == request->id && pack->command == EPacketRead) {    // read command reply received
                        if (pack->address == dbg_addr && (pack->data.size() == 1 || pack->data.size() == 2 || pack->data.size() == 6)) { // if data status command
                            SysLog()->LogDebug(LOG_CONTEXT, "received response with status %s", fwCommandToString(pack->data[0]));
                            if (pack->data[0] == COMMAND_DONE) {
                                lock.Unlock();
                                command_responded = true; // correct answer
                                break;
                            } else if (pack->data[0] == COMMAND_BUF_ERR) {    // buffer error
                                SysLog()->LogError(LOG_CONTEXT, "firmware has rejected buffer transaction");
                                drop_transaction();
                            } else if (!cnt_timeout) {    // timeouted
                                SysLog()->LogError(LOG_CONTEXT, "firmware response timeout");
                                drop_transaction(Timeout);
                            } else {    // repeat request
                                lock.Unlock(); // unlock mutex before waiting
                                armcpp11::ThisThread::SleepFor(armcpp11::MilliSeconds(2)); // 2 ms // give some time to FW for command processing
                                if (!request_status(request->id, pack->data.size(), cnt_timeout-1)) {    // repeat request and reduce timeout
                                    lock.Lock();    // has to be locked in WaitRequest state
                                }
                                break;
                            }
                        } else {
                            SysLog()->LogError(LOG_CONTEXT, "unexpected response from firmware");
                        }
                    }
                }
                if (state != WaitRequest && !command_responded)    // no response, go and wait for next
                    continue;
                switch (state) {
                case WaitRequest:
                    if (!reqest_queue.empty()) {
                        request = reqest_queue.front();    // get request from the queue
                        reqest_queue.pop_front();
                        received_packets.clear();        // clear receiver queue
                        lock.Unlock();
                        SysLog()->LogDebug(LOG_CONTEXT, "started to process request %s id " FSIZE_T, EFWRequestTypeStr(request->type), request->id);
                        if (api_acc == Fast && (request->type==APIRead || request->type==APIWrite)) {
                            if (send_full_api(request->cmd_type,request->cmd,request->value,request->id) &&
                                send_command(request->type==APIRead ? COMMAND_API_GET : COMMAND_API_SET,request->id) && request_status(request->id, 6)) {    // send reset command
                                state = Run; // go directly to Run
                            }
                        } else {
                            if (send_command(COMMAND_RESET,request->id) && request_status(request->id,2)) {    // send reset command
                                state = Reset;
                            }
                        }
                    }
                    break;
                case Reset:
                    SysLog()->LogDebug(LOG_CONTEXT, "request " FSIZE_T " has been reset", request->id);
                    if (api_acc == Unchecked) {
                        api_acc = pack->data[1] != 0xFF ? Slow : Fast;
                        SysLog()->LogDebug(LOG_CONTEXT, "detected %s version of firmware api ", api_acc == Fast ? "FAST" : "SLOW");
                        if (api_acc == Fast && (request->type==APIRead || request->type==APIWrite)) { // for Fast access API send full command directly
                            if (send_full_api(request->cmd_type,request->cmd,request->value,request->id) && send_command(request->type==APIRead ? COMMAND_API_GET : COMMAND_API_SET,request->id) && request_status(request->id,6)) {    // send reset command
                                state = Run; // go directly to Run
                            }
                            break;
                        }
                    }
                    if (send_data(request->type == APIRead || request->type == BUFRead ? COMMAND_GET : COMMAND_SET, request->id) && send_command(COMMAND_SET_DIR,request->id) && request_status(request->id)) {
                        state = SetDir;
                    }
                    break;
                case SetDir:
                    SysLog()->LogDebug(LOG_CONTEXT, "request " FSIZE_T " diriection set", request->id);
                    if (send_data(request->cmd_type,request->id) && send_command(COMMAND_SET_TYPE,request->id) && request_status(request->id)) {
                        state = SetType;
                    }
                    break;
                case SetType:
                    SysLog()->LogDebug(LOG_CONTEXT, "request " FSIZE_T " type (section) set", request->id);
                    if (send_data(request->cmd,request->id) && send_command(COMMAND_SET_ID,request->id) && request_status(request->id)) { // go to ID stage
                        state = SetId;
                    }
                    break;
                case SetId:
                    SysLog()->LogDebug(LOG_CONTEXT, "request " FSIZE_T " id (command) set", request->id);
                    switch (request->type) {
                    case APIRead:
                        if (send_command(COMMAND_RUN,request->id) && request_status(request->id,6,max_wait_packets*8)) { // Command RUN might take some time for execution
                            state = Run;
                        }
                        break;
                    case APIWrite:
                        if (send_value(request->value,request->id) && send_command(COMMAND_SET_VALUE,request->id) && request_status(request->id)) { // for write commands set value first
                            state = SetValue;
                        }
                        break;
                    case BUFRead:
                    case BUFWrite:
                        if (send_value((UInt32)request->data.size(),request->id) && send_command(COMMAND_BUF_SIZE,request->id) && request_status(request->id)) { // go to buffer size stage
                            state = BufSize;
                        }
                        break;
                    default:
                        SysLog()->LogError(LOG_CONTEXT, "wrong request type");
                        drop_transaction();
                        break;
                    }
                    break;
                case SetValue:
                    SysLog()->LogDebug(LOG_CONTEXT, "request " FSIZE_T " value set", request->id);
                    if (send_command(COMMAND_RUN,request->id) && request_status(request->id,6,max_wait_packets*8)) { // Command RUN might take some time for execution
                        state = Run;
                    }
                    break;
                case Run:
                    SysLog()->LogDebug(LOG_CONTEXT, "request " FSIZE_T " received result", request->id);
                    request->value = ((UInt32)pack->data[1]<<24) | ((UInt32)pack->data[2]<<16) | ((UInt32)pack->data[3]<<8) | pack->data[4]; // process value
                    request->status = pack->data[5];            // process status
                    request->state = Success;    // state = OK
                    fw_list.push(request);                        // push processed request to output list
                    state = WaitRequest;                    // restart state machine
                    SysLog()->LogDebug(LOG_CONTEXT, "request " FSIZE_T " successfully processed", request->id);
                    break;
                case BufSize:
                    SysLog()->LogDebug(LOG_CONTEXT, "request " FSIZE_T " buffer size set", request->id);
                    if (request->type == BUFRead) {
                        if (send_command(COMMAND_BUF_APPLY,request->id) && request_status(request->id,6,max_wait_packets*8)) { // Command BUF_APPLY might take some time for execution
                            state = BufApply;
                        }
                        break;
                    } else {
                        buf_inx = 0;
                        if (send_buf_inx(buf_inx,request->data.data(),request->id) && send_command(COMMAND_BUF_SET,request->id) && request_status(request->id)) { // set first buffer item
                            state = BufSet;
                        }
                        break;
                    }
                    break;
                case BufGet:
                {
                    SysLog()->LogDebug(LOG_CONTEXT, "request " FSIZE_T " for buffer received index %d", request->id, buf_inx);
                    basesize array_inx = (basesize)buf_inx<<2;
                    for (basesize i = array_inx; i < min(request->data.size(),array_inx+4); i++) {    // copy result to output buffer
                        request->data[i] = pack->data[4-(i-array_inx)];
                    }
                    if (++buf_inx == ((request->data.size()+3)>>2)) { // last packet has been gotten
                        // reading is finished here
                        request->state = Success;    // state = OK
                        fw_list.push(request);                // push the result to output queue
                        state = WaitRequest;            // reset state machine
                        SysLog()->LogDebug(LOG_CONTEXT, "request " FSIZE_T " successfully processed", request->id);
                    } else if (send_inx(buf_inx,request->id) && send_command(COMMAND_BUF_GET,request->id) && request_status(request->id,6)) {    // request sequential buffer item
                        // Do not change state
                    }
                    break;
                }
                case BufSet:
                    SysLog()->LogDebug(LOG_CONTEXT, "request " FSIZE_T " buffer set index %d", request->id, buf_inx);
                    if (++buf_inx >= ((request->data.size()+3)>>2)) { // all items were set
                        if (send_command(COMMAND_BUF_APPLY,request->id) && request_status(request->id,6,max_wait_packets*8)) { // Command BUF_APPLY might take some time for execution
                            state = BufApply;
                        }
                    } else if (send_buf_inx(buf_inx,request->data.data()+((basesize)buf_inx<<2),request->id) && send_command(COMMAND_BUF_SET,request->id) && request_status(request->id)) { // set next buffer item
                        // Do not change state
                    }
                    break;
                case BufApply:
                    SysLog()->LogDebug(LOG_CONTEXT, "request " FSIZE_T " buffer applied", request->id);
                    request->value = ((UInt32)pack->data[1]<<24) | ((UInt32)pack->data[2]<<16) | ((UInt32)pack->data[3]<<8) | pack->data[4];    // process the value
                    request->status = pack->data[5];    // get the status
                    if (request->type == BUFRead) {
                        if (request->status == STATUS_SUCCESS) {    // if buffer can be read
                            buf_inx = 0;
                            if (send_inx(buf_inx,request->id) && send_command(COMMAND_BUF_GET,request->id) && request_status(request->id,6)) { // request first buffer item
                                state = BufGet;
                            }
                        } else {
                            SysLog()->LogWarning(LOG_CONTEXT, "firmware can't return buffer for request id " FSIZE_T, request->id);
                            drop_transaction(NoAnswer);
                        }
                    } else {
                        // writing is finished here
                        request->state = Success;    // state = OK
                        fw_list.push(request);        // push request in output list
                        state = WaitRequest;    // reset state machine
                        SysLog()->LogDebug(LOG_CONTEXT, "request " FSIZE_T " successfully processed", request->id);
                    }
                    break;
                default:
                    SysLog()->LogError(LOG_CONTEXT, "wrong machine state");
                    state = WaitRequest;    // reset state machine
                    received_packets.clear();
                    break;
                }
            }
        }
        SysLog()->LogDebug(LOG_CONTEXT, "working thread has stopped");
    }

    void CFWDebugAccess::ProcessReply(void* ptr)
    {
        armcpp11::UniqueLock<armcpp11::Mutex> lock(mtx);
        received_packets.push_back(static_cast<CTransferPacket*>(ptr));
        lock.Unlock();
        cv.NotifyAll();
    }
}
