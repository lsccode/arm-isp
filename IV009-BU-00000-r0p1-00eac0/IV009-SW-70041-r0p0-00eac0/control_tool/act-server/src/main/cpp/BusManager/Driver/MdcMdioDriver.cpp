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

#define LOG_CONTEXT "MdcMdioDriver"
#include "MdcMdioDriver.h"

using namespace std;
using namespace atl;

static std::map<std::string, std::string> defaults;

static void CreateMap()
{
    defaults["dev"] =          "default";    // device name
    defaults["mem-offset"] =   "0";          // virtual memory offset

}

namespace act {

    const string CMdcMdioDriver::GetObjectStaticName() {
        return LOG_CONTEXT;
    }

    const string CMdcMdioDriver::GetName() {
        return "mdcmdio";
    }

    const vector< string > CMdcMdioDriver::GetDeviceList() {
        return ftdi.get_devices();
    }

    const vector< EPacketCommand > CMdcMdioDriver::GetCommandList() {
        vector< EPacketCommand > commands;
        commands.push_back( EPacketRead );
        commands.push_back( EPacketWrite );
        return commands;
    }

    const vector<EACTDriverMode> CMdcMdioDriver::GetSupportedModes() {
        vector<EACTDriverMode> modes;
        modes.push_back(EACTDriverSyncMode);
        return modes;
    }

    const EACTDriverMode CMdcMdioDriver::GetCurrentMode() {
        return EACTDriverSyncMode;
    }

    CATLError CMdcMdioDriver::Initialize(const flags32& options)
    {
        CreateMap();
        ftdi_opened = false;
        string prefix = getBusPrefix(options);
        SysConfig()->Merge(prefix, defaults);
        if (CATLError::GetLastError().IsError()) {
            return CATLError::GetLastError();
        }
        dev_name = SysConfig()->GetValue(prefix + "dev");
        mem_offset = SysConfig()->GetValueAsU32(prefix + "mem-offset");
        return EATLErrorOk;
    }

    CATLError CMdcMdioDriver::Open() {
        CATLError result;

        ftdi.open(dev_name);
        ftdi_opened = true;

        const string& chip = ftdi.get_chip_name();
        if (chip == "TTL232R-3V3") {
            baudrate = 39000;
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
        } else {
            result = EATLErrorFatal;
            SysLog()->LogError(LOG_CONTEXT, "FTDI chip %s is not supported", chip.c_str());
            Close();
        }
        return result;
    }

    CATLError CMdcMdioDriver::Close()
    {
        CATLError result;
        if (ftdi_opened) {
            try {
                ftdi.reset();
                ftdi.close();
            }
            catch (...){
                result = EATLErrorFatal;
            }
        }
        ftdi_opened = false;
        return result;
    }

    CATLError CMdcMdioDriver::Terminate() {
        CATLError result;
        return result;
    }

    CATLError CMdcMdioDriver::ProcessPacket(TSmartPtr< CTransferPacket > packet, const flags32&) {
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

    CATLError CMdcMdioDriver::write_data(TSmartPtr<CTransferPacket>& packet) {
        CATLError result;
        SysLog()->LogDebug(LOG_CONTEXT, "write packet: addr: 0x%X size: " FSIZE_T, packet->address, packet->data.size());
        packet->state = EPacketProcessing;
        if (!packet->mask.empty() && packet->mask.size() != packet->data.size()){
            SysLog()->LogError(LOG_CONTEXT, "write packet data and mask sizes mismatch");
            packet->state = EPacketInvalid;
            result =  EATLErrorInvalidParameters;
        }
        else {
            if (packet->mask.empty()) {
                try {
                    basesize i = 0;
                    for (baseaddr addr = mem_offset+packet->address; addr < mem_offset+packet->address+packet->data.size();) {
                        baseaddr addr_align = addr & ~alignment_mask;
                        baseaddr addr_byte = addr & alignment_mask;
                        UInt32 data = read_mem(addr_align);
                        for (baseaddr b = addr_byte; b < alignment_mask+1 && i < packet->data.size(); b++){
                            UInt32 mask = (UInt32)0xFF << (b*8);
                            data = (data & ~mask) | ((UInt32)packet->data[i++] << (b*8));
                        }
                        write_mem(addr_align,data);
                        addr += alignment_mask+1-addr_byte;
                    }
                    SysLog()->LogDebug(LOG_CONTEXT, "successful write to address 0x%X size " FSIZE_T " id " FSIZE_T, packet->address, (UInt32)packet->data.size(), packet->id);
                    packet->state = EPacketSuccess;
                }
                catch (...) {
                    SysLog()->LogError(LOG_CONTEXT, "can't write " FSIZE_T " bytes at address 0x%X id " FSIZE_T, (UInt32)packet->data.size(), packet->address, packet->id);
                    packet->state = EPacketFail;
                    result = EATLErrorDeviceError;
                }
            }
            else {
                try {
                    basesize i = 0;
                    for (baseaddr addr = mem_offset+packet->address; addr < mem_offset+packet->address+packet->data.size();) {
                        baseaddr addr_align = addr & ~alignment_mask;
                        baseaddr addr_byte = addr & alignment_mask;
                        UInt32 data = read_mem(addr_align);
                        for (baseaddr b = addr_byte; b < alignment_mask+1 && i < packet->data.size(); b++){
                            UInt32 mask = (UInt32)packet->mask[i] << (b*8);
                            data = (data & ~mask) | ((UInt32)(packet->data[i]&packet->mask[i]) << (b*8));
                            i++;
                        }
                        write_mem(addr_align,data);
                        addr += alignment_mask+1-addr_byte;
                    }
                    SysLog()->LogDebug(LOG_CONTEXT, "successful masked write to address 0x%X size " FSIZE_T " id " FSIZE_T, packet->address, packet->data.size(), packet->id);
                    packet->state = EPacketSuccess;
                }
                catch (...) {
                    SysLog()->LogError(LOG_CONTEXT, "can't execute masked write to address 0x%X size " FSIZE_T " id " FSIZE_T, packet->address, (UInt32)packet->data.size(), packet->id);
                    packet->state = EPacketFail;
                    result = EATLErrorDeviceError;
                }
            }
        }
        NotifyListerners(packet);
        return result;
    }

    CATLError CMdcMdioDriver::read_data(TSmartPtr<CTransferPacket>& packet)
    {
        CATLError result;
        SysLog()->LogDebug(LOG_CONTEXT, "read packet: addr: 0x%X size: " FSIZE_T, packet->address, packet->data.size());
        packet->state = EPacketProcessing;
        try {
            basesize i = 0;
            for (baseaddr addr = mem_offset+packet->address; addr < mem_offset+packet->address+packet->data.size();) {
                baseaddr addr_align = addr & ~alignment_mask;
                baseaddr addr_byte = addr & alignment_mask;
                UInt32 data = read_mem(addr_align);
                for (baseaddr b = addr_byte; b < alignment_mask+1 && i < packet->data.size(); b++){
                    packet->data[i++] = (UInt8)((data>>(b*8))&0xFF);
                }
                addr += alignment_mask+1-addr_byte;
            }
            packet->state = EPacketSuccess;
            SysLog()->LogDebug(LOG_CONTEXT, "successful read from address 0x%X size " FSIZE_T " id " FSIZE_T, packet->address, packet->data.size(), packet->id);
        }
        catch (...) {
            SysLog()->LogError(LOG_CONTEXT, "can't read from address 0x%X size " FSIZE_T " id " FSIZE_T, packet->address, packet->data.size(), packet->id);
            packet->state = EPacketFail;
            result = EATLErrorDeviceError;
        }
        NotifyListerners(packet);
        return result;
    }


    void CMdcMdioDriver::configure()
    {
        ftdi.reset();
        ftdi.set_bitmode(0xFF,FTDI_BITMODE_RESET);
        ftdi.set_bitmode(bitbang_sda_out|bitbang_scl,FTDI_BITMODE_BITBANG);
        ftdi.set_baudrate(baudrate*2);

        data.clear();
        for (int i = 0; i < 10; i++){
            data.push_back(0);
            data.push_back(bitbang_scl);
        }
        data.push_back(0);
        ftdi.write(data);
        ftdi.read(data);
        SysLog()->LogInfo(LOG_CONTEXT, "FTDI is configured in bitbang mode");
    }

    void CMdcMdioDriver::put_bit(UInt8 bit)
    {
        UInt8 sda = bit ? bitbang_sda_out : 0;
        data.push_back(sda);
        data.push_back(sda|bitbang_scl);
    }

    void CMdcMdioDriver::put_preamble()
    {
        for (int i = 0; i < 32; ++i){
            put_bit(1);
        }
    }

    void CMdcMdioDriver::put_start()
    {
        put_bit(0);
        put_bit(1);
    }

    void CMdcMdioDriver::write_word(baseaddr addr, UInt32 val)
    {
        int i;
        data.clear();
        put_preamble();
        put_start();
        // OP=01
        put_bit(0);
        put_bit(1);
        // PHYADD=00010
        put_bit(0);
        put_bit(0);
        put_bit(0);
        put_bit(1);
        put_bit(0);
        // REGADD=addr
        for (i = 5; i--;){
            put_bit((addr >> i) & 0x01);
        }
        // TA=10
        put_bit(1);
        put_bit(0);
        // DATA 16 bits
        for (i = 16; i--;){
            put_bit((val >> i) & 0x01);
        }

        data.push_back(0);
        ftdi.write(data);
        ftdi.read(data); // dummy read
    }

    UInt32 CMdcMdioDriver::read_word(baseaddr addr)
    {
        int i;
        data.clear();
        put_preamble();
        put_start();
        // OP=10
        put_bit(1);
        put_bit(0);
        // PHYADD=00010
        put_bit(0);
        put_bit(0);
        put_bit(0);
        put_bit(1);
        put_bit(0);
        // REGADD=addr
        for (i = 5; i--;){
            put_bit((addr >> i) & 0x01);
        }

        basesize offset = data.size()+3;

        // Reading starts
        for (i = 18; i--;){
            put_bit(0);
        }

        data.push_back(0);

        ftdi.write(data);
        ftdi.read(data);

        UInt32 word = 0;

        for (; offset < data.size(); offset++){
            if ((data[offset] & bitbang_scl) == 0){
                word = (word << 1) | (data[offset] & bitbang_sda_in ? 1 : 0);
            }
        }

        return word;
    }

    UInt32 CMdcMdioDriver::read_mem(baseaddr addr)
    {
        armcpp11::LockGuard<armcpp11::Mutex> lock(mtx);
        write_word(0x08,0x0000);
        write_word(0x0A,0x0000);
        write_word(0x00,(addr >> 16) & 0xFFFF);
        write_word(0x0C,(addr >>  0) & 0xFFFF);
        write_word(0x0E,0x8000);

        if ((read_word(0x0E) & 0x8000) != 0){
            SysLog()->LogError(LOG_CONTEXT, "MDC/MDIO device is not ready");
            return 0;
        }

        return (read_word(0x0a) << 16) | read_word(0x08);
    }

    void CMdcMdioDriver::write_mem(baseaddr addr, UInt32 val)
    {
        armcpp11::LockGuard<armcpp11::Mutex> lock(mtx);
        write_word(0x0A,(val >> 16) & 0xFFFF);
        write_word(0x08,(val >>  0) & 0xFFFF);
        write_word(0x00,(addr >> 16) & 0xFFFF);
        write_word(0x0C,(addr >>  0) & 0xFFFF);
        write_word(0x0E,0x800F);

        if ((read_word(0x0E) & 0x8000) != 0) {
            SysLog()->LogError(LOG_CONTEXT, "MDC/MDIO device is not ready");
        }
    }

}
#endif // !_ANDROID
