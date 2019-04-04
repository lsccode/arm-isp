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

#include <string.h>
#include <iostream>

#include <ATL/ATLConfig.h>

#define LOG_CONTEXT "BusManager"

#include <BusManager/BusManager.h>
#include <BusManager/DriverFactory.h>

using namespace std;

static std::map<std::string, std::string> defaults;

static void CreateMap()
{
    defaults["channel"] = "i2c";
    defaults["bus-mode"] = "sync";
}

namespace act {

    // Server interface
    CATLError CBusManager::Initialize(const flags32& options) {
        CreateMap();
        CATLError result;
        string prefix = getBusPrefix(options);
        if (CATLError::GetLastError().IsError()) {
            return CATLError::GetLastError();
        }
        SysConfig()->Merge(prefix, defaults);
        SysLog()->LogDebug(LOG_CONTEXT, "initializing bus manager for %schannel", prefix.c_str());
        std::string channel = SysConfig()->GetValue(prefix + "channel");
        std::string bus_mode = SysConfig()->GetValue(prefix + "bus-mode");
        if (channel.length()) {
            if ( bus_mode == "sync" ) {
                busMode = EACTBusSyncMode;
            } else if ( bus_mode == "async" ) {
                busMode = EACTBusASyncMode;
            } else {
                result = EATLErrorInvalidParameters;
                SysLog()->LogError(LOG_CONTEXT, "bad value for %sbus-mode parameter: %s", prefix.c_str(), bus_mode.c_str());
            }
            if (result.IsOk()) {
                driver = CDriverFactory::CreateDriver(channel);
                if (driver != NULL) {
                    result = driver->Initialize(options);
                    if (result.IsOk()) {
                        SysLog()->LogInfo(LOG_CONTEXT, "driver %s has been successfully initialized in %s mode for %schannel", driver->GetName().c_str(), bus_mode.c_str(), prefix.c_str());
                    } else {
                        result = driver->Terminate();
                        SysLog()->LogError(LOG_CONTEXT, "failed to initialize %s driver for %schannel", channel.c_str(), prefix.c_str());
                    }
                } else {
                    SysLog()->LogError(LOG_CONTEXT, "failed to create %s driver for %schannel", channel.c_str(), prefix.c_str());
                    result = EATLErrorFatal;
                }
            }
        } else {
            result = EATLErrorFatal;
            SysLog()->LogError(LOG_CONTEXT, "invalid driver name %s for %schannel", channel.c_str(), prefix.c_str());
        }
        return result;
    }

    const std::vector<string> CBusManager::GetSupportedDrivers() {
        return CDriverFactory::GetSupportedDrivers();
    }

    const std::vector<string > CBusManager::GetSupportedDevices() {
        std::vector<string> devices;
        if (driver != NULL) {
            devices = driver->GetDeviceList();
        }
        return devices;
    }

    CATLError CBusManager::Open() {
        CATLError result;
        if ( driver != NULL ) {
            result = driver->Open();
            if (result == EATLErrorOk) {
                // add a listener for async mode
                TATLObserver< CBusManager > *OnNewReplyObserver = new TATLObserver< CBusManager >(this, &CBusManager::ProcessAsyncReply);
                driver->AddListerner(EACTForwardReply, OnNewReplyObserver);
            } else {
                result = EATLErrorFatal;
                SysLog()->LogError(LOG_CONTEXT, "failed to open a device");
            }
        } else {
            result = EATLErrorFatal;
            SysLog()->LogError(LOG_CONTEXT, "trying to open a bus manager but driver is null");
        }
        return result;
    }


    void CBusManager::DriverCallback( void *sender ) {
        armcpp11::LockGuard<armcpp11::RecursiveMutex> lock( sync );
        if ( sender != NULL ) {
            TSmartPtr< CTransferPacket > packet =  reinterpret_cast< CTransferPacket * >( sender );
            if ( packet != NULL ) {
                if ( packet->state == EPacketSuccess )
                    std::cout << std::endl << "status: success";
                else std::cout << std::endl << "status: fail";
                if ( packet->command == EPacketRead && packet->data.size() > 0 ) {
                    std::cout << std::endl << "data: [ ";
                    for ( basesize idx = 0; idx < packet->data.size(); idx ++ ) {
                        std::cout << "0x" << std::hex << packet->data[ idx ];
                    }
                    std::cout << " ]";
                }
                syncRequest.NotifyAll();
            }
        } else {
            SysLog()->LogWarning(LOG_CONTEXT, "bad packet sender");
        }
    }

    CATLError CBusManager::Close() {
        CATLError result;
        if ( driver != NULL ) {
            result = driver->Close();
            if ( result == EATLErrorOk ) {
                SysLog()->LogInfo(LOG_CONTEXT, "device %s has been successfully closed", driver->GetName().c_str());
            } else {
                result = EATLWarning;
                SysLog()->LogWarning(LOG_CONTEXT, "failed to close device %s", driver->GetName().c_str());
            }
        } else {
            result = EATLWarning;
            SysLog()->LogWarning(LOG_CONTEXT, "null driver pointer. Failed to close a device");
        }
        return result;
    }


    CATLError CBusManager::Terminate() {
        CATLError result;
        if ( driver != NULL ) {
            result = driver->Terminate();
            if ( result == EATLErrorOk ) {
                SysLog()->LogInfo(LOG_CONTEXT, "device %s has been successfully terminated", driver->GetName().c_str());
            } else {
                result = EATLWarning;
                SysLog()->LogWarning(LOG_CONTEXT, "failed to terminate a device");
            }
        } else {
            result = EATLWarning;
            SysLog()->LogWarning(LOG_CONTEXT, "failed to terminate a device");
        }
        return result;
    }

    CATLError CBusManager::ProcessRequest(TSmartPtr< CTransferPacket> packet) {
        CATLError result = EATLErrorOk;
        SysLog()->LogDebug(LOG_CONTEXT, "start packet processing: packet id: " FSIZE_T " cmd: %d address: 0x%x data size: " FSIZE_T " mask size: " FSIZE_T, packet->id, packet->command, packet->address, packet->data.size(), packet->mask.size() );
        if ( packet != NULL ) {
            if (  packet->command == EPacketWrite || packet->command == EPacketRead || packet->command == EPacketTransaction ) {
                flags32 options = 0;
                if ( busMode == EACTBusSyncMode ) {
                    options |= EACTDriverProcessPacketSync;
                }
                result = driver->ProcessPacket( packet, options );
                if ( result.IsOk() ); else {
                    result = EATLErrorFatal;
                    SysLog()->LogError(LOG_CONTEXT, "failed to %s isp memory at address 0x%x", (packet->command == EPacketWrite ? "write" : "read"), packet->address);
                }
            } else {
                result = EATLErrorFatal;
                SysLog()->LogError(LOG_CONTEXT, "request has invalid format: supported commands are <set> and <get> only");
            }
        } else {
            result = EATLErrorInvalidParameters;
            SysLog()->LogError(LOG_CONTEXT, "null packet pointer");
        }
        SysLog()->LogDebug(LOG_CONTEXT, "end packet processing: packet id: " FSIZE_T " result = %d", packet->id, result.GetErrorCode());
        return result;
     }


    void CBusManager::ProcessAsyncReply( void *sender ) {
        if ( sender != NULL ) {
            TSmartPtr< CTransferPacket > packet =  reinterpret_cast< CTransferPacket * >( sender );
            // invoke a listener for this packet
            packet->Invoke();
            // broadcast this packet for observers
            OnReplyReceived.Invoke( sender );
        }
    }

    TSmartPtr< CTransferPacket > CBusManager::GetLastPacket() {
        armcpp11::LockGuard<armcpp11::RecursiveMutex> lock( sync );
        TSmartPtr< CTransferPacket> packet = 0;
        if ( packetDeque.size() > 0 ) {
            packet = packetDeque.front();
            packetDeque.pop_front();
        }
        return packet;
    }

}
