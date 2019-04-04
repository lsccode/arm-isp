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
#ifndef _ANDROID // this is not supported on Android platform

#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include <cstring>
#include <unistd.h>

#include <ATL/ATLConfig.h>

#define LOG_CONTEXT "SharedMemoryDriver"
#include "SharedMemoryDriver.h"

using namespace std;
using namespace atl;

static const string DEFAULT_SHARED_MEM_DEV = "/tmp/fw_shared_memory";
static std::map<std::string, std::string> defaults;

static void CreateMap()
{
    defaults["dev"] =      DEFAULT_SHARED_MEM_DEV;   // device name
    defaults["mem-size"] = "1048576";                // virtual memory size 1*1024*1024
}

namespace act {

    CSharedMemoryDriver::~CSharedMemoryDriver()
    {
        if (memory_ptr) Close();
    }

    const string CSharedMemoryDriver::GetObjectStaticName()
    {
        return LOG_CONTEXT;
    }

    const string CSharedMemoryDriver::GetName()
    {
        return "sharedmem";
    }

    const vector<string> CSharedMemoryDriver::GetDeviceList()
    {
        vector<string> devices;    // create possible socket names
        devices.push_back(DEFAULT_SHARED_MEM_DEV);
        return devices;
    }

    const vector<EPacketCommand> CSharedMemoryDriver::GetCommandList()
    {
        vector<EPacketCommand> commands;
        commands.push_back(EPacketRead);
        commands.push_back(EPacketWrite);
        return commands;
    }

    const vector<EACTDriverMode> CSharedMemoryDriver::GetSupportedModes()
    {
        vector<EACTDriverMode> modes;
        modes.push_back(EACTDriverSyncMode);
        return modes;
    }

    const EACTDriverMode CSharedMemoryDriver::GetCurrentMode()
    {
        return EACTDriverSyncMode;
    }

    CATLError CSharedMemoryDriver::Initialize(const flags32& options)
    {
        CreateMap();
        string prefix = getBusPrefix(options);
        SysConfig()->Merge(prefix, defaults);
        if (CATLError::GetLastError().IsError()) {
            return CATLError::GetLastError();
        }
        dev_name = SysConfig()->GetValue(prefix + "dev");
        memory_size = SysConfig()->GetValueAsU32(prefix + "mem-size");
        return EATLErrorOk;
    }

    CATLError CSharedMemoryDriver::Open()
    {
        CATLError result;
        if (!memory_ptr) {
            if (CATLError::GetLastError() == EATLErrorOk  && !dev_name.empty()) {
                if (CATLError::GetLastError() == EATLErrorOk) {
                    int mem = shm_open(dev_name.c_str(), O_RDWR, S_IRUSR | S_IWUSR);
                    if (mem >= 0) {
                        memory_ptr = reinterpret_cast<UInt8*>(mmap(NULL, memory_size, PROT_READ | PROT_WRITE, MAP_SHARED, mem, 0));
                        if (memory_ptr != MAP_FAILED) {
                            SysLog()->LogInfo(LOG_CONTEXT, "successfully opened shared memory '%s' with size " FSIZE_T, dev_name.c_str(), memory_size);
                        } else {
                            memory_ptr = 0;
                            result = EATLErrorFatal;
                            SysLog()->LogError(LOG_CONTEXT, "can't map shared memory with size " FSIZE_T " for device %s", memory_size, dev_name.c_str());
                        }
                        close(mem); // file descriptor can be closed
                    } else {
                        result = EATLErrorFatal;
                        SysLog()->LogError(LOG_CONTEXT, "can't open shared memory for device %s", dev_name.c_str());
                    }
                } else {
                    result = EATLErrorFatal;
                    SysLog()->LogError(LOG_CONTEXT, "parameter mem-size has invalid value");
                }
            } else {
                result = EATLErrorFatal;
                SysLog()->LogError(LOG_CONTEXT, "invalid device name: %s", dev_name.c_str());
            }
        } else {
            result = EATLErrorFatal;
            SysLog()->LogError(LOG_CONTEXT, "connection to%s is already opened", dev_name.c_str());
        }
        return result;
    }

    CATLError CSharedMemoryDriver::Close()
    {
        CATLError result;
        if (memory_ptr){
            munmap(memory_ptr, memory_size);
            memory_ptr = 0;
        }
        else {
            result = EATLErrorFatal;
            SysLog()->LogError(LOG_CONTEXT, "device %s is already closed", dev_name.c_str());
        }
        return result;
    }

    CATLError CSharedMemoryDriver::Terminate() {
        CATLError result;
        if (memory_ptr) Close();
        return result;
    }

    CATLError CSharedMemoryDriver::ProcessPacket(TSmartPtr< CTransferPacket > packet, const flags32&)
    {
        CATLError res;
        if (memory_ptr) {
            switch (packet->command){
            case EPacketRead:
                if (packet->address + packet->data.size() <= memory_size){
                    SysLog()->LogDebug(LOG_CONTEXT, "successfully processed read packet id " FSIZE_T ", size " FSIZE_T, packet->id, packet->data.size());
                    memcpy(packet->data.data(),&memory_ptr[packet->address],packet->data.size());
                    packet->state = EPacketSuccess;
                    NotifyListerners( packet );
                }
                else {
                    SysLog()->LogError(LOG_CONTEXT, "address+size 0x%X is out of range 0x%X", packet->address + packet->data.size(), memory_size);
                    packet->state = EPacketInvalid;
                    NotifyListerners( packet );
                    res = EATLErrorInvalidParameters;
                }
                break;
            case EPacketWrite:
                if (packet->address + packet->data.size() <= memory_size){
                    SysLog()->LogDebug(LOG_CONTEXT, "successfully processed write packet id " FSIZE_T ", size " FSIZE_T, packet->id, packet->data.size());
                    memcpy(&memory_ptr[packet->address],packet->data.data(),packet->data.size());
                    packet->state = EPacketSuccess;
                    NotifyListerners( packet );
                }
                else {
                    SysLog()->LogError(LOG_CONTEXT, "address+size 0x%X is out of range 0x%X", packet->address + packet->data.size(), memory_size);
                    packet->state = EPacketInvalid;
                    NotifyListerners( packet );
                    res = EATLErrorInvalidParameters;
                }
                break;
            default:
                SysLog()->LogError(LOG_CONTEXT, "invalid command %d", packet->command);
                packet->state = EPacketInvalid;
                NotifyListerners( packet );
                res = EATLErrorInvalidParameters;
            }
        }
        else {
            SysLog()->LogError(LOG_CONTEXT, "attemptted to access driver which is not configured");
            packet->state = EPacketFail;
            NotifyListerners( packet );
            res = EATLErrorNotInitialized;
        }
        return res;
    }
}

#endif // !_ANDROID
#endif // _WIN32

