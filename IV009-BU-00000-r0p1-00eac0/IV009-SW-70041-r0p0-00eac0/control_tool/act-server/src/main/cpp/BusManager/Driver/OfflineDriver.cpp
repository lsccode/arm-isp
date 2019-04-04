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

#define LOG_CONTEXT "OfflineDriver"

#include "OfflineDriver.h"

enum {
    COMMAND_RESET       = 0,
    COMMAND_SET_TYPE    = 1,
    COMMAND_SET_ID      = 2,
    COMMAND_SET_DIR     = 3,
    COMMAND_SET_VALUE   = 4,
    COMMAND_RUN         = 5,
    COMMAND_DONE        = 6,
    COMMAND_BUF_SIZE,
    COMMAND_BUF_ERR,
    COMMAND_BUF_SET,
    COMMAND_BUF_GET,
    COMMAND_BUF_APPLY,
    COMMAND_API_GET     = 12,
    COMMAND_API_SET     = 13
};

static const std::string DEFAULT_DEV = "/dev/act";

static std::map<std::string, std::string> defaults;

static void CreateMap()
{
    defaults["dev"] = DEFAULT_DEV;
}


namespace act {

const std::string COfflineDriver::GetObjectStaticName()
{
    return LOG_CONTEXT;
}

const std::string COfflineDriver::GetName()
{
    return "offline";
}

const std::vector<std::string> COfflineDriver::GetDeviceList()
{
    std::vector<std::string> devices;    // create possible socket names
    devices.push_back(DEFAULT_DEV);
    return devices;
}

const std::vector<EPacketCommand> COfflineDriver::GetCommandList()
{
    std::vector<EPacketCommand> commands;
    commands.push_back(EPacketRead);
    commands.push_back(EPacketWrite);
    return commands;
}

const std::vector<EACTDriverMode> COfflineDriver::GetSupportedModes()
{
    std::vector<EACTDriverMode> modes;
    modes.push_back(EACTDriverSyncMode);
    return modes;
}

const EACTDriverMode COfflineDriver::GetCurrentMode()
{
    return EACTDriverSyncMode;
}

UInt32 COfflineDriver::fw_dbg_read_data() const {
    UInt8 v[4];
    v[0] = debug_registers.at(1);
    v[1] = debug_registers.at(2);
    v[2] = debug_registers.at(3);
    v[3] = debug_registers.at(4);
    return (((UInt32)v[0]<<24)|((UInt32)v[1]<<16)|((UInt32)v[2]<<8)|v[3]);
}

void COfflineDriver::fw_dbg_write_data(const UInt32& value) {
    debug_registers[1] = ((value>>24) & 0xFF);
    debug_registers[2] = ((value>>16) & 0xFF);
    debug_registers[3] = ((value>>8)  & 0xFF);
    debug_registers[4] = ((value>>0)  & 0xFF);
}

CATLError COfflineDriver::ProcessPacket(TSmartPtr<CTransferPacket> packet, const flags32&) {
    CATLError res;

    basesize sz = packet->data.size();
    if (sz < 1) {
        SysLog()->LogError(LOG_CONTEXT, "empty packet data");
        packet->state = EPacketInvalid;
        NotifyListerners(packet);
        return EATLErrorInvalidParameters;
    }

    switch (packet->command) {
    case EPacketRead: {
        basesize sz = packet->data.size();
        if (packet->address == dbg_addr) { // special case for fw debug access
            if (debug_registers_reset) {
                packet->data[0] = COMMAND_DONE;
                if (sz > 1) {
                    packet->data[1] = 0xff; // fast api supported
                }
            } else {
                packet->data[0] = COMMAND_DONE;
                packet->data[1] = debug_registers[1];
                packet->data[2] = debug_registers[2];
                packet->data[3] = debug_registers[3];
                packet->data[4] = debug_registers[4];
                packet->data[5] = 0; // status
            }
        } else {
            std::map < baseaddr, std::vector<UInt8> >::const_iterator item = isp.find(packet->address);
            if (item == isp.end()) { // not found in isp
                isp[packet->address] = std::vector<UInt8>();
                isp[packet->address].resize(sz, 0x0);
            }
            for (basesize i = 0; i < sz; i++) {
                packet->data[i] = isp[packet->address][i];
            }
        }
        packet->state = EPacketSuccess;
        NotifyListerners(packet);
        SysLog()->LogDebug(LOG_CONTEXT, "successfully processed read packet id " FSIZE_T ", size " FSIZE_T, packet->id, packet->data.size());
        break;
    }
    case EPacketWrite: {
        if (packet->address == dbg_addr) {
            UInt8 cmd = packet->data[0];
            switch (cmd) {
            case COMMAND_RESET: {
                debug_registers[0] = 0;
                debug_registers[1] = 0;
                debug_registers[2] = 0;
                debug_registers[3] = 0;
                debug_registers[4] = 0;
                debug_registers[5] = 0;
                debug_registers[6] = 0;
                debug_registers[7] = 0;
                debug_registers_reset = true;

                break;
            }
            case COMMAND_API_GET: {
                debug_registers_reset = false;
                UInt8 section = debug_registers[5];   // get previously set section and command
                UInt8 command = debug_registers[6];
                UInt16 hash = (section << 8) + command;
                if (firmware.find(hash) == firmware.end()) { // not found in fw
                    firmware[hash] = 0;
                }
                fw_dbg_write_data(firmware[hash]);      // write value to debug registers
                packet->data[0] = COMMAND_DONE;
                break;
            }
            case COMMAND_API_SET: {
                debug_registers_reset = false;
                UInt8 section = debug_registers[5];   // get previously set section and command
                UInt8 command = debug_registers[6];
                UInt16 hash = (section << 8) + command;
                firmware[hash] = fw_dbg_read_data();    // remember the value for given section and command
                break;
            }
            default:
                break;
            }
        } if (packet->address == (dbg_addr + 1)) {
            debug_registers_reset = false;
            debug_registers[1] = packet->data[0];   // fw api value
            debug_registers[2] = packet->data[1];   //
            debug_registers[3] = packet->data[2];   //
            debug_registers[4] = packet->data[3];   //
            debug_registers[5] = packet->data[4];   // fw api section
            debug_registers[6] = packet->data[5];   // fw api command
        } else {
            if (isp.find(packet->address) == isp.end()) { // not found in isp
                isp[packet->address] = std::vector<UInt8>(sz);
            }
            isp[packet->address].resize(sz, 0x0);
            for (basesize i = 0; i < sz; i++) {
                isp[packet->address][i] = packet->data[i];
            }
        }
        packet->state = EPacketSuccess;
        NotifyListerners(packet);
        SysLog()->LogDebug(LOG_CONTEXT, "successfully processed write packet id " FSIZE_T ", size " FSIZE_T, packet->id, packet->data.size());
        break;
    }
    default:
        SysLog()->LogError(LOG_CONTEXT, "invalid command type %d", packet->command);
        packet->state = EPacketInvalid;
        NotifyListerners(packet);
        res = EATLErrorInvalidParameters;
    }

    return res;
}

CATLError COfflineDriver::Initialize(const flags32& options) {
    CreateMap();
    std::string prefix = getBusPrefix(options);
    SysConfig()->Merge(prefix, defaults);
    dbg_addr = SysConfig()->GetValueAsU32("dbg-offset");
    debug_registers[0] = 0;
    debug_registers[1] = 0;
    debug_registers[2] = 0;
    debug_registers[3] = 0;
    debug_registers[4] = 0;
    debug_registers[5] = 0;
    debug_registers[6] = 0;
    debug_registers[7] = 0;
    debug_registers_reset = true;
    return CATLError::GetLastError();
}

CATLError COfflineDriver::Open() {
    isp.clear();
    firmware.clear();
    return EATLErrorOk;
}

CATLError COfflineDriver::Close() {
    isp.clear();
    firmware.clear();
    return EATLErrorOk;
}

CATLError COfflineDriver::Terminate() {
    return EATLErrorOk;
}

}

