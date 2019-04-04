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

#include <ATL/ATLConfig.h>
#include <ATL/ATLLogger.h>

#include <map>

#define LOG_CONTEXT "AccessManager"

#include <AccessManager/AccessManager.h>

using namespace std;

static std::map<std::string,std::string> defaults;

static void CreateMap()
{
    defaults["hw-acc"] = "direct";
}

namespace act {

    CAccessManager::CAccessManager()
    {
        CreateMap();
        hw_access_type = NotConfigured;
        fw_access_type = NotConfigured;
        initialized = false;
        opened = false;
    }

    CAccessManager::~CAccessManager()
    {

    }

    const string CAccessManager::GetObjectStaticName()
    {
        return LOG_CONTEXT;
    }

    const string CAccessManager::GetName()
    {
        return LOG_CONTEXT;
    }

    CATLError CAccessManager::Initialize(const flags32&)
    {
        SysConfig()->Merge(defaults);
        if (initialized) {
            SysLog()->LogError(LOG_CONTEXT, "attempt to change already initialize access manager");
            return EATLErrorUnsupported;
        }

        CATLError result;
        string hw_access = SysConfig()->GetValue("hw-acc");

        if (hw_access == "direct" || hw_access == "stream") {

            if (hw_access == "direct") {
                reg_bus_manager = new CBusManager();                    // create bus manager for direct register access
                result = reg_bus_manager->Initialize(BUS_HW);
                if (result.IsOk()) {
                    SysLog()->LogDebug(LOG_CONTEXT, "using direct hardware access");
                    hw_access_type = Direct;
                    reg_dir_acc.Configure(reg_bus_manager);             // set bus manager to register access
                } else {
                    SysLog()->LogError(LOG_CONTEXT, "can't initialize bus manager for direct hardware access");
                    hw_access_type = NotConfigured;
                }
            } else if (hw_access == "stream") {
                hw_access_type = Stream;
                SysLog()->LogDebug(LOG_CONTEXT, "using stream hardware access");
            }

            if (result.IsOk()) {
                string fw_access = SysConfig()->GetValue("fw-acc");
                if (fw_access == "direct" || fw_access == "hw" || fw_access == "none" || fw_access == "stream" || fw_access == "sw") {
                    if (fw_access == "none") {
                        SysLog()->LogDebug(LOG_CONTEXT, "firmware will not be accessed as requested");
                        fw_access_type = None;
                    } else if (fw_access == "direct") {
                        baseaddr dbg_offset = 0x40000 + 0x1F0;
                        if (SysConfig()->HasOption("dbg-offset")) {
                            dbg_offset = SysConfig()->GetValueAsU32("dbg-offset");
                        } else {
                            SysLog()->LogWarning(LOG_CONTEXT, "using default debug offset " FSIZE_T " for direct firmware access", dbg_offset);
                        }
                        if (dbg_offset >= 0) {
                            fw_access_type = Direct;
                            fw_dbg_acc.Configure(reg_bus_manager, dbg_offset);    // configure debug fw access with bus manager and debug register offset
                            SysLog()->LogDebug(LOG_CONTEXT, "using direct firmware access with debug offset " FSIZE_T, dbg_offset);
                        } else {
                            fw_access_type = NotConfigured;
                            result = EATLErrorInvalidParameters;
                            SysLog()->LogError(LOG_CONTEXT, "invalid debug register offset: " FSIZE_T, dbg_offset);
                        }
                    } else if (fw_access == "hw") {
                        baseaddr buf_offset = SysConfig()->GetValueAsU32("hw-buf-offset");
                        UInt32 buf_size = SysConfig()->GetValueAsU32("hw-buf-size");
                        if (buf_offset >=0 && buf_size > 0x10) {
                            fw_access_type = Stream;
                            fw_str_acc.Configure(reg_bus_manager, Buffer, buf_offset, buf_size);    // configure stream fw access via hw buffer
                            SysLog()->LogDebug(LOG_CONTEXT, "using hw buffer firmware access with hw buffer offset " FSIZE_T " and size " FSIZE_T, buf_offset, buf_size);
                        } else {
                            fw_access_type = Stream;
                            result = EATLErrorInvalidParameters;
                            SysLog()->LogError(LOG_CONTEXT, "invalid hw buffer offset " FSIZE_T " or size " FSIZE_T, buf_offset, buf_size);
                        }
                    } else if (fw_access == "sw" || fw_access == "stream") {
                        fw_bus_manager = new CBusManager();                  // independent bus manager for stream access
                        result = fw_bus_manager->Initialize(BUS_FW);
                        if (result.IsOk()) {
                            fw_access_type = Stream;
                            fw_str_acc.Configure(fw_bus_manager, (fw_access == "sw") ? (EStreamType)Buffer : (EStreamType)Stream);
                            SysLog()->LogDebug(LOG_CONTEXT, "using stream firmware access through %s", (fw_access == "sw") ? "sw buffer": "stream");
                        } else {
                            fw_access_type = NotConfigured;
                            result = EATLErrorFatal;
                            SysLog()->LogError(LOG_CONTEXT, "can't initialize bus manager for firmware stream access");
                        }
                    }
                } else {
                    result = EATLErrorFatal;
                    SysLog()->LogError(LOG_CONTEXT, "bad firmware access: %s", fw_access.c_str());
                }
            }
        } else {
            result = EATLErrorInvalidParameters;
            SysLog()->LogError(LOG_CONTEXT, "bad hardware access: %s", hw_access.c_str());
        }

        initialized = result.IsOk();

        return result;
    }

    CATLError CAccessManager::Open()
    {
        if (opened) {
            SysLog()->LogError(LOG_CONTEXT, "attempt to start already opened access manager");
            return EATLErrorUnsupported;
        }
        CATLError res;

        if (reg_bus_manager) {
            if (SysConfig()->HasOption("hw-dev")) { // if hw dev name is in config, just open it
                SysLog()->LogDebug(LOG_CONTEXT, "opening reg bus manager for device %s", SysConfig()->GetValue("hw-dev").c_str());
                res = reg_bus_manager->Open();
                if (res.IsError()) {
                    SysLog()->LogError(LOG_CONTEXT, "can't open register bus manager for device %s", SysConfig()->GetValue("hw-dev").c_str());
                }
            } else {
                SysLog()->LogError(LOG_CONTEXT, "no connected devices set for hardware bus manager");
                res = EATLErrorFatal;
            }
        }
        if (res.IsOk() && fw_bus_manager) {
            if (SysConfig()->HasOption("fw-dev")) { // if fw dev name is in config, just open it
                res = fw_bus_manager->Open();
                if (res.IsError()) {
                    SysLog()->LogError(LOG_CONTEXT, "can't open firmware bus manager for device %s", SysConfig()->GetValue("fw-dev").c_str());
                }
            } else {
                SysLog()->LogError(LOG_CONTEXT, "no connected devices set for firmware bus manager");
                res = EATLErrorFatal;
            }
        }
        if (res.IsOk()) switch (fw_access_type) {
            case None: break;
            case Direct: if (reg_bus_manager) fw_dbg_acc.Start(); break;
            case Stream: if (reg_bus_manager || fw_bus_manager) fw_str_acc.Start(); break;
            default: res = EATLErrorInvalidParameters;
        }
        opened = res.IsOk();
        return res;
    }

    basebool CAccessManager::isConfigured() const
    {
        return (initialized && opened);
    }

    CATLError CAccessManager::Close()
    {
        opened = false;
        initialized = false;
        CATLError res;
        switch (fw_access_type) {
            case None: break;
            case Direct: fw_dbg_acc.Stop(); break;
            case Stream: fw_str_acc.Stop(); break;
            default: res = EATLErrorNotInitialized;
        }
        if (reg_bus_manager) res |= reg_bus_manager->Close();
        if (fw_bus_manager) res |= fw_bus_manager->Close();
        return res;
    }

    CATLError CAccessManager::Terminate()
    {
        opened = false;
        initialized = false;
        CATLError res;
        if (reg_bus_manager) res |= reg_bus_manager->Terminate();
        if (fw_bus_manager) res |= fw_bus_manager->Terminate();
        hw_access_type = NotConfigured;
        fw_access_type = NotConfigured;
        return res;
    }

    CATLError CAccessManager::PostFWRequest(TSmartPtr<CFWRequest> request)
    {
        switch (fw_access_type) {
            case None:    return EATLErrorNotImplemented;
            case Direct:  return fw_dbg_acc.PostFWRequest(request);
            case Stream:  return fw_str_acc.PostFWRequest(request);
            default: {
                SysLog()->LogError(LOG_CONTEXT, "firmware channel is not initialized properly");
                return EATLErrorNotInitialized;
            }
        }
    }

    TSmartPtr<CFWRequest> CAccessManager::GetFWResult(baseid id, UInt32 timeout_ms)
    {
        switch (fw_access_type) {
            case None:    return EATLErrorNotImplemented;
            case Direct:  return fw_dbg_acc.GetFWResult(id,timeout_ms);
            case Stream:  return fw_str_acc.GetFWResult(id,timeout_ms);
            default: {
                SysLog()->LogError(LOG_CONTEXT, "firmware channel is not initialized properly");
                return EATLErrorNotInitialized;
            }
        }
    }

    CATLError CAccessManager::PostRegRequest(TSmartPtr<CRegRequest> request)
    {
        switch (hw_access_type) {
            case Direct: return reg_dir_acc.PostRegRequest(request);
            case Stream: return fw_str_acc.PostRegRequest(request);
            default: {
                SysLog()->LogError(LOG_CONTEXT, "hardware channel is not initialized properly");
                return EATLErrorNotInitialized;
            }
        }
    }

    TSmartPtr<CRegRequest> CAccessManager::GetRegResult(baseid id, UInt32 timeout_ms)
    {
        switch (hw_access_type) {
            case Direct:
                SysLog()->LogError(LOG_CONTEXT, "CAccessManager::GetRegResult" FSIZE_T, timeout_ms);
                return reg_dir_acc.GetRegResult(id,timeout_ms);
            case Stream: return fw_str_acc.GetRegResult(id,timeout_ms);
            default: {
                SysLog()->LogError(LOG_CONTEXT, "hardware channel is not initialized properly");
                return EATLErrorNotInitialized;
            }
        }
    }

    basesize CAccessManager::GetRegSize() const
    {
        switch (hw_access_type) {
            case Direct: return reg_dir_acc.GetRegSize();
            case Stream: return fw_str_acc.GetRegSize();
            default: return -1;
        }
    }

    basesize CAccessManager::GetFWSize() const
    {
        switch (hw_access_type) {
            case Direct: return reg_dir_acc.GetFWSize();
            case Stream: return fw_str_acc.GetFWSize();
            default: return -1;
        }
    }

    const std::string CAccessManager::GetFWAccessType() const {
        switch (fw_access_type) {
            case Direct:          return "direct";
            case Stream:          return "stream";
            case None:            return "none";
            case NotConfigured:   return "not-configured";
            default:                        return "unkbown";
        }
    }
    const std::string CAccessManager::GetHWAccessType() const {
        switch (hw_access_type) {
            case Direct:          return "direct";
            case Stream:          return "stream";
            case None:            return "none";
            case NotConfigured:   return "not-configured";
            default:                        return "unkbown";
        }
    }

}
