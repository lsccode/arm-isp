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

#ifdef ACT_SUPPORT_LISTEN_COMMAND

#define LOG_CONTEXT "ACTListen"

#include "ACTListenCommand.h"
#include "HTTPServer/EventEngine.h"
#include "HTTPServer/HTTPServer.h"

#include <map>

using namespace act;

static std::map<std::string, std::string> defaults;

CACTListenCommand::CACTListenCommand() {
    SysConfig()->Merge(defaults);
}

CATLError CACTListenCommand::Execute() {

    CATLError result;
    SysLog()->LogDebug(LOG_CONTEXT, "start execution...");

    server = new CHTTPServer();
    result = server->Initialize();
    if (result.IsOk()) {
        server->OnServerFinish += new TATLObserver<CACTListenCommand>(this, &CACTListenCommand::OnServerFinish);
        result = server->Open();
    }

    SysLog()->LogDebug(LOG_CONTEXT, "finished execution");
    return result;

}

CATLError CACTListenCommand::Stop() {
    CATLError result;
    if ( server != NULL ) {
        result = server->Close();
        if (result.IsError()) {
            result = server->Terminate();
        }
    }
    return result;
}

void CACTListenCommand::OnServerFinish(void*) {
    SysLog()->LogInfo(LOG_CONTEXT, "HTTP server stopped");
}

void CACTListenCommand::ShowUsage() {
    std::cout << "  --http-port <n>     - port for http server: 8000 is default" << std::endl;
    std::cout << "  --http-address <ip> - bind address for http server: 0.0.0.0 is default" << std::endl;
    std::cout << "  --http-root <path>  - relative path to document root" << std::endl;
    std::cout << "" << std::endl;
}

#endif // ACT_SUPPORT_LISTEN_COMMAND

