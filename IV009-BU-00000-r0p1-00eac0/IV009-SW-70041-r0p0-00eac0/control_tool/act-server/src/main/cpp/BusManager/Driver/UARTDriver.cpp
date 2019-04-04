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

#ifdef _WIN32
#include <windows.h>
#define close CloseHandle
static int read(HANDLE fd, char* data, int size)
{
    DWORD read_bytes = 0;
    if (!ReadFile(fd,data,size,&read_bytes,NULL)) return -1;
    return (int)read_bytes;
}

static int write(HANDLE fd, const char* data, int size)
{
    DWORD written_bytes = 0;
    if (!WriteFile(fd,data,size,&written_bytes,NULL)) return -1;
    return (int)written_bytes;
}

static void fsync(HANDLE fd)
{
}

#else // _WIN32
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>
#endif // _WIN32

#include <limits.h>

#include <ATL/ATLConfig.h>

#define LOG_CONTEXT "UARTDriver"
#include "UARTDriver.h"

using namespace std;
using namespace atl;

#ifdef _WIN32
static const string DEFAULT_DEVICE = "COM1";
#else
static const string DEFAULT_DEVICE = "/dev/ttyUSB0";
#endif

static std::map<std::string, std::string> defaults;

static void CreateMap()
{
    defaults["dev"] =       DEFAULT_DEVICE;    // device name
    defaults["baud-rate"] = "115200";          // baud rate
}

namespace act {

    CUARTDriver::CUARTDriver() : rx_thread(0) { CreateMap(); }

    CUARTDriver::~CUARTDriver() {
        if (rx_thread) Close();
    }

    const string CUARTDriver::GetObjectStaticName() {
        return LOG_CONTEXT;
    }

    const string CUARTDriver::GetName() {
        return "uart";
    }

    const vector< string > CUARTDriver::GetDeviceList()
    {
        vector<string> devices;    // create possible UART names
        devices.push_back(DEFAULT_DEVICE);
        return devices;
    }

    const vector<EPacketCommand> CUARTDriver::GetCommandList()
    {
        vector<EPacketCommand> commands;
        commands.push_back(EPacketTransaction);
        return commands;
    }

    const vector<EACTDriverMode> CUARTDriver::GetSupportedModes()
    {
        vector<EACTDriverMode> modes;
        modes.push_back(EACTDriverASyncMode);
        return modes;
    }

    const EACTDriverMode CUARTDriver::GetCurrentMode()
    {
        return EACTDriverASyncMode;
    }

    static int set_uart_param(UART fd, UInt32 baudrate)
    {
#ifdef _WIN32
        static const DWORD speed_map[][2] = {{9600,CBR_9600},{19200,CBR_19200},{38400,CBR_38400},{57600,CBR_57600},{115200,CBR_115200}};
        DWORD speed = 0;
#else
        static const UInt32 speed_map[][2] = {{9600,B9600},{19200,B19200},{38400,B38400},{57600,B57600},{115200,B115200},{230400,B230400}};
        UInt32 speed = 0;
#endif
        for (basesize i = 0; i < array_size(speed_map); i++){
            if (speed_map[i][0] == baudrate) {
                speed = speed_map[i][1];
                break;
            }
        }
        if (speed == 0){
            SysLog()->LogError(LOG_CONTEXT, "can't support non-standart UART baud rate %d", baudrate);
            return -1;
        }

#ifdef _WIN32
        DCB dcb;
        dcb.DCBlength = sizeof(DCB);
        GetCommState(fd, &dcb);
        dcb.BaudRate = speed;
        dcb.fBinary = TRUE;
        dcb.fParity = FALSE;
        dcb.fOutxCtsFlow = FALSE;
        dcb.fOutxDsrFlow = FALSE;
        dcb.fDtrControl = DTR_CONTROL_DISABLE;
        dcb.fRtsControl = RTS_CONTROL_DISABLE;
        dcb.fOutX = FALSE;
        dcb.fInX = FALSE;
        dcb.fNull = FALSE;
        dcb.ByteSize = 8;
        dcb.Parity = NOPARITY;
        dcb.StopBits = ONESTOPBIT;
        SetCommState(fd, &dcb);
        COMMTIMEOUTS tm = {0};
        tm.ReadTotalTimeoutConstant = 100;
        SetCommTimeouts(fd, &tm);
#else
        struct termios param;
        tcgetattr(fd,&param);
        cfsetspeed(&param,speed);
        param.c_iflag |= IGNBRK | IGNCR | CREAD | CLOCAL;
        param.c_iflag &= ~(INPCK | INLCR | ISTRIP | IXON | IXOFF | PARENB);
        tcsetattr(fd,TCSANOW,&param);
#endif
        return 0;
    }
    static bool is_timeout(void)
    {
#ifdef _WIN32
        int err = WSAGetLastError();
        return err == WSAETIMEDOUT;
#else
        return errno == EAGAIN;
#endif
    }

    CATLError CUARTDriver::Initialize(const flags32& options)
    {
        string prefix = getBusPrefix(options);
        SysConfig()->Merge(prefix, defaults);
        if (CATLError::GetLastError().IsError()) {
            return CATLError::GetLastError();
        }
        dev_name = SysConfig()->GetValue(prefix + "dev");
        baudrate = SysConfig()->GetValueAsU32(prefix + "baud-rate");
        return EATLErrorOk;
    }

    CATLError CUARTDriver::Open()
    {
        CATLError result;
        if (!rx_thread) {
            if (CATLError::GetLastError() == EATLErrorOk  && !dev_name.empty()) {
#ifdef _WIN32
                if ((uart = CreateFile(("\\\\.\\"+dev_name).c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) != INVALID_HANDLE_VALUE) {
#else
                if ((uart = open(dev_name.c_str(),O_RDWR | O_NONBLOCK)) > 0) {
#endif
                    /* receiver thread should wake up every 100 ms */
                    if (!set_uart_param(uart, baudrate)) {
                        SysLog()->LogInfo(LOG_CONTEXT, "Device %s is successfully opened with baud rate %d", dev_name.c_str(), baudrate);
                        create_rx_thread();
                    }
                    else {
                        close(uart);
                        uart = 0;
                        result = EATLErrorFatal;
                        SysLog()->LogError(LOG_CONTEXT, "can't set UART parameters for device %s", dev_name.c_str());
                    }
                }
                else {
                    result = EATLErrorFatal;
                    SysLog()->LogError(LOG_CONTEXT, "can't open UART device %s", dev_name.c_str());
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

    CATLError CUARTDriver::Close()
    {
        CATLError result;
        if (rx_thread){
            stop_rx_thread();
            close(uart);
        }
        else {
            result = EATLErrorFatal;
            SysLog()->LogError(LOG_CONTEXT, "device %s is already closed", dev_name.c_str());
        }
        return result;
    }

    CATLError CUARTDriver::Terminate() {
        CATLError result;
        if (rx_thread) Close();
        return result;
    }

    CATLError CUARTDriver::ProcessPacket(TSmartPtr< CTransferPacket > packet, const flags32&) {
        if (rx_thread) {
            switch (packet->command){
            case EPacketTransaction:
                return PushTransaction(packet);
            default:
                SysLog()->LogError(LOG_CONTEXT, "invalid command %d for UART driver", packet->command);
                packet->state = EPacketInvalid;
                NotifyListerners( packet );
                return EATLErrorInvalidParameters;
            }
        }
        else {
            SysLog()->LogError(LOG_CONTEXT, "attemptted to access driver which is not configured");
            packet->state = EPacketFail;
            NotifyListerners( packet );
            return EATLErrorNotInitialized;
        }
    }

    CATLError CUARTDriver::PushTransaction(TSmartPtr<CTransferPacket>& packet) {
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
            armcpp11::UniqueLock<armcpp11::Mutex> tx_lock(tx_mutex);
            if (write(uart,reinterpret_cast<char*>(packet->data.data()),(int)packet->data.size()) != (int)packet->data.size()) {
                tx_lock.Unlock();
                SysLog()->LogError(LOG_CONTEXT, "can't send transaction: id: " FSIZE_T ", size: " FSIZE_T, packet->id, packet->data.size());
                packet->state = EPacketInvalid;
                result = EATLErrorFatal;
            }
            else {
                fsync(uart);
                tx_lock.Unlock();
                SysLog()->LogDebug(LOG_CONTEXT, "transaction id: " FSIZE_T " is sent successfully",packet->id);
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
    void CUARTDriver::rx_thread_processing(void)
    {
        vector<UInt8> buffer(4);
        buffer.reserve(2048);
        basesize buf_pos = 0;
        bool wait_header = true;
        basesize full_size = 0;

        SysLog()->LogDebug( LOG_CONTEXT, "RX thread started");
        while(!rx_thread_stop) {
            basesize n;
            if (wait_header) { // header
                int k = read(uart,reinterpret_cast<char*>(buffer.data() + buf_pos),4 - (int)buf_pos);
                n = k < 0 ? std::string::npos : (basesize)k;
                if (n == std::string::npos) {
                    if (is_timeout()) {
                        armcpp11::ThisThread::SleepFor(armcpp11::MilliSeconds(10));
                    }
                    else {
                        char err_buf[128];
                        SysLog()->LogError(LOG_CONTEXT, "reading from UART failed: error %d (%s)", errno, __strerror(errno, err_buf, 128));
                        buf_pos = 0;
                        wait_header = true;
                    }
                }
                else if (n) {
                    buf_pos += n;
                    if (buf_pos == 4){
                        full_size = (basesize)(buffer[0]) | ((basesize)buffer[1] << 8) | ((basesize)buffer[2] << 16) | ((basesize)buffer[3] << 24);
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
                int k = read(uart, reinterpret_cast<char*>(buffer.data() + buf_pos), (int)min(full_size - buf_pos, (basesize)INT_MAX));
                n = k < 0 ? std::string::npos : (basesize)k;
                if (n == std::string::npos) {
                    if (is_timeout()) {
                        armcpp11::ThisThread::SleepFor(armcpp11::MilliSeconds(10));
                    }
                    else {
                        char err_buf[128];
                        SysLog()->LogError(LOG_CONTEXT, "reading from UART failed: error %d (%s)", errno, __strerror(errno, err_buf, 128));
                        buf_pos = 0;
                        wait_header = true;
                    }
                }
                else if (n) {
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
                UInt32 past_ms = armcpp11::DurationCast<armcpp11::MilliSeconds>(now - transaction.sent_time).Count();
                if (past_ms > transaction.packet->timeout_ms) { // timeout for packet
                    TSmartPtr<CTransferPacket> packet = it->second.packet;    // get packet from the list
                    list_type::iterator tmp = it++;
                    transfer_list.erase(tmp);    // remove transaction from the list with iterator repositioning
                    packet->state = EPacketNoAnswer;
                    SysLog()->LogError(LOG_CONTEXT, "timeout for transaction id " FSIZE_T " waited %d ms, timeout value %d ms", packet->id, past_ms, transaction.packet->timeout_ms); // unfortunatelly has to call log and notify listeners withing mutex lock
                    NotifyListerners(packet);
                }
                else {
                    ++it; // normal iterator increment
                }
            }
        }
        SysLog()->LogDebug(LOG_CONTEXT, "RX thread stopped");
    }

    void *CUARTDriver::rx_thread_wrapper(void *arg)
    {
        ((CUARTDriver*)arg)->rx_thread_processing();
        return NULL;
    }

    void CUARTDriver::create_rx_thread(void)
    {
        rx_thread_stop = false;
        rx_thread = new armcpp11::Thread(&CUARTDriver::rx_thread_wrapper,this);
    }

    void CUARTDriver::stop_rx_thread(void)
    {
        if (rx_thread) {
            rx_thread_stop = true;
            if (rx_thread && rx_thread->Joinable()) rx_thread->Join();
            delete rx_thread;
            rx_thread = 0;
        }
    }
}
