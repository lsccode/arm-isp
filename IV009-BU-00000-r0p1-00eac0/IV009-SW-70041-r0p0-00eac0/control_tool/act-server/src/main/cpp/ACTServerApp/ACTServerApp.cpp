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

#define TOSTRING(x) #x
#define TOSTRING_VALUE_OF(x) TOSTRING(x)

#include <version.h>

#define LOG_CONTEXT "ACTServer"

#include <ATL/ATLApplication.h>
#include <ATL/ATLConfig.h>
#include <CommandManager/CommandManager.h>
#include <HTTPServer/EventEngine.h>
#include <HTTPServer/HTTPServer.h>
#include <BusManager/BusManager.h>
#include <BusManager/DriverFactory.h>

#include "ACTServerApp.h"
#ifdef ACT_SUPPORT_LISTEN_COMMAND
#include "ACTListenCommand.h"
#endif // ACT_SUPPORT_LISTEN_COMMAND
#ifdef ACT_SUPPORT_DIRECT_COMMAND
#include "ACTDirectCommand.h"
#endif // ACT_SUPPORT_DIRECT_COMMAND

#include <sstream>
#include <algorithm>
#include <map>

using namespace std;
using namespace atl;
using namespace act;

#define APP_NAME        "Control Tool"
#define APP_VENDOR      "ARM"
#define APP_URL         "https://arm.com/"

static std::map<std::string, std::string> defaults;

static void CreateMap()
{
    defaults["hw-acc"] =      "direct";      // hw access
    defaults["fw-acc"] =      "hw";          // fw access
    defaults["hw-channel"] =  "i2c";         // hw channel (driver)
    defaults["log"] =         "info";        // log level: 3 - info
    defaults["hw-buf-size"] = "1024";        // default hw buffer size
    defaults["dbg-offset"] =  "0";           // default debug offset for offline driver
}

const std::vector<std::string> GetSupportedDevices() {
    TSmartPtr<CBusManager> bm = new CBusManager();
    bm->Initialize(BUS_HW);
    const std::vector<std::string> devs = bm->GetSupportedDevices();
    bm->Terminate();
    return devs;
}

const std::vector<std::string> GetSupportedDrivers() {
    return CDriverFactory::GetSupportedDrivers();
}

std::string GetDevice() {

    std::string result;
    std::string hw_access = SysConfig()->GetValue("hw-acc");
    if (hw_access == "direct" && !SysConfig()->HasOption("hw-dev")) { // we need hw device name - so ask it from user
        std::vector<std::string> devices = GetSupportedDevices();
        if (devices.size() > 0) {
            if (devices.size() == 1) {
                result = devices.at(0);
            } else {
                basesize size = devices.size();
                SysLog()->LogInfo(LOG_CONTEXT, "-----------------------------------------------");
                SysLog()->LogInfo(LOG_CONTEXT, "         Please choose a desired device        ");
                for (basesize idx = 0; idx < size; idx++) {
                    SysLog()->LogInfo(LOG_CONTEXT, " - " FSIZE_T " - %s", idx, devices.at(idx).c_str());
                }
                SysLog()->LogInfo(LOG_CONTEXT, "-----------------------------------------------");
                UInt32 num = 0;
                std::cout << "> select device number: ";
                std::cin >> num;
                if (num >= size || num < 0) {
                    num = 0;
                    SysLog()->LogError(LOG_CONTEXT, "invalid number device number: exiting...");
                    CATLError::SetLastError(EATLErrorInvalidParameters);
                }
                CATLError::SetLastError(EATLErrorOk);
                result = devices.at(num);
            }
            SysLog()->LogInfo(LOG_CONTEXT, "%s device will be used", result.c_str() );
        } else {
            CATLError::SetLastError(EATLErrorFatal);
            SysLog()->LogError(LOG_CONTEXT, "no supported hardware detected: exiting");
        }
    }

    return result;
}

void CACTServerApp::ShowUsage() {
    ShowApplicationTitle();
    cout << "" << endl;
    cout << " Usage: " <<  SysConfig()->GetExec() <<  " [ <queries> | [ [<options>] [<command>] ] ]" << endl;
    cout << "" << endl;
    cout << " where supported queries are: " << endl;
    cout << "  --version  - get application version" << endl;
    cout << "  --channels - get list of channels (drivers) supported on your platform" << endl;
    cout << "  --devices  - get list of available devices for a given channel (use --hw-channel option to specify channel)" << endl;
    cout << "" << endl;
    cout << " and options are:" << endl;
    cout << "" << endl;
    cout << " * general options:" << endl;
    cout << "  --preset <file>      - load settings from property file" << endl;
    cout << "  --log <n>            - level of logging: 0: none, 1: errors, 2: warnings, 3: info (default), 4: debug" << endl;
    cout << "  --hw-channel <name>  - channel for hardware access" << endl;
    cout << "  --fw-channel <name>  - channel for firmware access" << endl;
    cout << "  --hw-acc <name>      - hardware access type: direct or stream" << endl;
    cout << "  --fw-acc <name>      - firmware access type: direct, stream, none, hw (default), sw" << endl;
    cout << "  --dbg-offset <n>     - offset of debug buffer in bytes" << endl;
    cout << "  --hw-buf-offset <n>  - offset of hardware buffer in bytes" << std::endl;
    cout << "  --hw-buf-size <n>    - size of hardware buffer in bytes" << std::endl;
    cout << "  --hw-drv-path <path> - path to the dynamic library with external hw driver" << endl;
    cout << "  --fw-drv-path <path> - path to the dynamic library with external fw driver" << endl;
    cout << "" << endl;
    cout << " * channel (driver) options:" << endl;
    cout << "  --hw-dev <name>      - device name for hardware channel" << endl;
    cout << "  --hw-mem-offset <n>  - hardware start address of memory buffer" << endl;
    cout << "  --hw-mem-size <n>    - hardware size of memory buffer in bytes" << endl;
    cout << "  --hw-baud-rate <n>   - hardware baud rate (default: 115200)" << endl;
    cout << "  --fw-dev <name>      - device name for firmware channel" << endl;
    cout << "  --fw-mem-offset <n>  - firmware start address of memory buffer" << endl;
    cout << "  --fw-mem-size <n>    - firmware size of memory buffer in bytes" << endl;
    cout << "  --fw-baud-rate <n>   - firmware baud rate (default: 115200)" << endl;
    cout << "" << endl;
    cout << " and supported commands are:" << endl << endl;
#ifdef ACT_SUPPORT_LISTEN_COMMAND
    cout << " * listen - this is default command: use CT server as a middleware to access HW and FW via GUI" << endl;
    CACTListenCommand::ShowUsage();
#endif // ACT_SUPPORT_LISTEN_COMMAND
#ifdef ACT_SUPPORT_DIRECT_COMMAND
    cout << " * direct - communicate with hardware and firmware directly" << endl;
    CACTDirectCommand::ShowUsage();
#endif // ACT_SUPPORT_DIRECT_COMMAND
#ifdef ACT_DEFAULT_COMMAND
    cout << " default command: " << TOSTRING_VALUE_OF(ACT_DEFAULT_COMMAND) << endl;
#endif // ACT_DEFAULT_COMMAND
    cout << "" << endl;
}

void CACTServerApp::ShowVersion() {
    ShowApplicationTitle();
}

void CACTServerApp::ShowApplicationTitle() {
    std::cout << APP_NAME << " " << __MODULE_VERSION << " of " << __SOURCE_DATE_TIME << " (built: " << __DATE__ << " " << __TIME__ << ")" << std::endl;
    std::cout << "(C) COPYRIGHT " << __COPYRIGHT_INYR << " - " << __COPYRIGHT_YEAR << " ARM Limited or its affiliates." << std::endl;
}

CATLError CACTServerApp::Initialize() {
    CreateMap();
    return SysConfig()->Merge(defaults);
}

CATLError CACTServerApp::InitCommands() {

    if (SysConfig()->HasOption("hw-channel") && SysConfig()->GetValue("hw-channel") == "i2c" && (!SysConfig()->HasOption("hw-dev"))) {
        std::string device = GetDevice();
        SysConfig()->SetOption("hw-dev", device);
        if (CATLError::GetLastError().IsError()) {
            return CATLError::GetLastError();
        }
    }
#ifdef ACT_DEFAULT_COMMAND
    std::string command = SysConfig()->GetCommand(TOSTRING_VALUE_OF(ACT_DEFAULT_COMMAND));
#else
    std::string command = SysConfig()->GetCommand();
#endif // ACT_DEFAULT_COMMAND

    SysLog()->LogDebug(LOG_CONTEXT, "running command %s", command.c_str());
    return CreateCommand(command);
}

CATLError CACTServerApp::CreateCommand(const std::string& name) {
#ifdef ACT_SUPPORT_LISTEN_COMMAND
    if (name == "listen") {
        command = new CACTListenCommand();
        return EATLErrorOk;
    }
#endif // ACT_SUPPORT_LISTEN_COMMAND
#ifdef ACT_SUPPORT_DIRECT_COMMAND
    if (name == "direct") {
        command = new CACTDirectCommand();
        return EATLErrorOk;
    }
#endif // ACT_SUPPORT_DIRECT_COMMAND
    SysLog()->LogError(LOG_CONTEXT, "command %s not supported", name.c_str());
    return EATLErrorInvalidParameters;

}

CATLError CACTServerApp::CheckForImmediateAction() {
    CATLError res = CATLApplication::CheckForImmediateAction();     // call to base function for most basic options
    if (res == EATLErrorImmediateActionHandled) {
        return res;
    }

    if (SysConfig()->HasOption("devices")) {                        // list supported devices for current driver
        SysLog()->SetLogLevel(ELogError);
        const std::vector<std::string> list = GetSupportedDevices();
        std::cout << "Available devices for channel " << SysConfig()->GetValue("hw-channel") << ":" << std::endl;
        for (std::vector<std::string>::const_iterator it = list.begin();
             it != list.end(); ++it) {
//        for (std::string const& entry : list) {
            std::cout << " - " << *it << std::endl;
        }
        return EATLErrorImmediateActionHandled;
    } else if (SysConfig()->HasOption("channels") || SysConfig()->HasOption("drivers")) { // list supported drivers
        SysLog()->SetLogLevel(ELogError);
        const std::vector<std::string> list = GetSupportedDrivers();
        std::cout << "Supported drivers (channels):" << std::endl;
        for (std::vector<std::string>::const_iterator it = list.begin();
             it != list.end(); ++it) {
//        for (std::string const& entry : list) {
            std::cout << " - " << *it << std::endl;
        }
        return EATLErrorImmediateActionHandled;
    } else {
        return EATLErrorOk;
    }

    return res;
}
