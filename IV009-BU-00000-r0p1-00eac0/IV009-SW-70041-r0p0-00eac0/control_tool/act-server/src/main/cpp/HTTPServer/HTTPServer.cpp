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

#define LOG_CONTEXT "HTTPServer"

#include <ATL/ATLConfig.h>
#include <ATL/ATLLogger.h>
#include <ATL/ATLNetwork.h>
#include <CommandManager/CommandManager.h>

#include <HTTPServer/EventEngine.h>
#include <HTTPServer/HTTPServer.h>

static std::map<std::string,std::string> defaults;

static void CreateMap()
{
    defaults["http-root"] =       "./content";   // document root for http server
    defaults["http-port"] =       "8000";        // port which is bound to http server
    defaults["http-address"] =    "0.0.0.0";     // default network interface IP address for http server
}

namespace act {

    CATLError CHTTPServer::Initialize(const flags32&) {
        CreateMap();
        if (state >= EHTTPServerInitialized) {
            SysLog()->LogError(LOG_CONTEXT, "attempt to initialize http server which is already initialized");
            return EATLErrorUnsupported;
        }
        SysConfig()->Merge(defaults);
        CATLError result;
        cm = new CCommandManager();
        if (cm == NULL) {
            return EATLErrorNoMemory;
        }
        result = cm->Initialize();
        if (result.IsOk()) {
            engine = new CEventEngine(cm);
            if (engine == NULL) {
                SysLog()->LogError(LOG_CONTEXT, "failed to instantiate http engine");
            } else {
                result = engine->Initialize();
                if (result.IsOk()) {
                    engine->OnServerFinish += new TATLObserver<CHTTPServer>(this, &CHTTPServer::ServerFinish);
                    state = EHTTPServerInitialized;
                    UInt16 port = SysConfig()->GetValueAsU16("http-port");
                    std::string address = SysConfig()->GetValue("http-address");
                    SysLog()->LogInfo(LOG_CONTEXT, "please open http://%s:%d/ in firefox or chrome", atl::network::getIPAddress(address).c_str(), port);
                } else {
                    SysLog()->LogError(LOG_CONTEXT, "failed to initialize http server");
                }
            }
        } else {
            SysLog()->LogError(LOG_CONTEXT, "failed to initialize command manager");
        }
        return result;
    }

    CATLError CHTTPServer::Open() {
        if (state == EHTTPServerInitialized) {
            CATLError result = cm->Open();
            if (result.IsOk()) {
                state = EHTTPServerRunning;
                CATLError result = engine->Open(); // wait until engine is stopped
                if (result.IsError()) {
                    SysLog()->LogError(LOG_CONTEXT, "failed to start http server");
                    state = EHTTPServerInitialized;
                }
            }
            return result;
        } else {
             SysLog()->LogError(LOG_CONTEXT, "http server has not been initialized");
             return EATLErrorNotInitialized;
        }
    }

    CATLError CHTTPServer::Close() {
        if (state == EHTTPServerRunning && engine->GetState() == EHTTPEngineRunning) {
            CATLError result = engine->Close();
            if (result.IsOk()) {
                state = EHTTPServerStopped;
            } else {
                SysLog()->LogError(LOG_CONTEXT, "failed to stop http engine");
            }
            return result;
        } else {
            SysLog()->LogError(LOG_CONTEXT, "http server is not running");
            return EATLErrorNotInitialized;
        }
    }

    CATLError CHTTPServer::Terminate() {
        return EATLErrorOk;
    }

    void CHTTPServer::ServerFinish(void*) {
        SysLog()->LogInfo(LOG_CONTEXT, "http server stops");
        OnServerFinish.Invoke(this);
    }

}
