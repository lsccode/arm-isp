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

#include <string.h> // for memcpy
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include <ATL/ATLConfig.h>

#define LOG_CONTEXT "DevMemDriver"
#include "DevMemDriver.h"

using namespace std;
using namespace atl;

static const string DEV_MEM = "/dev/mem";
static map<string, string> defaults;

static const basesize alignment_mask = (sizeof(void*)-1);

static void CreateMap()
{
    defaults["dev"] =           DEV_MEM;    // device name
    defaults["mem-offset"] =    "0";        // virtual memory offset
    defaults["mem-size"] =      "131072";   // virtual memory size 1<<17
}

namespace act {

    CDevMemDriver::CDevMemDriver() {
        CreateMap();
        devmemfd = -1;
        base_addr = NULL;
    }

    CDevMemDriver::~CDevMemDriver() {
        close();
    }

    const vector<string> CDevMemDriver::GetDeviceList() {
        vector<string> devices;    // create possible socket names
        devices.push_back(DEV_MEM);
        return devices;
    }

    const vector<EPacketCommand> CDevMemDriver::GetCommandList() {
        vector<EPacketCommand> commands;
        commands.push_back(EPacketRead);
        commands.push_back(EPacketWrite);
        return commands;
    }

    const vector<EACTDriverMode> CDevMemDriver::GetSupportedModes() {
        vector<EACTDriverMode> modes;
        modes.push_back (EACTDriverSyncMode);
        return modes;
    }

    const EACTDriverMode CDevMemDriver::GetCurrentMode() {
        return EACTDriverSyncMode;
    }

    CATLError CDevMemDriver::Initialize(const flags32& options) {
        string prefix = getBusPrefix(options);
        SysConfig()->Merge(prefix, defaults);
        if (CATLError::GetLastError().IsError()) {
            return CATLError::GetLastError();
        }
        dev_name = SysConfig()->GetValue(prefix + "dev");
        mem_offset = SysConfig()->GetValueAsU32(prefix + "mem-offset");
        max_addr = SysConfig()->GetValueAsU32(prefix + "mem-size");
        return EATLErrorOk;
    }

    CATLError CDevMemDriver::Open() {
        CATLError result;
        if (devmemfd < 0 && !base_addr) {
            if (!dev_name.empty()) {
                if (mem_offset) {
                    if (CATLError::GetLastError() == EATLErrorOk && max_addr) {
                        result = mapmemoryblock(mem_offset);
                    } else {
                        result = EATLErrorInvalidParameters;
                        SysLog()->LogError(LOG_CONTEXT, "parameter mem-size is wrong");
                    }
                } else {
                    result = EATLErrorInvalidParameters;
                    SysLog()->LogError(LOG_CONTEXT, "parameter mem-offset is not specified");
                }
            } else {
                result = EATLErrorInvalidParameters;
                SysLog()->LogError(LOG_CONTEXT, "parameter --(fw/hw)-dev is wrong: %s", dev_name.c_str());
            }
        } else {
            result = EATLErrorFatal;
            SysLog()->LogError(LOG_CONTEXT, "connection to %s is already opened", dev_name.c_str());
        }
        return result;
    }

    CATLError CDevMemDriver::Close() {
        CATLError result;
        if (devmemfd >= 0 || base_addr){
            close();
            SysLog()->LogInfo(LOG_CONTEXT, "device successfully closed");
        }
        else {
            result = EATLErrorFatal;
            SysLog()->LogError(LOG_CONTEXT, "device %s is already closed", dev_name.c_str());
        }
        return result;
    }

    void CDevMemDriver::close() {
        if (base_addr) munmap(base_addr, max_addr);
        if (devmemfd >= 0) ::close(devmemfd);
        base_addr = NULL;
        devmemfd = -1;
    }

    CATLError CDevMemDriver::Terminate() {
        CATLError result;
        close();
        return result;
    }

    CATLError CDevMemDriver::ProcessPacket(TSmartPtr< CTransferPacket > packet, const flags32&) {
        if (devmemfd >= 0 && base_addr) {
            switch (packet->command){
            case EPacketWrite:
                return write(packet);
            case EPacketRead:
                return read(packet);
            default:
                SysLog()->LogError(LOG_CONTEXT, "invalid command %d for dev mem driver", packet->command);
                packet->state = EPacketInvalid;
                NotifyListerners( packet );
                return EATLErrorInvalidParameters;
            }
        }
        else {
            SysLog()->LogError(LOG_CONTEXT, "attemptted to access driver which is not configured");
            packet->state = EPacketFail;
            packet->data.clear();
            NotifyListerners( packet );
            return EATLErrorNotInitialized;
        }
    }

    CATLError CDevMemDriver::read(TSmartPtr<CTransferPacket>& packet) {
        CATLError result;
        if (packet->address + packet->data.size() <= max_addr) {
            basesize i = 0;
            for (basesize addr = (basesize)base_addr+packet->address; addr < (basesize)base_addr+packet->address+packet->data.size();) {
                basesize* addr_align = (basesize*)(addr & ~alignment_mask);
                basesize addr_byte = addr & alignment_mask;
                basesize data = *addr_align;
                for (basesize b = addr_byte; b < alignment_mask+1 && i < packet->data.size(); b++){
                    packet->data[i++] = (UInt8)((data>>(b*8))&0xFF);
                }
                addr += alignment_mask+1-addr_byte;
            }
            packet->state = EPacketSuccess;
            SysLog()->LogDebug(LOG_CONTEXT, "successful read from address 0x%X size " FSIZE_T " id " FSIZE_T, packet->address, packet->data.size(), packet->id);
        } else {
            SysLog()->LogWarning(LOG_CONTEXT, "address 0x%X and size " FSIZE_T " is out of range 0x%X", packet->address, packet->data.size(), max_addr);
            packet->state = EPacketSuccess;
            memset(packet->data.data(),0,packet->data.size()*sizeof(UInt8));
        }
        NotifyListerners( packet );
        return result;
    }

    CATLError CDevMemDriver::write(TSmartPtr<CTransferPacket>& packet) {
        CATLError result;
        if (packet->address + packet->data.size() <= max_addr) {
            if (packet->mask.empty()){
                basesize i = 0;
                for (basesize addr = (basesize)base_addr+packet->address; addr < (basesize)base_addr+packet->address+packet->data.size();) {
                    basesize* addr_align = (basesize*)(addr & ~alignment_mask);
                    basesize addr_byte = addr & alignment_mask;
                    basesize data = *addr_align;
                    for (basesize b = addr_byte; b < alignment_mask+1 && i < packet->data.size(); b++){
                        basesize mask = (basesize)0xFF << (b*8);
                        data = (data & ~mask) | ((basesize)packet->data[i++] << (b*8));
                    }
                    *addr_align = data;
                    addr += alignment_mask+1-addr_byte;
                }
                SysLog()->LogDebug(LOG_CONTEXT, "successful write to address 0x%X size " FSIZE_T " id " FSIZE_T, packet->address, (UInt32)packet->data.size(), packet->id);
                packet->state = EPacketSuccess;
            } else if (packet->data.size() == packet->mask.size()) {
                basesize i = 0;
                for (basesize addr = (basesize)base_addr+packet->address; addr < (basesize)base_addr+packet->address+packet->data.size();) {
                    basesize* addr_align = (basesize*)(addr & ~alignment_mask);
                    basesize addr_byte = addr & alignment_mask;
                    basesize data = *addr_align;
                    for (basesize b = addr_byte; b < alignment_mask+1 && i < packet->data.size(); b++){
                        basesize mask = (basesize)packet->mask[i] << (b*8);
                        data = (data & ~mask) | ((basesize)(packet->data[i]&packet->mask[i]) << (b*8));
                        i++;
                    }
                    *addr_align = data;
                    addr += alignment_mask+1-addr_byte;
                }
                SysLog()->LogDebug(LOG_CONTEXT, "successful masked write to address 0x%X size " FSIZE_T " id " FSIZE_T, packet->address, packet->data.size(), packet->id);
                packet->state = EPacketSuccess;
            } else {
                SysLog()->LogError(LOG_CONTEXT, "data size " FSIZE_T " is not equal to mask size " FSIZE_T, packet->data.size(), packet->mask.size());
                packet->state = EPacketInvalid;
                packet->data.clear();
                result = EATLErrorInvalidParameters;
            }
        } else {
            SysLog()->LogWarning(LOG_CONTEXT, "address 0x%X and size " FSIZE_T " are out of range 0x%X", packet->address, packet->data.size(), max_addr);
            packet->state = EPacketSuccess;
        }
        NotifyListerners( packet );
        return result;
    }

    CATLError CDevMemDriver::mapmemoryblock(const off64_t &offset) {
        CATLError err;
        devmemfd = open(dev_name.c_str(), O_RDWR | O_SYNC);
        if (devmemfd >= 0) {
            base_addr = reinterpret_cast<UInt8*>(mmap64(NULL, max_addr, PROT_READ | PROT_WRITE, MAP_SHARED, devmemfd, offset));
            if (base_addr == MAP_FAILED) {
                ::close(devmemfd);
                devmemfd = -1;
                SysLog()->LogError(LOG_CONTEXT, "memory map failed for device %s: %s", dev_name.c_str(), strerror(errno));
                err = EATLErrorFatal;
            } else {
                SysLog()->LogInfo(LOG_CONTEXT, "successfully mapped %d bytes from device %s offset 0x%X", max_addr, dev_name.c_str(), offset);
            }
        } else {
            SysLog()->LogError(LOG_CONTEXT, "can't open memory device %s: error %d", dev_name.c_str(), devmemfd);
            err = EATLErrorFatal;
        }
        return err;
    }

}

#endif // __WIN32

