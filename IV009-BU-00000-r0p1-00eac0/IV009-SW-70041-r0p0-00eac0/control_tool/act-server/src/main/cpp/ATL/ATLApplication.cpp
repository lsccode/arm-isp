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

#include <ATL/ATLApplication.h>

#include <algorithm>

#define LOG_CONTEXT "CATLApplication"

inline std::string trim(const std::string& str) {
    std::string result;
    std::size_t first = str.find_first_not_of(" \t");
    if(first == std::string::npos)
        return result;
    std::size_t last = str.find_last_not_of(" \t");
    result = str.substr(first, last - first + 1);
    return result;
}

inline std::pair<std::string, std::string> parseOptionLine(const std::string& line) {

    std::string s = trim(line);
    std::size_t z = s.find("#");
    if (z != std::string::npos) {
        s = trim(s.substr(0, z));
    }
    z = s.find("=");
    if (z == std::string::npos) {
        return std::pair<std::string, std::string>("", "");
    }
    std::string k = trim(s.substr(0, z));
    std::replace(k.begin(), k.end(), '_', '-'); // fix legacy option names
    std::string v = trim(s.substr(z+1));
    return std::pair<std::string, std::string>(k, v);
}

namespace atl {

    CATLError CATLApplication::Main(const std::vector<std::string> &args) {
        CATLError result = ParseParameters(args);
        if (result.IsOk()) {
            result = Initialize();
        } else {
            SysLog()->LogError(LOG_CONTEXT, "unable to parse command line arguments");
        }
        if (result.IsOk()) {
            result = CheckForImmediateAction(); // check for --help -h --version -v options
            if (result == EATLErrorImmediateActionHandled) { // immediate action was handled by application
                return result;
            }
            ShowApplicationTitle();
            result |= InitLog();
            result |= InitCommands();
            if (result.IsOk()) {
                state = EATLApplicationRunning;
                result = Run();
                state = EATLApplicationStopped;
            } else {
                if (result == EATLErrorInvalidParameters) {
                    SysLog()->LogError(LOG_CONTEXT, "bad input parameters");
                } else {
                    SysLog()->LogError(LOG_CONTEXT, "application initialization failed");
                }

            }
        } else {
            SysLog()->LogError(LOG_CONTEXT, "can't initialize application");
        }
        return result;
    }

    CATLError CATLApplication::ApplyPreset() {
        std::string fname;
        if (SysConfig()->HasOption("preset")) {
            fname = SysConfig()->GetValue("preset");
        } else if (SysConfig()->HasOption("p")) {
            fname = SysConfig()->GetValue("p");
        } else { // nothing to do
            return EATLErrorOk;
        }
        if (fname.size() > 0) {
            std::ifstream presetStream(fname.c_str());
            if (presetStream.is_open()) {
                std::string line, k, v;
                while (std::getline (presetStream, line)) {
                    if (line.size() > 0) {
                        std::pair<std::string, std::string> option = parseOptionLine(line);
                        k = option.first;
                        v = option.second;
                        if (k.size() == 0 || v.size() == 0) {
                            continue;
                        }
                        if (SysConfig()->HasOption(k)) {} else { // explicit options have higher priority
                            SysConfig()->SetOption(k, v);
                        }
                    }
                }
                return EATLErrorOk;
            } else {
                SysLog()->LogError(LOG_CONTEXT, "can't open preset file %s", fname.c_str());
                return EATLErrorInvalidParameters;
            }
        } else {
            SysLog()->LogError(LOG_CONTEXT, "empty preset file name");
            return EATLErrorInvalidParameters;
        }
    }

    CATLError CATLApplication::ParseParameters(const std::vector<std::string> &args) {
        CATLError res = SysConfig()->Parse(args);
        if (res.IsOk()) {
            res = ApplyPreset();
        }
        return res;
    }

    CATLError CATLApplication::CheckForImmediateAction () {
        if (SysConfig()->HasOption("h") || SysConfig()->HasOption("help")) {
            ShowUsage();
            return EATLErrorImmediateActionHandled;
        } else if (SysConfig()->HasOption("v") || SysConfig()->HasOption("version")) {
            ShowVersion();
            return EATLErrorImmediateActionHandled;
        } else {
            return EATLErrorOk;
        }
    }

    CATLError CATLApplication::InitLog() {
        std::string ll = SysConfig()->GetValue("log"); // default level - info
        if (ll == "0" || ll == "none" || ll == "n") {
            SysLog()->SetLogLevel(ELogNothing);
        } else if (ll == "1" || ll == "error" || ll == "e") {
            SysLog()->SetLogLevel(ELogError);
        } else if (ll == "2" || ll == "warn" || ll == "w") {
            SysLog()->SetLogLevel(ELogWarning);
        } else if (ll == "3" || ll == "info" || ll == "i") {
            SysLog()->SetLogLevel(ELogInfo);
        } else if (ll == "4" || ll == "debug" || ll == "d") {
            SysLog()->SetLogLevel(ELogDebug);
        } else {
            SysConfig()->SetOption("log", "4");
            SysLog()->SetLogLevel(ELogDebug);
        }
        return EATLErrorOk;
    }

    CATLError CATLApplication::Run() {
        if( command != NULL ) {
            return command->Execute();
        } else {
            return EATLErrorNotInitialized;
        }
    }

    CATLError CATLApplication::Stop() {
        CATLError result = EATLErrorOk;
        state = EATLApplicationStopping;
        if (command != NULL) {
            result = command->Stop();
            SysLog()->LogInfo(LOG_CONTEXT, "application stopped by user request");
        }
        if (result.IsOk()) {
            state = EATLApplicationStopped;
        } else {
            SysLog()->LogError(LOG_CONTEXT, "there was an error when trying to stop application");
        }

        return result;
    }

} // atl namespace
