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

#ifndef _ANDROID // this is not supported on Android platform
#include <ATL/ATLConfig.h>

#define LOG_CONTEXT "I2CDriver"
#include "I2CDriver.h"

using namespace std;
using namespace atl;

static map<string, string> defaults;

static void CreateMap()
{
    defaults["dev"] = "default";
}

namespace act {

    const UInt8 CI2CDriver::mpsse_dir = 0xDB;      // only SDA input (ADBUS2) and wait signal (ADBUS5) are configured as input

    const vector< string > CI2CDriver::GetDeviceList() {
        return ftdi.get_devices();
    }

    const vector<EPacketCommand> CI2CDriver::GetCommandList() {
        vector<EPacketCommand> commands;
        commands.push_back(EPacketRead);
        commands.push_back(EPacketWrite);
        return commands;
    }

    const vector<EACTDriverMode> CI2CDriver::GetSupportedModes() {
        vector<EACTDriverMode> modes;
        modes.push_back(EACTDriverASyncMode);
        return modes;
    }

    const EACTDriverMode CI2CDriver::GetCurrentMode() {
        return EACTDriverASyncMode;
    }

    CATLError CI2CDriver::Initialize(const flags32& options)
    {
        CreateMap();
        string prefix = getBusPrefix(options);
        SysConfig()->Merge(prefix, defaults);
        if (CATLError::GetLastError().IsError()) {
            return CATLError::GetLastError();
        }
        dev_name = SysConfig()->GetValue(prefix + "dev");
        return EATLErrorOk;
    }

    CATLError CI2CDriver::Open() {
        CATLError result;
        ftdi.open(dev_name);
        ftdi_opened = true;
        const string& chip = ftdi.get_chip_name();
        if (chip == "TTL232R-3V3") {
            baudrate = 260000;
            maximum_bytes_per_transaction = baudrate*2/1000; // 2ms
            bitbang = true;
            bitbang_scl = 0x01;
            bitbang_sda_in = 0x08;
            bitbang_sda_out = 0x04;
            try {
                configure();
            }
            catch (...) {
                ftdi_opened = false;
                result = EATLErrorFatal;
            }
        } else if (chip == "C232HM-DDHSL-0" ||
                 chip == "Dual RS232-HS" ||         // FT2232H without EEPROM
                 chip == "Dual RS232-HS A" ||       // this chip on windows has A at the end (and B for second channel)
                 chip == "FT2232H MiniModule" ||
                 chip == "FT2232H MiniModule A")    // this chip on windows has A at the end (and B for second channel)
        {
            baudrate = 5000000; // Maximum stable FPGA I2C speed is 10 Mb/s
            maximum_bytes_per_transaction = baudrate*3/1000/8; // 2ms
            bitbang = false;
            try {
                configure();
            } catch (...) {
                ftdi_opened = false;
                result = EATLErrorFatal;
            }
        } else {
            result = EATLErrorFatal;
            SysLog()->LogError(LOG_CONTEXT, "FTDI chip %s is not supported", chip.c_str());
            Close();
        }
        return result;
    }

    CATLError CI2CDriver::Close()
    {
        CATLError result;
        stop_threads();
        if (ftdi_opened) {
            try {
                ftdi.reset();
                ftdi.close();
            } catch (...) {
                result = EATLErrorFatal;
            }
        }
        ftdi_opened = false;
        return result;
    }

    CATLError CI2CDriver::Terminate() {
        return EATLErrorOk;
    }

    CATLError CI2CDriver::ProcessPacket( TSmartPtr< CTransferPacket > packet, const flags32&) {
        switch (packet->command){
        case EPacketWrite:
            return write_data(packet);
        case EPacketRead:
            return read_data(packet);
        default:
            packet->state = EPacketInvalid;
            NotifyListerners( packet );
            return EATLErrorInvalidParameters;
        }
    }

    CATLError CI2CDriver::write_data(TSmartPtr<CTransferPacket>& packet) {
        CATLError result;
        SysLog()->LogDebug(LOG_CONTEXT, "write packet: addr: 0x%X size: " FSIZE_T, packet->address, packet->data.size());
        packet->state = EPacketProcessing;
        if (!packet->mask.empty() && packet->mask.size() != packet->data.size()){
            SysLog()->LogError(LOG_CONTEXT, "write packet data and mask sizes mismatch");
            packet->state = EPacketInvalid;
            result =  EATLErrorInvalidParameters;
            NotifyListerners(packet);
        }
        else {
            put_tx_packet(packet);
        }
        return result;
    }

    CATLError CI2CDriver::read_data(TSmartPtr<CTransferPacket>& packet)
    {
        CATLError result;
        SysLog()->LogDebug(LOG_CONTEXT, " read packet: addr: 0x%X size: " FSIZE_T, packet->address, packet->data.size());
        packet->state = EPacketProcessing;
        put_tx_packet(packet);
        return result;
    }

    void CI2CDriver::configure()
    {
        current_page = -1;
        ftdi.reset();
        ftdi.set_bitmode(0xFF,FTDI_BITMODE_RESET);
        if (bitbang){
            ftdi.set_bitmode(bitbang_sda_out|bitbang_scl,FTDI_BITMODE_BITBANG);
            ftdi.set_baudrate(baudrate*scl_phase_num);
            SysLog()->LogInfo(LOG_CONTEXT, "FTDI is configured in bitbang mode");
        }
        else {
            ftdi.set_bitmode(0xFF,FTDI_BITMODE_MPSSE);
            tx_buffer.clear();
            tx_buffer.push_back(FTDI_DIS_DIV_5);
            tx_buffer.push_back(FTDI_DIS_ADAPT_CLK);
            tx_buffer.push_back(FTDI_EN_3_PHASE);
            tx_buffer.push_back(FTDI_DIS_LOOPBACK);

            UInt16 div = (60000000 + (baudrate>>1))/baudrate/scl_phase_num - 1;
            tx_buffer.push_back(FTDI_TCK_DIVIDER);
            tx_buffer.push_back((UInt8)(div&0xFF));
            tx_buffer.push_back((UInt8)(div>>8));

            mpsse_led(false);

            mpsse_out(true,true);       // initial I2C state: SCL - high, SDA - high

            ftdi.write(tx_buffer);
            SysLog()->LogInfo(LOG_CONTEXT, "FTDI is configured in MPSSE mode");
        }
        create_threads();
    }

    // RX thread
    void* CI2CDriver::tx_thread_wrapper(void* arg)
    {
        ((CI2CDriver*)arg)->tx_thread_processing();
        return NULL;
    }

    void* CI2CDriver::rx_thread_wrapper(void* arg)
    {
        ((CI2CDriver*)arg)->rx_thread_processing();
        return NULL;
    }

    void CI2CDriver::create_threads(void)
    {
        tx_buffer.clear();
        tx_queue.clear();
        tx_queue_write.clear();
        rx_queues.clear();
        threads_stop = false;
        tx_thread = armcpp11::Thread(&CI2CDriver::tx_thread_wrapper,this);
        rx_thread = armcpp11::Thread(&CI2CDriver::rx_thread_wrapper,this);
    }

    void CI2CDriver::stop_threads(void)
    {
        threads_stop = true;
        tx_cv.NotifyAll();
        rx_cv.NotifyAll();
        if (tx_thread.Joinable()) tx_thread.Join();
        if (rx_thread.Joinable()) rx_thread.Join();
    }

    void CI2CDriver::put_rx_messages(deque<TSmartPtr<RxMessage> >& msg)
    {
        armcpp11::UniqueLock<armcpp11::Mutex> lock(rx_mutex);
        rx_queues.push_back(msg);
        lock.Unlock();
        rx_cv.NotifyAll(); // wake read thread
    }

    bool CI2CDriver::is_rx_queue_empty()
    {
        armcpp11::LockGuard<armcpp11::Mutex> lock(rx_mutex);
        return rx_queues.empty();
    }

    void CI2CDriver::rx_data_read(deque<TSmartPtr<RxMessage> >& msg_queue)
    {
        basesize rx_size = 0;
        for (std::deque<TSmartPtr<RxMessage> >::const_iterator it = msg_queue.begin();
             it != msg_queue.end(); ++it) {
            rx_size += (*it)->size;
        }
        SysLog()->LogDebug(LOG_CONTEXT, "read buffer: rd size: " FSIZE_T, rx_size);
        rx_buffer.resize(rx_size);
        try{
            basesize inx = 0;
            ftdi.read(rx_buffer);
            for (std::deque<TSmartPtr<RxMessage> >::const_iterator it = msg_queue.begin();
                 it != msg_queue.end(); ++it) {
                basesize size = min(rx_buffer.size()-inx,(*it)->size);
                parse_rec_data(*(*it),rx_buffer.data()+inx,size);
                inx += size;
            }
        }
        catch (...){
            for (std::deque<TSmartPtr<RxMessage> >::const_iterator it = msg_queue.begin();
                 it != msg_queue.end(); ++it) {
                (*it)->packet->state = EPacketFail;
            }
        }
        for (std::deque<TSmartPtr<RxMessage> >::const_iterator it = msg_queue.begin();
             it != msg_queue.end(); ++it) {
            if ((*it)->read_modify_write && (*it)->packet->state == EPacketSuccess){
                put_tx_packet((*it)->packet,true);
            }
            else {
                NotifyListerners((*it)->packet);
            }
        }
    }

    void CI2CDriver::rx_thread_processing()
    {
        SysLog()->LogDebug(LOG_CONTEXT, "RX thread started");
        while(!threads_stop)
        {
            armcpp11::UniqueLock<armcpp11::Mutex> lock(rx_mutex);
            if (!threads_stop && rx_queues.empty()){
                rx_cv.Wait(lock);
            }
            if (!threads_stop && !rx_queues.empty())
            {
                deque<TSmartPtr<RxMessage> > msg_queue(rx_queues.front());
                rx_queues.pop_front();
                lock.Unlock();
                rx_data_read(msg_queue);
            }
        }
        SysLog()->LogDebug(LOG_CONTEXT, "RX thread finished");
    }

    // TX part
    TSmartPtr<CI2CDriver::RxMessage> CI2CDriver::prepare_data(TSmartPtr<CTransferPacket>& packet, bool check_mask)
    {
        TSmartPtr<RxMessage> msg;
        if (packet->command == EPacketWrite){
            if (check_mask && !packet->mask.empty()) {
                msg = new RxMessage(packet,true);
                prepare_data_read(*msg);
            }
            else {
                msg = new RxMessage(packet,false);
                prepare_data_write(*msg);
            }
        }
        else {
            msg = new RxMessage(packet,false);
            prepare_data_read(*msg);
        }
        return msg;
    }

    void CI2CDriver::tx_thread_processing()
    {
        SysLog()->LogDebug(LOG_CONTEXT, "TX thread started");
        std::deque<TSmartPtr<RxMessage> > msg_queue;
        while(!threads_stop)
        {
            armcpp11::UniqueLock<armcpp11::Mutex> lock(tx_mutex);
            if (!threads_stop && tx_queue.empty() && tx_queue_write.empty() && tx_buffer.empty()){
                tx_cv.Wait(lock);
            }
            if (threads_stop) break;
            if (tx_buffer.size() > maximum_bytes_per_transaction || (tx_queue.empty() && tx_queue_write.empty())){
                lock.Unlock();
                if (!bitbang) tx_buffer.push_back(FTDI_SEND_ANSWER); // transaction is finished - send back the answer
                SysLog()->LogDebug(LOG_CONTEXT, "send buffer: total size: " FSIZE_T, tx_buffer.size());
                try {
                    if (ftdi.read_before_write) {
                        put_rx_messages(msg_queue);
                        ftdi.write(tx_buffer);
                    }
                    else {
                        while (!is_rx_queue_empty()) armcpp11::ThisThread::YieldThread(); // in D2XX we have to wait that previous read has been finished
                        ftdi.write(tx_buffer);
                        put_rx_messages(msg_queue);
                    }
                }
                catch (...) {
                    for (std::deque<TSmartPtr<RxMessage> >::const_iterator it = msg_queue.begin();
                         it != msg_queue.end(); ++it) {
                        (*it)->packet->state = EPacketFail;
                        NotifyListerners((*it)->packet);
                    }
                }
                tx_buffer.clear();
            }
            else if (!tx_queue_write.empty())
            {
                TSmartPtr<CTransferPacket> packet(tx_queue_write.front());
                tx_queue_write.pop_front();
                lock.Unlock();
                SysLog()->LogDebug(LOG_CONTEXT, "get packet id " FSIZE_T " from TX write queue", packet->id);
                msg_queue.push_back(prepare_data(packet, false));
            }
            else if (!tx_queue.empty())
            {
                TSmartPtr<CTransferPacket> packet(tx_queue.front());
                tx_queue.pop_front();
                lock.Unlock();
                SysLog()->LogDebug(LOG_CONTEXT, "get packet id " FSIZE_T " from TX queue", packet->id);
                msg_queue.push_back(prepare_data(packet));
            }
        }
        SysLog()->LogDebug(LOG_CONTEXT, "TX thread finished");
    }

    void CI2CDriver::put_tx_packet(TSmartPtr<CTransferPacket>& packet, bool write)
    {
        bool put = false;
        while(!put){
            armcpp11::UniqueLock<armcpp11::Mutex> lock(tx_mutex);
            if (write) {
                tx_queue_write.push_back(packet);
                lock.Unlock();
                tx_cv.NotifyAll();
                put = true;
                SysLog()->LogDebug(LOG_CONTEXT, "put packet id " FSIZE_T " in TX write queue", packet->id);
            }
            else if (tx_queue.size() < maximum_tx_queue){
                tx_queue.push_back(packet);
                lock.Unlock();
                tx_cv.NotifyAll();
                put = true;
                SysLog()->LogDebug(LOG_CONTEXT, "put packet id " FSIZE_T " in TX queue", packet->id);
            }
            else {
                lock.Unlock();
                armcpp11::ThisThread::SleepFor(armcpp11::MilliSeconds(1));
            }
        }
    }

    void CI2CDriver::set_page(RxMessage& msg, UInt32 addr)
    {
        int required_page = addr >> page_bits;
        if (required_page != current_page){
            SysLog()->LogDebug(LOG_CONTEXT, "set page: %d", required_page);
            UInt8 subpage = (UInt8)(required_page&0xFF);
            put_chunk_write(msg,(UInt8)(required_page>>8)|0x80,&subpage,1);
            current_page = required_page;
        }
    }

    void CI2CDriver::prepare_data_write(RxMessage& msg)
    {
        UInt32 req_size = (UInt32)msg.packet->data.size();
        UInt32 addr = msg.packet->address;
        UInt8* ptr = msg.packet->data.data();
        tx_buffer_inx = tx_buffer.size();
        put_start();        // here we start
        while (req_size) {
            UInt8 addr_in_page = (UInt8)(addr & page_mask);
            UInt32 chunk = (((addr + req_size-1) >> page_bits) == (addr >> page_bits)) ? req_size : (((addr + page_size) & ~page_mask) - addr);
            set_page(msg, addr);
            req_size -= chunk;
            put_chunk_write(msg, addr_in_page,ptr,chunk,req_size == 0);
            ptr += chunk;
            addr += chunk;
        }
        if (bitbang)
            msg.size = tx_buffer.size() - tx_buffer_inx;
    }

    void CI2CDriver::prepare_data_read(RxMessage& msg)
    {
        UInt32 req_size = (UInt32)msg.packet->data.size();
        UInt32 addr = msg.packet->address;
        UInt8* ptr = msg.packet->data.data();
        tx_buffer_inx = tx_buffer.size();
        put_start();        // here we start
        while (req_size) {
            UInt8 addr_in_page = (UInt8)(addr & page_mask);
            UInt32 chunk = (((addr + req_size-1) >> page_bits) == (addr >> page_bits)) ? req_size : (((addr + page_size) & ~page_mask) - addr);
            set_page(msg, addr);
            put_chunk_write(msg, addr_in_page,0,0);
            req_size -= chunk;
            put_chunk_read(msg, chunk, req_size == 0);
            ptr += chunk;
            addr += chunk;
        }
        if (bitbang)
            msg.size = tx_buffer.size() - tx_buffer_inx;
    }

    void CI2CDriver::parse_rec_data(RxMessage& msg, const UInt8* buf, basesize size)
    {
        msg.packet->state = EPacketFail;
        if (size != msg.size){
            SysLog()->LogError(LOG_CONTEXT, "RX buffer has less data than requested");
        }
        else if (bitbang) {
            for (vector<basesize>::iterator ack = msg.ack_position.begin(); ack != msg.ack_position.end(); ++ack){
                if (buf[*ack] & bitbang_sda_in) {
                    msg.packet->state = EPacketNoAck;
                    SysLog()->LogError(LOG_CONTEXT, "device acknowledge error at position: " FSIZE_T, *ack);
                    return;
                }
            }
            for (basesize cnt = 0; cnt < msg.byte_position.size(); cnt++){
                basesize byte_start = msg.byte_position[cnt];
                UInt8 byte = 0;
                for (int i = 0; i < 8; i++){
                    byte <<= 1;
                    if (buf[byte_start+scl_phase_num*i] & bitbang_sda_in) {
                        byte |= 1;
                    }
                }
                if (msg.read_modify_write) {
                    msg.packet->data[cnt] = (msg.packet->data[cnt] & msg.packet->mask[cnt]) | (byte & ~msg.packet->mask[cnt]);
                }
                else {
                    msg.packet->data[cnt] = byte;
                }
            }
            msg.packet->state = EPacketSuccess;
        }
        else {
            for (vector<basesize>::iterator ack = msg.ack_position.begin(); ack != msg.ack_position.end(); ++ack){
                if (buf[*ack] & 0x01) {
                    msg.packet->state = EPacketNoAck;
                    SysLog()->LogError(LOG_CONTEXT, "device acknowledge error at position: " FSIZE_T, *ack);
                    return;
                }
            }
            for (basesize cnt = 0; cnt < msg.byte_position.size(); cnt++){
                UInt8 byte = buf[msg.byte_position[cnt]];
                if (msg.read_modify_write) {
                    msg.packet->data[cnt] = (msg.packet->data[cnt] & msg.packet->mask[cnt]) | (byte & ~msg.packet->mask[cnt]);
                }
                else {
                    msg.packet->data[cnt] = byte;
                }
            }
            msg.packet->state = EPacketSuccess;
        }
        SysLog()->LogDebug(LOG_CONTEXT, "successful parsing of request data, id " FSIZE_T, msg.packet->id);
    }

    void CI2CDriver::put_chunk_write(RxMessage& msg, UInt8 addr, const UInt8* data, UInt32 size, bool last)
    {
        SysLog()->LogDebug(LOG_CONTEXT, "write chunk: addr: 0x%X size: " FSIZE_T, addr, size);
        put_byte(msg, dev_address << 1);
        put_byte(msg, addr);
        while (size--) {
            put_byte(msg, *data++);
        }
        if (last) put_stop();
        else put_cont_start();
    }

    void CI2CDriver::put_chunk_read(RxMessage& msg, UInt32 size, bool last)
    {
        SysLog()->LogDebug(LOG_CONTEXT, "read chunk: size: " FSIZE_T, size);
        put_byte(msg, (dev_address << 1) | 1);
        while (size--) {
            get_byte(msg, !size);
        }
        if (last) put_stop();
        else put_cont_start();
    }

    void CI2CDriver::put_start()
    {
        if (bitbang){
            tx_buffer.push_back(bitbang_scl);
        }
        else {
            mpsse_led(true);

            mpsse_out(true,false);          // SCL - high, SDA - low
            mpsse_out(false,false);         // SCL - low, SDA - low
            //tx_buffer.push_back(0x89);    // test command - wait for pin ADBUS5 going low
        }
    }

    void CI2CDriver::put_stop()
    {
        if (bitbang){
            tx_buffer.push_back(0);
            tx_buffer.push_back(bitbang_scl);
            tx_buffer.push_back(bitbang_scl|bitbang_sda_out);
        }
        else {
            mpsse_led(false);

            mpsse_out(false,false);     // SCL - low, SDA - low
            mpsse_out(true,false);      // SCL - high, SDA - low
            mpsse_out(true,true);       // SCL - high, SDA - high
        }
    }

    void CI2CDriver::put_cont_start()
    {
        if (bitbang) {
            tx_buffer.push_back(bitbang_sda_out);
            tx_buffer.push_back(bitbang_scl|bitbang_sda_out);
            tx_buffer.push_back(bitbang_scl);
            tx_buffer.push_back(0);
        }
        else {
            mpsse_out(false,true);      // SCL - low, SDA - high
            mpsse_out(true,true);       // SCL - high, SDA - high
            mpsse_out(true,false);      // SCL - high, SDA - low
            mpsse_out(false,false);     // SCL - low, SDA - low
        }
    }

    void CI2CDriver::put_byte(RxMessage& msg, UInt8 byte)
    {
        if (bitbang) {
            for (int i = 0; i < 8; i++) {
                UInt8 bit = (byte&0x80) ? bitbang_sda_out : 0;
                put_bit(bit);
                byte <<= 1;
            }
        }
        else {
            tx_buffer.push_back(FTDI_SEND_BYTE);
            tx_buffer.push_back(0);         // 0 - only 1 byte
            tx_buffer.push_back(0);
            tx_buffer.push_back(byte);

            //mpsse_out(false,true);        // SCL - low, SDA - high === ??? Do we realy need this ???
        }
        // get ACK
        get_bit(msg);
    }

    void CI2CDriver::put_bit(UInt8 bit)
    {
        if (bitbang) {
            tx_buffer.push_back(bit);
            tx_buffer.push_back(bit|bitbang_scl);
            tx_buffer.push_back(bit);
        }
        else {
            tx_buffer.push_back(FTDI_SEND_BIT);
            tx_buffer.push_back(0);     // 0 - only one bit
            tx_buffer.push_back(bit ? 0xFF : 0);
        }
    }

    void CI2CDriver::get_bit(RxMessage& msg)
    {
        if (bitbang) {
            msg.ack_position.push_back(tx_buffer.size()-tx_buffer_inx+bitbang_delay);
            put_bit(bitbang_sda_out);
        }
        else {
            tx_buffer.push_back(FTDI_REC_BIT);
            tx_buffer.push_back(0); // 0 - only one bit
            msg.ack_position.push_back(msg.size++);
        }
    }

    void CI2CDriver::get_byte(RxMessage& msg, bool last)
    {
        if (bitbang) {
            msg.byte_position.push_back(tx_buffer.size()-tx_buffer_inx+bitbang_delay);
            for (int i = 0; i < 8; i++) {
                put_bit(bitbang_sda_out);
            }
        }
        else {
            //mpsse_out(false,true);        // SCL - low, SDA - high === ??? Do we realy need this ???

            tx_buffer.push_back(FTDI_REC_BYTE);
            tx_buffer.push_back(0); // 0 - only one byte
            tx_buffer.push_back(0);
            msg.byte_position.push_back(msg.size++);
        }
        // ACK
        put_bit(last ? bitbang_sda_out : 0);
    }

    void CI2CDriver::mpsse_out(bool scl, bool sda)
    {
        UInt8 out = 0x7C;
        if (scl) out |= 0x01;   // SCL is ADBUS0
        if (sda) out |= 0x02;   // SDA output is ADBUS1
        if (!led_status) out |= 0x80;   // LED output is ADBUS7, ON = GND
        tx_buffer.push_back(FTDI_SET_BITS_LOW);
        tx_buffer.push_back(out);
        tx_buffer.push_back(mpsse_dir);
    }

    void CI2CDriver::mpsse_led(bool on)
    {
        led_status = on;
        //tx_buffer.push_back(FTDI_SET_BITS_HIGH);
        //tx_buffer.push_back(on ? 0xBF : 0xFF);  // red LED is ACBUS6, 0 - on, 1 - off
        //tx_buffer.push_back(0x40);              // only red LED (ACBUS6) is out
    }

}
#endif // !_ANDROID
