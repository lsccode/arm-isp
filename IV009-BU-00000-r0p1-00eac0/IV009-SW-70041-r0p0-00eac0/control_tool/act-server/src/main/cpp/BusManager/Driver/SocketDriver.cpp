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
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define closesocket close

#include <limits.h>

#include <ATL/ATLConfig.h>

#define LOG_CONTEXT "SocketDriver"
#include "SocketDriver.h"

using namespace std;
using namespace atl;

static const string DEFAULT_SOCKET = "localhost:30000";
static map<string,string> defaults;

static void CreateMap()
{
    defaults["dev"] = DEFAULT_SOCKET;
}

namespace act {
    CSocketDriver::CSocketDriver() : rx_thread(0) { CreateMap();}

    CSocketDriver::~CSocketDriver() {
        if (rx_thread) Close();
    }

    // IATLObject interface
    const string CSocketDriver::GetObjectStaticName() {
        return LOG_CONTEXT;
    }

    // IATLComponent
    const string CSocketDriver::GetName() {
        return "socket";
    }

    const vector<string> CSocketDriver::GetDeviceList() {
        vector<string> devices;    // create possible socket names
        static const char default_sock_name[] = "/tmp/fw_socket";
        if (access(default_sock_name, F_OK) != -1){
            devices.push_back(default_sock_name);
        }
        devices.push_back(DEFAULT_SOCKET);
        return devices;
    }

    const vector<EPacketCommand> CSocketDriver::GetCommandList() {
        vector<EPacketCommand> commands;
        commands.push_back(EPacketTransaction);
        return commands;
    }

    const vector<EACTDriverMode> CSocketDriver::GetSupportedModes() {
        vector<EACTDriverMode> modes;
        modes.push_back(EACTDriverASyncMode);
        return modes;
    }

    const EACTDriverMode CSocketDriver::GetCurrentMode() {
        return EACTDriverASyncMode;
    }

    static int set_socket_timeout(SOCKET fd, int timeout_ms)
    {
        int rc;
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        timeout_ms = timeout_ms - tv.tv_sec * 1000;
        tv.tv_usec = timeout_ms * 1000;
        rc = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char*>(&tv), sizeof(tv));
        if (rc) {
            char err_buf[128];
            SysLog()->LogError(LOG_CONTEXT, "setsockopt error %d (%s)", errno, __strerror(errno, err_buf, 128));
        }
        return rc;
    }

    static bool is_timeout(void)
    {
        return errno == EAGAIN;
    }

    CATLError CSocketDriver::Initialize(const flags32& options)
    {
        string prefix = getBusPrefix(options);
        SysConfig()->Merge(prefix, defaults);
        if (CATLError::GetLastError().IsError()) {
            return CATLError::GetLastError();
        }
        dev_name = SysConfig()->GetValue(prefix + "dev");
        return EATLErrorOk;
    }

    CATLError CSocketDriver::Open() {
        CATLError result;
        if (!rx_thread) {
            if (CATLError::GetLastError() == EATLErrorOk  && !dev_name.empty()) {
                basesize semicolon_pos = dev_name.find(':'); // try to understand is it a TCP/IP socket or UNIX socket
                if (semicolon_pos == string::npos) {    // no semicolon - UNIX socket
                    soc = socket(AF_UNIX,SOCK_STREAM,0);
                    if (soc != -1) {
                        /* receiver thread should wake up every 100 ms */
                        if (!set_socket_timeout(soc, 100)) {
                            struct sockaddr_un remote;
                            remote.sun_family = AF_UNIX;
                            strcpy(remote.sun_path, dev_name.c_str());
                            if (connect(soc, (struct sockaddr *)&remote, strlen(remote.sun_path) + sizeof(remote.sun_family)) == 0) {
                                create_rx_thread();
                                SysLog()->LogInfo(LOG_CONTEXT, "established connection with %s", dev_name.c_str());
                            }
                            else {
                                result = EATLErrorFatal;
                                SysLog()->LogError(LOG_CONTEXT, "can't connect to %s", dev_name.c_str());
                            }
                        }
                        else {
                            result = EATLErrorFatal;
                            SysLog()->LogError(LOG_CONTEXT, "can't set read timeout to socket %s", dev_name.c_str());
                        }
                    }
                    else {
                        result = EATLErrorFatal;
                        SysLog()->LogError(LOG_CONTEXT, "can't create socket to %s", dev_name.c_str());
                    }
                } else {
                    soc = socket(AF_INET,SOCK_STREAM,0);    // TCP/IP socket
                    if (soc > 0) { // works for both Windows and Linux
                        /* receiver thread should wake up every 100 ms */
                        if (!set_socket_timeout(soc, 100)) {
                            sockaddr_in service;
                            service.sin_family = AF_INET;
                            inet_pton(AF_INET,dev_name.substr(0,semicolon_pos).c_str(),&service.sin_addr.s_addr);   // set IP address
                            service.sin_port = htons(atoi(dev_name.substr(semicolon_pos+1).c_str()));               // set port
                            if (connect(soc, (struct sockaddr *) &service, sizeof (service)) == 0){                 // connect to FW
                                create_rx_thread();    // all right, can run the thread now
                                SysLog()->LogInfo(LOG_CONTEXT, "established connection with %s device", dev_name.c_str());
                            }
                            else {
                                result = EATLErrorFatal;
                                SysLog()->LogError(LOG_CONTEXT, "can't connect to %s", dev_name.c_str());
                            }
                        }
                        else {
                            result = EATLErrorFatal;
                            SysLog()->LogError(LOG_CONTEXT, "can't set read timeout to socket to %s", dev_name.c_str());
                        }
                    }
                    else {
                        result = EATLErrorFatal;
                        SysLog()->LogError(LOG_CONTEXT, "can't create socket to %s", dev_name.c_str());
                    }
                }
            }
            else {
                result = EATLErrorFatal;
                SysLog()->LogError(LOG_CONTEXT, "invalid device name: %s", dev_name.c_str());
            }
        }
        else {
            result = EATLErrorFatal;
            SysLog()->LogError(LOG_CONTEXT, "connection to %s is already opened", dev_name.c_str());
        }
        return result;
    }

    CATLError CSocketDriver::Close()
    {
        CATLError result;
        if (rx_thread) {
            stop_rx_thread();
            closesocket(soc);
            armcpp11::ThisThread::SleepFor(armcpp11::MilliSeconds(100)); // Need some time so FW will understand that socket is closed
        }
        else {
            result = EATLErrorFatal;
            SysLog()->LogError(LOG_CONTEXT, "device %s is already closed", dev_name.c_str());
        }
        return result;
    }

    CATLError CSocketDriver::Terminate() {
        CATLError result;
        if (rx_thread) Close();
        return result;
    }

    CATLError CSocketDriver::ProcessPacket(TSmartPtr< CTransferPacket > packet, const flags32&) {
        if (rx_thread) {
            switch (packet->command){
            case EPacketTransaction:
                return PushTransaction(packet);
            default:
                SysLog()->LogError(LOG_CONTEXT, "invalid command %d for socket driver", packet->command);
                packet->state = EPacketInvalid;
                NotifyListerners( packet );
                return EATLErrorInvalidParameters;
            }
        }
        else {
            SysLog()->LogError(LOG_CONTEXT, "attemptted to access driver which is not configured");
            packet->state = EPacketFail;
            NotifyListerners(packet);
            return EATLErrorNotInitialized;
        }
    }

    CATLError CSocketDriver::PushTransaction(TSmartPtr<CTransferPacket>& packet) {
        CATLError result;
        if (packet->data.size() > 8) {
            SysLog()->LogDebug(LOG_CONTEXT, "sending transaction: id: " FSIZE_T " size: " FSIZE_T,packet->id, packet->data.size());
            UInt32 id = (UInt32)(packet->id&0xFFFFFFFF);
            packet->state = EPacketProcessing;
            packet->SetValue((UInt32)packet->data.size(),0);
            packet->SetValue(id,4);
            // put packet to the RX list
            armcpp11::UniqueLock<armcpp11::Mutex> rx_lock(rx_mutex);
            transfer_list.insert(list_type::value_type(id,CTransaction(packet)));
            rx_lock.Unlock();
            armcpp11::UniqueLock<armcpp11::Mutex> tx_lock(tx_mutex);
            if (send(soc,reinterpret_cast<char*>(packet->data.data()),(int)packet->data.size(),0) != (int)packet->data.size()) {
                tx_lock.Unlock();
                SysLog()->LogError(LOG_CONTEXT, "can't send transaction: id: " FSIZE_T " size: " FSIZE_T, packet->id, packet->data.size());
                packet->state = EPacketInvalid;
                result = EATLErrorFatal;
            }
        }
        else {
            packet->state = EPacketInvalid;
            NotifyListerners( packet );
            result = EATLErrorInvalidParameters;
        }
        return result;
    }

    // RX thread
    void CSocketDriver::rx_thread_processing()
    {
        vector<UInt8> buffer(4);
        buffer.reserve(2048);
        UInt32 buf_pos = 0;
        bool wait_header = true;
        UInt32 full_size = 0;

        SysLog()->LogDebug(LOG_CONTEXT, "RX thread started");
        while(!rx_thread_stop) {
            int n;
            if (wait_header) { // header
                n = recv(soc,reinterpret_cast<char*>(buffer.data()+buf_pos),4-(int)buf_pos,0);
                if (n < 0) {
                    if (is_timeout()) {
                        /* timeout is used to poll @rx_thread_stop */
                    } else {
                        SysLog()->LogError(LOG_CONTEXT, "reading from socket %s failed", dev_name.c_str());
                        buf_pos = 0;
                        wait_header = true;
                    }
                }
                else{
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
            }
            else { // data
                n = recv(soc,reinterpret_cast<char*>(buffer.data()+buf_pos),(int)min(full_size-buf_pos,(UInt32)INT_MAX),0);
                if (n < 0) {
                    if (is_timeout()) {
                        /* timeout is used to poll @rx_thread_stop */
                    }
                    else {
                        SysLog()->LogError(LOG_CONTEXT, "reading from socket %s failed", dev_name.c_str());
                        buf_pos = 0;
                        wait_header = true;
                    }
                }
                else{
                    buf_pos += n;
                    if (buf_pos == full_size){ // packet is received
                        UInt32 id = (UInt32)(buffer[4]) | ((UInt32)buffer[5] << 8) | ((UInt32)buffer[6] << 16) | ((UInt32)buffer[7] << 24);
                        SysLog()->LogDebug(LOG_CONTEXT, "received response id " FSIZE_T " size " FSIZE_T, id, full_size);
                        armcpp11::UniqueLock<armcpp11::Mutex> rx_lock(rx_mutex);
                        list_type::iterator it = transfer_list.find(id);
                        if (it != transfer_list.end()){
                            TSmartPtr<CTransferPacket> packet = it->second.packet;    // get packet from the list
                            list_type::iterator tmp = it++;
                            transfer_list.erase(tmp);    // remove from the list
                            rx_lock.Unlock();
                            packet->data = buffer;
                            packet->state = EPacketSuccess;
                            NotifyListerners(packet);
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
                }
                else {
                    ++it; // normal iterator increment
                }
            }
        }
        SysLog()->LogDebug(LOG_CONTEXT, "RX thread stopped");
    }

    void* CSocketDriver::rx_thread_wrapper(void *arg)
    {
        ((CSocketDriver*)arg)->rx_thread_processing();
        return NULL;
    }

    void CSocketDriver::create_rx_thread(void)
    {
        rx_thread_stop = false;
        rx_thread = new armcpp11::Thread(&CSocketDriver::rx_thread_wrapper,this);
    }

    void CSocketDriver::stop_rx_thread(void)
    {
        if (rx_thread) {
            rx_thread_stop = true;
            if (rx_thread && rx_thread->Joinable()) rx_thread->Join();
            delete rx_thread;
            rx_thread = 0;
        }
    }
}

#endif // _WIN32
