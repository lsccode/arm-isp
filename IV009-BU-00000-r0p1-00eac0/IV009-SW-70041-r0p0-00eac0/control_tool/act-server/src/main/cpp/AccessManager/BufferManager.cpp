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

#define LOG_CONTEXT "BufferManager"

#include <AccessManager/BufferManager.h>

using namespace atl;

enum {
    FW_IS_RESET = 0,
    CHANNEL_REQUESTED,
    CHANNEL_ACKNOWLEDGE,
    WRITE_REGISTER_UPDATED,
    READ_REGISTER_UPDATED
};

enum {
    API_RESET = 0,
    API_READ,
    API_WRITE
};

namespace act {

    static const UInt32 val_err = (UInt32)-1;
    static inline UInt32 get_buffer_value(const TSmartPtr<CTransferPacket>& packet, basesize offset, bool check = true)
    {
        UInt32 val = packet->GetValue32(offset);
        if (check && (val & 0xFFFF) != (val >> 16)) return val_err;
        else return val & 0xFFFF;
    }

    CBufferManager::~CBufferManager() {
        Stop();
    }

    void CBufferManager::Configure(TSmartPtr<CBusManager> bus_manager, baseaddr buf_addr, UInt32 buf_size)
    {
        if (thread_stop) {
            stop_threads();
        }

        rx_received_packets.clear();
        tx_received_packets.clear();
        api_received_packets.clear();
        tx_reqest_queue.clear();
        rx_reqest_list.clear();
        api_reqest_queue.clear();

        this->bus_manager = bus_manager;
        this->buf_size = buf_size>>1;
        api_tx_addr = buf_addr;
        api_rx_addr = buf_addr + this->buf_size;
        buf_tx_addr = buf_addr + api_size;
        buf_rx_addr = buf_addr + this->buf_size + api_size;
        buf_data_size = this->buf_size - data_offset - api_size;

        SysLog()->LogInfo(LOG_CONTEXT, "configuration is successfull: address: 0x%X size: " FSIZE_T, buf_addr, buf_size);
    }

    void CBufferManager::Start() {
        thread_stop = true;
        create_threads();
    }

    void CBufferManager::Stop() {
        thread_stop = true;
        stop_threads();
    }

    CATLError CBufferManager::TransferPacket(TSmartPtr<CTransferPacket>& packet)
    {
        CATLError result;
        if (packet->command == EPacketTransaction && packet->data.size() > 8){
            SysLog()->LogDebug(LOG_CONTEXT, "started processing for transaction id: " FSIZE_T " size: " FSIZE_T,packet->id, packet->data.size());
            armcpp11::SharedPtr<CTransaction> transaction(new CTransaction(packet));
            packet->state = EPacketProcessing;
            packet->SetValue(transaction->full_size,0);
            packet->SetValue(transaction->id,4);
            packet->data.reserve(transaction->full_size);    // don't has to worry about last couple of bytes
            // put packet to the transmitter list
            bool request_posted = false;
            while (!request_posted){
                armcpp11::UniqueLock<armcpp11::Mutex> lock(tx_mutex);
                if (tx_reqest_queue.size() >= max_request_num){
                    lock.Unlock();
                    SysLog()->LogWarning(LOG_CONTEXT, "too many requests in queue: wait some time");
                    armcpp11::ThisThread::SleepFor(armcpp11::MilliSeconds(10));
                }
                else {
                    tx_reqest_queue.push_back(transaction);    // push request in the queue
                    lock.Unlock();        // now can unlock the mutex
                    tx_cv.NotifyAll();    // notify working thread
                    request_posted = true;
                    SysLog()->LogDebug(LOG_CONTEXT, "transaction " FSIZE_T " has been put into transmitter queue", packet->id) ;
                }
            }
        }
        else {
            SysLog()->LogError(LOG_CONTEXT, "invalid transaction id: " FSIZE_T " for packet with size: " FSIZE_T, packet->id, packet->data.size());
            packet->state = EPacketInvalid;
            packet->Invoke();
            result = EATLErrorInvalidParameters;
        }
        return result ;
    }

    CATLError CBufferManager::ProcessAPIRequest(const armcpp11::SharedPtr<CAPIRequest>& request)
    {
        CATLError result;
        if (request->request->type == APIRead || request->request->type == APIWrite){
            // put packet to the transmitter list
            bool request_posted = false;
            while (!request_posted){
                armcpp11::UniqueLock<armcpp11::Mutex> lock(api_mutex);
                if (api_reqest_queue.size() >= max_request_num){
                    lock.Unlock();
                    SysLog()->LogWarning(LOG_CONTEXT, "too many requests in queue: wait some time");
                    armcpp11::ThisThread::SleepFor(armcpp11::MilliSeconds(10)); // 10 ms
                }
                else {
                    api_reqest_queue.push_back(request);    // push request in the queue
                    lock.Unlock();        // now can unlock the mutex
                    api_cv.NotifyAll();    // notify working thread
                    request_posted = true;
                    SysLog()->LogDebug(LOG_CONTEXT, "firmware api request id " FSIZE_T " is put into api queue", request->request->id) ;
                }
            }
        }
        else {
            SysLog()->LogError(LOG_CONTEXT, "invalid firmware request type: %s", EFWRequestTypeStr(request->request->type));
            request->request->state = Fail;
            request->listener.Invoke(request->request);
            result = EATLErrorInvalidParameters;
        }
        return result ;
    }

    void CBufferManager::send_data(UInt8* data, basesize size, UInt32 offset, baseid id, TATLObserver<CBufferManager>& listener)
    {
        SysLog()->LogDebug(LOG_CONTEXT, "sending data to offset 0x%X, size: " FSIZE_T, offset, size) ;
        TSmartPtr<CTransferPacket> pack = new CTransferPacket;
        pack->address = offset;
        pack->data = std::vector<UInt8>(size);
        copy(data,data+size,pack->data.begin());
        pack->command = EPacketWrite;
        pack->listener += &listener;
        pack->reg_id = id;
        bus_manager->ProcessRequest(pack);
    }

    void CBufferManager::set_value(UInt32 value, UInt32 offset, baseid id, TATLObserver<CBufferManager>& listener)
    {
        SysLog()->LogDebug(LOG_CONTEXT, "set value " FSIZE_T " to offset 0x%X", value, offset) ;
        TSmartPtr<CTransferPacket> pack = new CTransferPacket;
        pack->address = offset;
        pack->data = std::vector<UInt8>(4);
        pack->SetValue((value&0xFFFF)|(value<<16),0);
        pack->command = EPacketWrite;
        pack->listener += &listener;
        pack->reg_id = id;
        bus_manager->ProcessRequest(pack);
    }

    void CBufferManager::request_status(UInt32 offset, baseid id, TATLObserver<CBufferManager>& listener)
    {
        SysLog()->LogDebug(LOG_CONTEXT, "request status from offset 0x%X", offset) ;
        TSmartPtr<CTransferPacket> pack = new CTransferPacket;
        pack->address = offset;
        pack->data = std::vector<UInt8>(header_size);
        pack->command = EPacketRead;
        pack->listener += &listener;
        pack->reg_id = id;
        bus_manager->ProcessRequest(pack);
    }

    void CBufferManager::request_data(UInt32 offset, basesize size, baseid id, TATLObserver<CBufferManager>& listener)
    {
        SysLog()->LogDebug(LOG_CONTEXT, "request data from offset 0x%X size: " FSIZE_T, offset, size) ;
        TSmartPtr<CTransferPacket> pack = new CTransferPacket;
        pack->address = offset;
        pack->data = std::vector<UInt8>(size);
        pack->command = EPacketRead;
        pack->listener += &listener;
        pack->reg_id = id;
        bus_manager->ProcessRequest(pack);
    }

    void * CBufferManager::tx_thread_processing_wrapper(void* context) {
        ((CBufferManager*)context)->tx_thread_processing();
        return NULL;
    }

    void CBufferManager::tx_thread_processing(void)
    {
        try {
        SysLog()->LogDebug(LOG_CONTEXT, "tx thread started");
        TSmartPtr<CTransferPacket> packet;
        armcpp11::SharedPtr<CTransaction> trx;
        UInt32 sent_part = 0;
        bool established = false;
        enum SM {Idle, GetSpace, Reset, Write, WriteFinished} state = Idle;
        set_value(CHANNEL_REQUESTED,buf_tx_addr+state_offset,0,transmitter);
        atl::thread rx_thread(&CBufferManager::rx_thread_processing_wrapper, this);
        while(!thread_stop) {
            armcpp11::UniqueLock<armcpp11::Mutex> lock(tx_mutex);
            if ((state == Idle && tx_reqest_queue.empty()) || (state != Idle && tx_received_packets.empty())) {
                tx_cv.Wait(lock); // sleep while something will come
            }
            if (state != Idle) {
                if (trx->timeout()){    // check transaction timeout
                    lock.Unlock(); // now we can unlock the mutex
                    SysLog()->LogError(LOG_CONTEXT, "timeout for transaction id " FSIZE_T,trx->packet->id);
                    trx->packet->state = EPacketNoAnswer;
                    trx->packet->Invoke();
                    state = Idle;
                    continue;
                }
                else if (!tx_received_packets.empty()){
                    packet = tx_received_packets.front();
                    tx_received_packets.pop_front();
                    lock.Unlock(); // now we can unlock the mutex
                    if (packet->reg_id != trx->packet->id) {
                        SysLog()->LogWarning(LOG_CONTEXT, "unexpected packet with reg id: " FSIZE_T ": expected id " FSIZE_T,packet->reg_id, trx->packet->id);
                        continue;
                    }
                    else if (packet->state != EPacketSuccess) {
                        SysLog()->LogError(LOG_CONTEXT, "failed to get response during transaction " FSIZE_T, trx->packet->id);
                        trx->packet->state = EPacketFail;
                        trx->packet->Invoke();
                        state = Idle;
                        continue;
                    }
                }
                else {
                    continue; // nothing to do without reply
                }
            }
            switch (state){
            case Idle:
                if (!tx_reqest_queue.empty()) {
                    trx = tx_reqest_queue.front(); // take request
                    tx_reqest_queue.pop_front();
                    tx_received_packets.clear();
                    lock.Unlock(); // now we can unlock the mutex
                    SysLog()->LogDebug(LOG_CONTEXT, "get transaction id " FSIZE_T " from queue", trx->packet->id);
                    if (trx->timeout()){    // check transaction timeout
                        SysLog()->LogError(LOG_CONTEXT, "timeout for transaction id " FSIZE_T, trx->packet->id);
                        trx->packet->state = EPacketNoAnswer;
                        trx->packet->Invoke();
                        continue;
                    }
                    else if (established) {
                        SysLog()->LogDebug(LOG_CONTEXT, "connection is established: requesting status");
                        request_status(buf_tx_addr,trx->packet->id,transmitter);
                        state = GetSpace;
                    }
                    else {
                        SysLog()->LogWarning(LOG_CONTEXT, "connection is not established: sending channel request and status request");
                        set_value(0,buf_tx_addr+write_inx_offset,trx->packet->id,transmitter);
                        set_value(CHANNEL_REQUESTED,buf_tx_addr+state_offset,trx->packet->id,transmitter);
                        request_status(buf_tx_addr,trx->packet->id,transmitter);
                        state = Reset;
                    }
                }
                break;
            case GetSpace:
                if (packet->command == EPacketRead && packet->data.size() == header_size && packet->address == buf_tx_addr){
                    UInt32 tr_state = get_buffer_value(packet,state_offset,false);
                    UInt32 wr_inx = get_buffer_value(packet,write_inx_offset);
                    UInt32 rd_inx = get_buffer_value(packet,read_inx_offset);
                    if (tr_state == val_err || wr_inx == val_err || rd_inx == val_err) { // repeat request in case of error
                        request_status(buf_tx_addr,trx->packet->id,transmitter);
                        break;
                    }
                    UInt32 free_space = (wr_inx >= rd_inx) ? rd_inx - 4 - wr_inx + buf_data_size : rd_inx - 4 - wr_inx;
                    SysLog()->LogDebug(LOG_CONTEXT, "got space information: state: %d wr_inx: " FSIZE_T "rd_inx: " FSIZE_T " free_space " FSIZE_T, tr_state, wr_inx, rd_inx, free_space);
                    switch (tr_state) {
                    case FW_IS_RESET:
                    case CHANNEL_REQUESTED:
                        SysLog()->LogDebug(LOG_CONTEXT, "firmware is reset: request channel");
                        set_value(0,buf_tx_addr+write_inx_offset,trx->packet->id,transmitter);
                        set_value(CHANNEL_REQUESTED,buf_tx_addr+state_offset,trx->packet->id,transmitter);
                        request_status(buf_tx_addr,trx->packet->id,transmitter);
                        state = Reset;
                        established = false;
                        break;
                    case CHANNEL_ACKNOWLEDGE:
                    case WRITE_REGISTER_UPDATED:
                    case READ_REGISTER_UPDATED:
                        if (free_space) {
                            // transmition strats here, we have to put request in the receiver queue
                            armcpp11::UniqueLock<armcpp11::Mutex> lock(rx_mutex);
                            rx_reqest_list.insert(rx_list_type::value_type(trx->id,trx));
                            lock.Unlock();
                            rx_cv.NotifyAll();
                            // now calculate portion of the data and send it
                            sent_part = std::min(trx->full_size,buf_data_size-wr_inx);
                            sent_part = std::min(sent_part, free_space);
                            send_data(trx->packet->data.data(),sent_part,buf_tx_addr+data_offset+wr_inx,trx->packet->id,transmitter);
                            wr_inx += sent_part;
                            if (wr_inx >= buf_data_size) wr_inx = 0;
                            SysLog()->LogDebug(LOG_CONTEXT, "sent " FSIZE_T " out of " FSIZE_T " new wr_inx " FSIZE_T, sent_part, trx->full_size, wr_inx);
                            set_value(wr_inx,buf_tx_addr+write_inx_offset,trx->packet->id,transmitter);
                            set_value(WRITE_REGISTER_UPDATED,buf_tx_addr+state_offset,trx->packet->id,transmitter);
                            if (sent_part == trx->full_size) { // everything is sent
                                state = WriteFinished;
                            }
                            else {
                                state = Write;
                                request_status(buf_tx_addr,trx->packet->id,transmitter);
                            }
                        }
                        else {
                            SysLog()->LogDebug(LOG_CONTEXT, "no free space: wait some time");
                            armcpp11::ThisThread::SleepFor(armcpp11::MilliSeconds(2)); // 2 ms // give some time to FW for processing previous requests
                            request_status(buf_tx_addr,trx->packet->id,transmitter);
                        }
                        break;
                    default:
                        SysLog()->LogError(LOG_CONTEXT, "wrong transaction state %d",tr_state);
                        trx->packet->state = EPacketFail;
                        trx->packet->Invoke();
                        state = Idle;
                        established = false;
                        break;
                    }
                }
                else {
                    SysLog()->LogError(LOG_CONTEXT, "unexpected response during transaction id " FSIZE_T,trx->packet->id);
                }
                break;
            case Reset:
                if (packet->command == EPacketRead && packet->data.size() == header_size && packet->address == buf_tx_addr){
                    UInt32 tr_state = get_buffer_value(packet,state_offset,false);
                    if (tr_state == val_err) {
                        request_status(buf_tx_addr,trx->packet->id,transmitter);
                        break;
                    }
                    SysLog()->LogDebug(LOG_CONTEXT, "received state: %d", tr_state);
                    switch (tr_state) {
                    case FW_IS_RESET:
                        set_value(0,buf_tx_addr+write_inx_offset,trx->packet->id,transmitter);
                        set_value(CHANNEL_REQUESTED,buf_tx_addr+state_offset,trx->packet->id,transmitter);
                    case CHANNEL_REQUESTED:
                        SysLog()->LogDebug(LOG_CONTEXT, "firmware is still in reset mode: wait some time");
                        armcpp11::ThisThread::SleepFor(armcpp11::MilliSeconds(2)); // 2 ms // give some time to FW for process our request
                        request_status(buf_tx_addr,trx->packet->id,transmitter);
                        break;
                    case CHANNEL_ACKNOWLEDGE:
                    case WRITE_REGISTER_UPDATED:
                    case READ_REGISTER_UPDATED:
                        SysLog()->LogDebug(LOG_CONTEXT, "established channel with firmware: requesting free space");
                        established = true;
                        request_status(buf_tx_addr,trx->packet->id,transmitter);
                        state = GetSpace;
                        break;
                    default:
                        SysLog()->LogError(LOG_CONTEXT, "wrong transaction state %d",tr_state);
                        trx->packet->state = EPacketFail;
                        trx->packet->Invoke();
                        state = Idle;
                        established = false;
                        break;
                    }
                }
                break;
            case Write:
                if (packet->command == EPacketRead && packet->data.size() == header_size && packet->address == buf_tx_addr){
                    UInt32 tr_state = get_buffer_value(packet,state_offset,false);
                    UInt32 wr_inx = get_buffer_value(packet,write_inx_offset);
                    UInt32 rd_inx = get_buffer_value(packet,read_inx_offset);
                    if (tr_state == val_err || wr_inx == val_err || rd_inx == val_err) { // repeat request in case of error
                        request_status(buf_tx_addr,trx->packet->id,transmitter);
                        break;
                    }
                    UInt32 free_space = (wr_inx >= rd_inx) ? wr_inx + buf_data_size - 4 - rd_inx : rd_inx - 4 - wr_inx;
                    SysLog()->LogDebug(LOG_CONTEXT, "got space information: state: %d wr_inx: %d rd_inx: %d free_space %d", tr_state, wr_inx, rd_inx, free_space);
                    switch (tr_state) {
                    case FW_IS_RESET:
                    case CHANNEL_REQUESTED:
                    case CHANNEL_ACKNOWLEDGE:
                        SysLog()->LogDebug(LOG_CONTEXT, "firmware is reset: restarting transaction");
                        set_value(0,buf_tx_addr+write_inx_offset,trx->packet->id,transmitter);
                        set_value(CHANNEL_REQUESTED,buf_tx_addr+state_offset,trx->packet->id,transmitter);
                        request_status(buf_tx_addr,trx->packet->id,transmitter);
                        state = Reset;
                        established = false;
                        break;
                    case WRITE_REGISTER_UPDATED:
                    case READ_REGISTER_UPDATED:
                        if (free_space) {
                            // now calculate new portion of the data and send it
                            UInt32 portion = std::min(trx->full_size - sent_part, buf_data_size - wr_inx);
                            portion = std::min(portion, free_space);
                            send_data(trx->packet->data.data() + sent_part,portion,buf_tx_addr+data_offset+wr_inx,trx->packet->id,transmitter);
                            sent_part += portion;
                            wr_inx += portion;
                            if (wr_inx >= buf_data_size) wr_inx = 0;
                            SysLog()->LogDebug(LOG_CONTEXT, "sent %d out of %d new wr_inx %d", sent_part, trx->full_size, wr_inx);
                            set_value(wr_inx,buf_tx_addr+write_inx_offset,trx->packet->id,transmitter);
                            set_value(WRITE_REGISTER_UPDATED,buf_tx_addr+state_offset,trx->packet->id,transmitter);
                            if (sent_part == trx->full_size) { // everything is sent
                                state = WriteFinished;
                            }
                            else {
                                request_status(buf_tx_addr,trx->packet->id,transmitter); // request status to check free space
                            }
                        }
                        else {
                            SysLog()->LogDebug(LOG_CONTEXT, "no free space: wait some time");
                            armcpp11::ThisThread::SleepFor(armcpp11::MilliSeconds(2)); // 2 ms // give some time to FW for processing previous requests
                            request_status(buf_tx_addr,trx->packet->id,transmitter);
                        }
                        break;
                    default:
                        SysLog()->LogError(LOG_CONTEXT, "wrong transaction state %d",tr_state);
                        trx->packet->state = EPacketFail;
                        trx->packet->Invoke();
                        state = Idle;
                        established = false;
                        break;
                    }
                }
                break;
            case WriteFinished:
                if (packet->command == EPacketWrite && packet->data.size() == 4 && packet->address == buf_tx_addr + state_offset) {
                    SysLog()->LogDebug(LOG_CONTEXT, "write for transaction id " FSIZE_T " has finished", trx->packet->id);
                    trx->transmitted = true;
                    trx.Reset(); // don't need this transaction anymore
                    state = Idle;
                }
                break;
            }
        }
        rx_thread.Join();
        SysLog()->LogDebug(LOG_CONTEXT, "tx thread stopped");
        } catch(std::exception& ex) {
            SysLog()->LogError(LOG_CONTEXT, "exception inside tx thread: %s", ex.what());
        }
    }

    void CBufferManager::ProcessTxReply(void* ptr)
    {
        armcpp11::UniqueLock<armcpp11::Mutex> lock(tx_mutex);
        tx_received_packets.push_back(static_cast<CTransferPacket*>(ptr));
        lock.Unlock();
        tx_cv.NotifyAll();
    }

    void * CBufferManager::rx_thread_processing_wrapper(void* context) {
        ((CBufferManager*)context)->rx_thread_processing();
        return NULL;
    }

    void CBufferManager::rx_thread_processing(void) {
        TSmartPtr<CTransferPacket> transaction = new CTransferPacket;
        TSmartPtr<CTransferPacket> packet;
        UInt32 received_part = 0;
        bool established = false;
        bool is_request;
        enum SM {Idle, Reset, WaitHeader, Header, ReadData, Data} state = Idle;
        SysLog()->LogDebug(LOG_CONTEXT, "RX thread started");
        set_value(CHANNEL_REQUESTED,buf_rx_addr+state_offset,0,receiver);
        while(!thread_stop) {
            armcpp11::UniqueLock<armcpp11::Mutex> lock(rx_mutex);
            if ((state == Idle && rx_reqest_list.empty()) || (state != Idle && rx_received_packets.empty())) {
                rx_cv.Wait(lock); // sleep while something will come
            }
            // check deleted packets or timeouts
            for (rx_list_type::iterator it = rx_reqest_list.begin(); it != rx_reqest_list.end();) {
                armcpp11::SharedPtr<CTransaction>& ptr = it->second;
                if (ptr->packet->state != EPacketProcessing) { // TX thread might cancel transaction, so remove it from list
                    rx_list_type::iterator tmp = it++;
                    rx_reqest_list.erase(tmp);
                }
                else if (ptr->transmitted && ptr->timeout()) { // for transmitted packets we are responsible for timeout
                    ptr->packet->state = EPacketFail;
                    ptr->packet->Invoke();
                    rx_list_type::iterator tmp = it++;
                    rx_reqest_list.erase(tmp);
                }
                else {
                    ++it;
                }
            }
            // process state
            if (state != Idle) {
                if (!rx_received_packets.empty()){
                    packet = rx_received_packets.front();
                    rx_received_packets.pop_front();
                    lock.Unlock(); // now we can unlock the mutex
                    if (packet->state != EPacketSuccess) {
                        SysLog()->LogError(LOG_CONTEXT, "failed to receive response");
                        state = Reset;
                        continue;
                    }
                }
                else {
                    continue;
                }
            }
            switch (state) {
            case Idle:
                is_request = !rx_reqest_list.empty();
                rx_received_packets.clear();
                lock.Unlock();
                if (is_request) {
                    if (established) {
                        SysLog()->LogDebug(LOG_CONTEXT, "request RX status");
                        request_status(buf_rx_addr,0,receiver);
                        state = WaitHeader;
                    }
                    else {
                        SysLog()->LogDebug(LOG_CONTEXT, "sending RX communication reset");
                        set_value(0,buf_rx_addr+read_inx_offset,0,receiver);
                        set_value(CHANNEL_REQUESTED,buf_rx_addr+state_offset,0,receiver);
                        request_status(buf_rx_addr,0,receiver);
                        state = Reset;
                    }
                }
                break;
            case Reset:
                if (packet->command == EPacketRead && packet->data.size() == header_size && packet->address == buf_rx_addr) {
                    UInt32 tr_state = get_buffer_value(packet,state_offset,false);
                    if (tr_state == val_err) {
                        request_status(buf_rx_addr,0,receiver);
                        break;
                    }
                    switch (tr_state){
                    case FW_IS_RESET:
                    case CHANNEL_REQUESTED:
                        SysLog()->LogDebug(LOG_CONTEXT, "firmware is reset: wait some time");
                        armcpp11::ThisThread::SleepFor(armcpp11::MilliSeconds(2)); // 2 ms // give some time to FW for processing requests
                        request_status(buf_rx_addr,0,receiver);
                        break;
                    case CHANNEL_ACKNOWLEDGE:
                    case WRITE_REGISTER_UPDATED:
                    case READ_REGISTER_UPDATED:
                        set_value(READ_REGISTER_UPDATED,buf_rx_addr+state_offset,0,receiver);
                        request_status(buf_rx_addr,0,receiver);
                        state = WaitHeader;
                        established = true;
                        break;
                    default:
                        SysLog()->LogError(LOG_CONTEXT, "wrong transaction state %d",tr_state);
                        state = Idle;
                        break;
                    }
                }
                break;
            case WaitHeader:
                if (packet->command == EPacketRead && packet->data.size() == header_size && packet->address == buf_rx_addr) {
                    UInt32 tr_state = get_buffer_value(packet,state_offset,false);
                    UInt32 wr_inx = get_buffer_value(packet,write_inx_offset);
                    UInt32 rd_inx = get_buffer_value(packet,read_inx_offset);
                    if (tr_state == val_err || wr_inx == val_err || rd_inx == val_err) { // repeat request in case of error
                        request_status(buf_rx_addr,0,receiver);
                        break;
                    }
                    UInt32 available = (wr_inx >= rd_inx) ? wr_inx - rd_inx : wr_inx + buf_data_size - rd_inx;
                    SysLog()->LogDebug(LOG_CONTEXT, "got available information: state: %d wr_inx: %d rd_inx: %d available %d", tr_state, wr_inx, rd_inx, available);
                    switch (tr_state){
                    case FW_IS_RESET:
                    case CHANNEL_REQUESTED:
                    case CHANNEL_ACKNOWLEDGE:
                        SysLog()->LogWarning(LOG_CONTEXT, "firmware is reset: restarting tranaction");
                        state = Idle;
                        established = false;
                        break;
                    case WRITE_REGISTER_UPDATED:
                    case READ_REGISTER_UPDATED:
                        if (available >= 4) { // can read header
                            request_data(buf_rx_addr + data_offset + rd_inx,4,0,receiver);
                            rd_inx += 4;
                            if (rd_inx >= buf_data_size) rd_inx = 0;
                            set_value(rd_inx,buf_rx_addr+read_inx_offset,0,receiver);
                            set_value(READ_REGISTER_UPDATED,buf_rx_addr+state_offset,0,receiver);
                            state = Header;
                            SysLog()->LogDebug(LOG_CONTEXT, "read header new rd_inx %d", rd_inx);
                        }
                        else {
                            SysLog()->LogDebug(LOG_CONTEXT, "nothing to read: wait some time");
                            armcpp11::ThisThread::SleepFor(armcpp11::MilliSeconds(2)); // 2 ms // nothing to read
                            request_status(buf_rx_addr,0,receiver);
                        }
                        break;
                    default:
                        SysLog()->LogError(LOG_CONTEXT, "wrong transaction state %d",tr_state);
                        state = Idle;
                        break;
                    }
                }
                break;
            case Header:
                if (packet->command == EPacketRead && packet->data.size() == 4) {
                    UInt32 size = (packet->GetValue32(0)+3)&~3;
                    if (size >= 12){
                        try {
                            transaction->data.resize(size);
                        }
                        catch (std::bad_alloc) {
                            SysLog()->LogError(LOG_CONTEXT, "can't allocate " FSIZE_T " bytes for packet", size);
                            state = Idle;
                            break;
                        }
                        SysLog()->LogDebug(LOG_CONTEXT, "allocated " FSIZE_T " bytes for transaction, request data", size);
                        copy(packet->data.begin(),packet->data.end(),transaction->data.begin());
                        received_part = 4;
                        request_status(buf_rx_addr,0,receiver);
                        state = ReadData;
                    }
                    else {
                        SysLog()->LogError(LOG_CONTEXT, "wrong packet size: " FSIZE_T " for transaction", size);
                        state = Idle;
                    }
                }
                break;
            case ReadData:
                if (packet->command == EPacketRead && packet->data.size() == header_size && packet->address == buf_rx_addr) {
                    UInt32 tr_state = get_buffer_value(packet,state_offset,false);
                    UInt32 wr_inx = get_buffer_value(packet,write_inx_offset);
                    UInt32 rd_inx = get_buffer_value(packet,read_inx_offset);
                    if (tr_state == val_err || wr_inx == val_err || rd_inx == val_err) { // repeat request in case of error
                        request_status(buf_rx_addr,0,receiver);
                        break;
                    }
                    UInt32 available = (wr_inx >= rd_inx) ? wr_inx - rd_inx : wr_inx + buf_data_size - rd_inx;
                    SysLog()->LogDebug(LOG_CONTEXT, "got available information: state: %d wr_inx: %d rd_inx: %d available %d", tr_state, wr_inx, rd_inx, available);
                    switch (tr_state){
                    case FW_IS_RESET:
                    case CHANNEL_REQUESTED:
                    case CHANNEL_ACKNOWLEDGE:
                        SysLog()->LogWarning(LOG_CONTEXT, "firmware is reset: restarting tranaction...");
                        state = Idle;
                        established = false;
                        break;
                    case WRITE_REGISTER_UPDATED:
                    case READ_REGISTER_UPDATED:
                        if (available > 0) { // can read data
                            UInt32 portion = std::min((UInt32)transaction->data.size() - received_part, buf_data_size - rd_inx);
                            portion = std::min(portion, available);
                            request_data(buf_rx_addr + data_offset + rd_inx,portion,0,receiver);
                            rd_inx += portion;
                            if (rd_inx >= buf_data_size) rd_inx = 0;
                            set_value(rd_inx,buf_rx_addr+read_inx_offset,0,receiver);
                            set_value(READ_REGISTER_UPDATED,buf_rx_addr+state_offset,0,receiver);
                            state = Data;
                            SysLog()->LogDebug(LOG_CONTEXT, "request for read portion size: " FSIZE_T " new rd_inx %d", portion, rd_inx);
                        }
                        else {
                            SysLog()->LogDebug(LOG_CONTEXT, "nothing to read: wait some time");
                            armcpp11::ThisThread::SleepFor(armcpp11::MilliSeconds(2)); // 2 ms // nothing to read
                            request_status(buf_rx_addr,0,receiver);
                        }
                        break;
                    default:
                        SysLog()->LogError(LOG_CONTEXT, "wrong trasaction state %d",tr_state);
                        state = Idle;
                        break;
                    }
                }
                break;
            case Data:
                if (packet->command == EPacketRead) {
                    SysLog()->LogDebug(LOG_CONTEXT, "received portion size: " FSIZE_T, packet->data.size());
                    if (packet->data.size() <= transaction->data.size() - received_part){
                        copy(packet->data.begin(),packet->data.end(),transaction->data.begin()+received_part);
                        received_part += (UInt32)packet->data.size();
                        if (received_part == transaction->data.size()) { // all received
                            UInt32 id = transaction->GetValue32(4);
                            lock.Lock();
                            rx_list_type::iterator it = rx_reqest_list.find(id);
                            if (it == rx_reqest_list.end()){
                                lock.Unlock();
                                SysLog()->LogWarning(LOG_CONTEXT, "can't find transaction with id %d", id);
                            }
                            else {
                                TSmartPtr<CTransferPacket> trx = it->second->packet;
                                rx_reqest_list.erase(it);
                                lock.Unlock();
                                SysLog()->LogDebug(LOG_CONTEXT, "received response for transaction id %d, size: " FSIZE_T, trx->id, transaction->data.size());
                                trx->data = transaction->data;
                                trx->state = EPacketSuccess;
                                trx->Invoke();
                            }
                            state = Idle; // now just wait next packet
                        }
                        else { // read next portion of data
                            SysLog()->LogDebug(LOG_CONTEXT, "sending status request for next portion");
                            request_status(buf_rx_addr,0,receiver);
                            state = ReadData;
                        }
                    }
                    else {
                        SysLog()->LogError(LOG_CONTEXT, "reply has too big size: " FSIZE_T ": can't fit to %d bytes ",packet->data.size(), transaction->data.size() - received_part);
                        state = Idle;
                        break;
                    }
                }
                break;
            }
        }
        SysLog()->LogDebug(LOG_CONTEXT, "RX thread stopped");
    }

    void CBufferManager::ProcessRxReply(void* ptr)
    {
        armcpp11::UniqueLock<armcpp11::Mutex> lock(rx_mutex);
        rx_received_packets.push_back(static_cast<CTransferPacket*>(ptr));
        lock.Unlock();
        rx_cv.NotifyAll();
    }

    void CBufferManager::request_api(UInt32 offset, UInt32 size)
    {
        SysLog()->LogDebug(LOG_CONTEXT, "requesting firmware api from offset 0x%X", offset) ;
        TSmartPtr<CTransferPacket> pack = new CTransferPacket;
        pack->address = offset;
        pack->data = std::vector<UInt8>(size);
        pack->command = EPacketRead;
        pack->listener += &api_listener;
        bus_manager->ProcessRequest(pack);
    }

    void CBufferManager::reset_api_rx()
    {
        SysLog()->LogDebug(LOG_CONTEXT, "sending reset RX firmware api command");
        TSmartPtr<CTransferPacket> pack = new CTransferPacket;
        pack->address = api_rx_addr;
        pack->data = std::vector<UInt8>(1);
        pack->data[0] = API_RESET;
        pack->command = EPacketWrite;
        bus_manager->ProcessRequest(pack);
    }

    void * CBufferManager::api_thread_processing_wrapper(void* context) {
        ((CBufferManager*)context)->api_thread_processing();
        return NULL;
    }

    void CBufferManager::api_thread_processing(void)
    {
        try {
        SysLog()->LogDebug(LOG_CONTEXT, "firmware api thread started");
        TSmartPtr<CTransferPacket> packet;
        bool got_packet;
        api_list_type sent_list;
        bool queue_is_empty;
        enum TXSM {TXIdle, Status} tx_state = TXIdle;
        enum RXSM {RXIdle, Data} rx_state = RXIdle;
        reset_api_rx();
        while(!thread_stop) {
            armcpp11::UniqueLock<armcpp11::Mutex> lock(api_mutex);
            if (((tx_state == TXIdle && api_reqest_queue.empty()) || (tx_state != TXIdle && api_received_packets.empty())) &&
                ((rx_state == RXIdle && sent_list.empty()) || (rx_state != RXIdle && api_received_packets.empty()))) {
                api_cv.Wait(lock); // sleep while something happens
            }
            // check timeouts
            for (std::deque<armcpp11::SharedPtr<CAPIRequest> >::iterator it = api_reqest_queue.begin();
                 it != api_reqest_queue.end();) {
                armcpp11::SharedPtr<CAPIRequest>& req = *it;
                if (req->timeout()) { // for transmitted packets we are responsible for timeout
                    req->request->state = Timeout;
                    req->listener.Invoke(req->request);
                    std::deque<armcpp11::SharedPtr<CAPIRequest> >::iterator tmp = it++;
                    api_reqest_queue.erase(tmp);
                }
                else {
                    ++it;
                }
            }
            queue_is_empty = api_reqest_queue.empty();
            // Get income packet
            if (!api_received_packets.empty()){
                packet = api_received_packets.front();
                api_received_packets.pop_front();
                got_packet = true;
            }
            else {
                got_packet = false;
            }
            lock.Unlock();
            for (api_list_type::iterator it = sent_list.begin(); it != sent_list.end();) {
                if (it->second->timeout()) { // for transmitted packets we are responsible for timeout
                    it->second->request->state = Timeout;
                    it->second->listener.Invoke(it->second->request);
                    api_list_type::iterator tmp = it++;
                    sent_list.erase(tmp);
                }
                else {
                    ++it;
                }
            }
            // Transmitter part
            switch (tx_state) {
            case TXIdle:
                if (!queue_is_empty) {
                    request_api(api_tx_addr+tx_cmd_offset,1);
                    tx_state = Status;
                }
                break;
            case Status:
                if (queue_is_empty) {
                    tx_state = TXIdle;
                }
                else if (got_packet && packet->address == api_tx_addr+tx_cmd_offset && packet->command == EPacketRead) {
                    got_packet = false;
                    if (packet->state == EPacketSuccess && packet->data.size() == 1 && packet->data[0] == API_RESET) { // FW is ready to receive command
                        lock.Lock();
                        armcpp11::SharedPtr<CAPIRequest> req = api_reqest_queue.front();
                        api_reqest_queue.pop_front();
                        lock.Unlock();
                        SysLog()->LogDebug(LOG_CONTEXT, "sending firmware api command id " FSIZE_T, req->request->id) ;
                        TSmartPtr<CTransferPacket> pack = new CTransferPacket;
                        pack->address = api_tx_addr;
                        pack->command = EPacketWrite;
                        pack->data = std::vector<UInt8>(api_size);
                        pack->SetValue((UInt32)req->request->id,0);
                        pack->SetValue(req->request->value,4);
                        pack->data[8] = req->request->cmd_type;
                        pack->data[9] = req->request->cmd;
                        pack->data[11] = req->request->type == APIRead ? API_READ : API_WRITE;
                        bus_manager->ProcessRequest(pack);
                        sent_list.insert(api_list_type::value_type((UInt32)req->request->id,req));
                        tx_state = TXIdle;
                    }
                    else {
                        SysLog()->LogDebug(LOG_CONTEXT, "firmware is not ready: resending status request...");
                        request_api(api_tx_addr+tx_cmd_offset,1);    // request status again
                        if (sent_list.empty()){
                            reset_api_rx();
                        }
                    }
                }
            }
            // Receiver part
            switch (rx_state) {
            case RXIdle:
                if (!sent_list.empty()) {
                    request_api(api_rx_addr,api_size);
                    rx_state = Data;
                }
                break;
            case Data:
                if (sent_list.empty()) {
                    rx_state = RXIdle;
                    reset_api_rx();
                }
                else if (got_packet && packet->address == api_rx_addr && packet->command == EPacketRead) {
                    got_packet = false;
                    if (packet->state == EPacketSuccess && packet->data.size() == api_size && (packet->data[0] == API_READ || packet->data[0] == API_WRITE)) { // Get valid reply
                        UInt32 id = packet->GetValue32(4);
                        api_list_type::iterator it = sent_list.find(id);
                        if (it == sent_list.end()){
                            SysLog()->LogWarning(LOG_CONTEXT, "can't find firmware transaction with id %d", id);
                        } else {
                            TSmartPtr<CFWRequest>& req = it->second->request;
                            req->state = Success;
                            req->value = packet->GetValue32(8);
                            req->status = packet->GetValue16(2);
                            SysLog()->LogDebug(LOG_CONTEXT, "firmware api request id " FSIZE_T " response has been received", req->id) ;
                            it->second->listener.Invoke(req);
                            sent_list.erase(it);
                        }
                        rx_state = RXIdle;
                        reset_api_rx();
                    } else {
                        SysLog()->LogDebug(LOG_CONTEXT, "firmware has not replied: state %d size %d status %d: resending data request...", packet->state, packet->data.size(), packet->data.size() > 0 ? packet->data[0] : -1);
                        request_api(api_rx_addr, api_size); // request data again
                    }
                }
            }
        }
        SysLog()->LogDebug(LOG_CONTEXT, "firmware api thread stopped");
        } catch(std::exception& ex) {
            SysLog()->LogError(LOG_CONTEXT, "exception inside firmware api thread: %s", ex.what());
        }
    }

    void CBufferManager::ProcessAPIReply(void* ptr)
    {
        armcpp11::UniqueLock<armcpp11::Mutex> lock(api_mutex);
        api_received_packets.push_back(static_cast<CTransferPacket*>(ptr));
        lock.Unlock();
        api_cv.NotifyAll();
    }

    void CBufferManager::create_threads(void)
    {
        if (thread_stop) {
            stop_threads();
        }
        thread_stop = false;
        SysLog()->LogDebug(LOG_CONTEXT, "about to spawn threads");
        try {
            tx_thread = atl::thread(&CBufferManager::tx_thread_processing_wrapper, this);
            api_thread = atl::thread(&CBufferManager::api_thread_processing_wrapper, this);
        } catch(std::exception& ex) {
            SysLog()->LogError(LOG_CONTEXT, "exception at thread creation: %s", ex.what());
        }
        SysLog()->LogDebug(LOG_CONTEXT, "threads have been spawned");
    }

    void CBufferManager::stop_threads(void)
    {
        if (tx_thread.Joinable()) {
            tx_cv.NotifyAll();
            rx_cv.NotifyAll();
            tx_thread.Join();
        }
        if (api_thread.Joinable()) {
            api_cv.NotifyAll();
            api_thread.Join();
        }
    }
}
