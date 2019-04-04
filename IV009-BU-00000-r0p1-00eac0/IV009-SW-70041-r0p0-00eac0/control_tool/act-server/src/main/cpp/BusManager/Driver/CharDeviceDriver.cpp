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

#ifndef _WIN32

#include <unistd.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/types.h>
#include <limits.h>

#include <ATL/ATLConfig.h>

#define LOG_CONTEXT "CharDeviceDriver"
#include "CharDeviceDriver.h"

using namespace std;
using namespace atl ;

static const std::string DEV_ISP = "/dev/ac_isp";
static std::map<std::string, std::string> defaults;

static void CreateMap()
{
    defaults["dev"] = DEV_ISP;
}

namespace act {

    CCharDeviceDriver::~CCharDeviceDriver() {
        if (rx_thread) Close();
    }

    const vector<string> CCharDeviceDriver::GetDeviceList() {
        vector<string> devices;
        devices.push_back(DEV_ISP);
        return devices;
    }

    const vector<EPacketCommand> CCharDeviceDriver::GetCommandList() {
        vector<EPacketCommand> commands;
        commands.push_back(EPacketTransaction);
        return commands;
    }

    const vector<EACTDriverMode> CCharDeviceDriver::GetSupportedModes() {
        vector<EACTDriverMode> modes;
        modes.push_back(EACTDriverASyncMode);
        return modes;
    }

    const EACTDriverMode CCharDeviceDriver::GetCurrentMode() {
        return EACTDriverASyncMode;
    }

    CATLError CCharDeviceDriver::Initialize(const flags32& options) {
        CreateMap();
        string prefix = getBusPrefix(options);
        SysConfig()->Merge(prefix, defaults);
        if (CATLError::GetLastError().IsError()) {
            return CATLError::GetLastError();
        }
        dev_name = SysConfig()->GetValue(prefix + "dev");
        return EATLErrorOk;
    }

    CATLError CCharDeviceDriver::Open() {
        CATLError result;
        if (!rx_thread) {
            if( result.IsOk() ) {
                if (!dev_name.empty()) {
                    chardev = open(dev_name.c_str(), O_RDWR | O_SYNC);
                    if (chardev >= 0) {
                        create_rx_thread();    // all right, can run the thread now
                        SysLog()->LogInfo(LOG_CONTEXT, "successfully opened device %s", dev_name.c_str()) ;
                    } else {
                        result = EATLErrorFatal;
                        SysLog()->LogError(LOG_CONTEXT, "can't open device %s", dev_name.c_str()) ;
                    }
                } else {
                    result = EATLErrorInvalidParameters;
                    SysLog()->LogError(LOG_CONTEXT, "invalid device name %s", dev_name.c_str());
                }
            }
        } else {
            result = EATLErrorFatal;
            SysLog()->LogError(LOG_CONTEXT, "connection to %s is already opened", dev_name.c_str());
        }
        return result ;
    }

    CATLError CCharDeviceDriver::Close() {
        CATLError result;
        if (rx_thread){
            stop_rx_thread();
            close(chardev);
            armcpp11::ThisThread::SleepFor(armcpp11::MilliSeconds(100));
        } else {
            result = EATLErrorFatal ;
            SysLog()->LogError(LOG_CONTEXT, "char device %s is already closed", dev_name.c_str());
        }
        return result;
    }

    CATLError CCharDeviceDriver::Terminate() {
        CATLError result;
        if (rx_thread) result = Close();
        return result;
    }

    CATLError CCharDeviceDriver::ProcessPacket(TSmartPtr< CTransferPacket > packet, const flags32&) {
        if (rx_thread) {
            switch (packet->command){
            case EPacketTransaction:
                return PushTransaction(packet);
            default:
                SysLog()->LogError(LOG_CONTEXT, "invalid command %d for char device", packet->command);
                packet->state = EPacketInvalid;
                NotifyListerners( packet ) ;
                return EATLErrorInvalidParameters;
            }
        } else {
            SysLog()->LogError(LOG_CONTEXT, "attemptted to access driver which is not configured");
            packet->state = EPacketFail;
            NotifyListerners( packet ) ;
            return EATLErrorNotInitialized;
        }
    }

    CATLError CCharDeviceDriver::PushTransaction(TSmartPtr<CTransferPacket>& packet) {
        CATLError result;
        if (packet->data.size() > 8){
            SysLog()->LogDebug(LOG_CONTEXT, "sending transaction: id: " FSIZE_T " size: " FSIZE_T,packet->id, packet->data.size());
            UInt32 id = (UInt32)(packet->id&0xFFFFFFFF);
            packet->state = EPacketProcessing;
            packet->SetValue((UInt32)packet->data.size(),0);
            packet->SetValue(id,4);
            // put packet to the RX list
            armcpp11::UniqueLock<armcpp11::Mutex> rx_lock(rx_mutex);
            transfer_list.insert(list_type::value_type(id,CTransaction(packet)));
            rx_lock.Unlock();

            const std::size_t dataSize = packet->data.size();
            const std::size_t writeCount = write(chardev,
                                                 packet->data.data(),
                                                 packet->data.size());
            if (writeCount != dataSize) {
                SysLog()->LogError(LOG_CONTEXT,
                                   "can't send transaction: id: " FSIZE_T
                                   " data size: " FSIZE_T
                                   " write count: " FSIZE_T,
                                   packet->id, dataSize, writeCount);
                packet->state = EPacketInvalid;
                result = EATLErrorFatal;
            }
        } else {
            packet->state = EPacketInvalid;
            NotifyListerners(packet);
            result = EATLErrorInvalidParameters;
        }
        return result ;
    }

    static UInt32 nonblockread(void *ptr, const size_t size, const int fd)
    {
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100 * 1000;
        fd_set  fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        select(fd + 1, &fds, NULL, NULL, &tv);
        if (FD_ISSET(fd, &fds)) {
            int count = read(fd, ptr, size);
            return (count > 0) ? count : 0;
        }
        else {
            return 0;
        }
    }

    // RX thread
    void CCharDeviceDriver::rx_thread_processing() {
        vector<UInt8> buffer(4);
        buffer.reserve(2048);
        UInt32 buf_pos = 0;
        bool wait_header = true;
        UInt32 full_size = 0;

        SysLog()->LogDebug(LOG_CONTEXT, "RX thread started");
        while(!rx_thread_stop) {
            UInt32 n;
            if (wait_header) { // header
                n = nonblockread(buffer.data() + buf_pos, 4 - buf_pos, chardev);
                if (n > 0) {
                    buf_pos += n;
                    if (buf_pos == 4){
                        full_size = (UInt32)(buffer[0]) | ((UInt32)buffer[1] << 8) | ((UInt32)buffer[2] << 16) | ((UInt32)buffer[3] << 24);
                        try {
                            buffer.resize(full_size);
                            wait_header = false;
                        }
                        catch (bad_alloc) {
                            SysLog()->LogError(LOG_CONTEXT, "can't resize buffer for data: requested size: " FSIZE_T, full_size);
                            buf_pos = 0; // reset buffer to beginning
                        }
                    }
                }
            } else { // data
                n = nonblockread(buffer.data() + buf_pos,
                                 std::min(full_size-buf_pos,(UInt32)INT_MAX),
                                 chardev);
                if (n > 0) {
                    buf_pos += n;
                    if (buf_pos == full_size){ // packet is received
                        UInt32 id = (UInt32)(buffer[4]) | ((UInt32)buffer[5] << 8) | ((UInt32)buffer[6] << 16) | ((UInt32)buffer[7] << 24);
                        SysLog()->LogDebug(LOG_CONTEXT, "received response id " FSIZE_T " size " FSIZE_T, id, full_size);
                        armcpp11::UniqueLock<armcpp11::Mutex> rx_lock(rx_mutex);
                        list_type::iterator it = transfer_list.find(id);
                        if (it != transfer_list.end()){
                            TSmartPtr<CTransferPacket> packet = it->second.packet;    // get packet from the list
                            transfer_list.erase(it);    // remove from the list
                            rx_lock.Unlock();
                            packet->data = buffer;
                            packet->state = EPacketSuccess;
                            NotifyListerners(packet) ;
                        }
                        else {
                            rx_lock.Unlock();
                            SysLog()->LogError(LOG_CONTEXT, "can't find transfer with id " FSIZE_T, id);
                        }
                        buf_pos = 0;
                        wait_header = true;
                    }
                }
            }
            // check timeouts
            armcpp11::HighResolutionClock::timepoint now = armcpp11::HighResolutionClock::Now();
            armcpp11::UniqueLock<armcpp11::Mutex> rx_lock(rx_mutex);
            for (list_type::iterator it = transfer_list.begin(); it != transfer_list.end();) {
                const CTransaction& transaction = it->second;
                if (armcpp11::DurationCast<armcpp11::MilliSeconds>(now - transaction.sent_time).Count() > transaction.packet->timeout_ms) { // timeout for packet
                    TSmartPtr<CTransferPacket> packet = it->second.packet;    // get packet from the list
                    list_type::iterator tmp = it++;
                    transfer_list.erase(tmp);    // remove transaction from the list with iterator repositioning
                    packet->state = EPacketNoAnswer;
                    SysLog()->LogError(LOG_CONTEXT, "timeout for transaction id " FSIZE_T, packet->id); // unfortunatelly has to call log and notify listeners withing mutex lock
                    NotifyListerners(packet);
                } else {
                    ++it; // normal iterator increment
                }
            }
        }
        SysLog()->LogDebug(LOG_CONTEXT, "RX thread stopped");
    }

    void *CCharDeviceDriver::rx_thread_wrapper(void *arg)
    {
        ((CCharDeviceDriver*)arg)->rx_thread_processing();
        return NULL;
    }

    void CCharDeviceDriver::create_rx_thread(void) {
        rx_thread_stop = false;
        rx_thread = new armcpp11::Thread(&CCharDeviceDriver::rx_thread_wrapper,this);
    }

    void CCharDeviceDriver::stop_rx_thread(void) {
        if (rx_thread) {
            rx_thread_stop = true;
            if (rx_thread && rx_thread->Joinable()) rx_thread->Join();
            delete rx_thread;
            rx_thread = 0;
        }
    }
}

#endif // _WIN32
