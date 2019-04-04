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

#include <sstream>

#define LOG_CONTEXT "ATLConfig"

using namespace std;

namespace atl {

    CATLConfig CATLConfig::global;

    const std::string CATLConfig::GetExec() const {
        return exec;
    }

    const basebool CATLConfig::HasOption(const std::string &name) const {
        return (options.find(name) != options.end());
    }

    const basebool CATLConfig::HasCommand() const {
        return (arguments.size() > 0);
    }

    const string CATLConfig::GetCommand() const {
        if (HasCommand()) {
            return command;
        } else {
            SysLog()->LogError(LOG_CONTEXT, "no command specified");
            CATLError::SetLastError(EATLErrorInvalidParameters);
            return "";
        }
    }

    const string CATLConfig::GetCommand(const std::string& fallback) const {
        if (HasCommand()) {
            return command;
        } else {
            SysLog()->LogDebug(LOG_CONTEXT, "default command used: %s", fallback.c_str());
            return fallback;
        }
    }

    const string CATLConfig::GetValue(const string& name) const {
        if (HasOption(name)) {
            return options.at(name);
        } else {
            SysLog()->LogError(LOG_CONTEXT, "failed to get required option `%s`", name.c_str());
            CATLError::SetLastError(EATLErrorInvalidParameters);
            return "";
        }
    }

    const UInt8 CATLConfig::GetValueAsU8(const string &name) const {
        if (HasOption(name)) {
            return stou8(options.at(name));
        } else {
            SysLog()->LogError(LOG_CONTEXT, "failed to get required option `%s`", name.c_str());
            CATLError::SetLastError(EATLErrorInvalidParameters);
            return 0;
        }
    }

    const UInt16 CATLConfig::GetValueAsU16(const string &name) const {
        if (HasOption(name)) {
            return stou16(options.at(name));
        } else {
            SysLog()->LogError(LOG_CONTEXT, "failed to get required option `%s`", name.c_str());
            CATLError::SetLastError(EATLErrorInvalidParameters);
            return 0;
        }
    }

    const UInt32 CATLConfig::GetValueAsU32(const string &name) const {
        if (HasOption(name)) {
            return stou32(options.at(name));
        } else {
            SysLog()->LogError(LOG_CONTEXT, "failed to get required option `%s`", name.c_str());
            CATLError::SetLastError(EATLErrorInvalidParameters);
            return 0;
        }
    }

    CATLError CATLConfig::SetOption(const string &key, const UInt8 &value) {
        return SetOption(key, utos(value));
    }

    CATLError CATLConfig::SetOption(const string &key, const UInt16 &value) {
        return SetOption(key, utos(value));
    }

    CATLError CATLConfig::SetOption(const string &key, const UInt32 &value) {
        return SetOption(key, utos(value));
    }

    CATLError CATLConfig::SetOption(const string &key, const string &value) {
        if (key.size() > 1) {
            options[key] = value;
            SysLog()->LogDebug(LOG_CONTEXT, "option value set --%s %s", key.c_str(), value.c_str());
            return EATLErrorOk;
        } else {
            return EATLErrorInvalidParameters;
        }
    }

    CATLError CATLConfig::Parse(const std::vector<string> &args) {

        const static std::string equals = "=";
        const static char hyphen = '-';

        exec.clear();
        options.clear();
        arguments.clear();
        command = "";

        string arg;
        basesize as = args.size(), next;
        exec = args.at(0);
        basebool is_option;
        basesize caret;

        for (basesize i = 1; i < as; i++) { // skip application name
            user_command_line += (args.at(i) + " ");
        }

        for (basesize i = 1; i < as; i++) { // skip application name

            arg = args.at(i);

            if ((caret = arg.find(equals)) != std::string::npos) { // we have format --option-name=option-value
                std::string oname  = arg.substr(0, caret);
                std::string ovalue = arg.substr(caret + 1);
                if (oname.size() > 1 && oname.at(0) == hyphen && ovalue.size() > 0) { // probably good option
                    if (oname.at(1) == hyphen && oname.size() > 3) { // long option name like --opt-name
                        options[oname.substr(2)] = ovalue;
                    } else if (oname.size() == 2) { // good short option name like -p
                        options[oname.substr(1)] = ovalue;
                    } else {
                        SysLog()->LogError(LOG_CONTEXT, "bad option name: %s", oname.c_str());
                        return EATLErrorInvalidParameters;
                    }
                } else {
                    SysLog()->LogError(LOG_CONTEXT, "bad option: %s", arg.c_str());
                    return EATLErrorInvalidParameters;
                }
                continue;
            }

            // format is --option-name option-value
            is_option = (arg.size() >= 2);
            if (is_option && arg.at(0) == '-') { // start of option name
                next = i + 1;
                if (arg.at(1) == '-' && arg.size() > 3) { // long option name like --opt-name
                    if (next >= as) { // option without explicit value
                        options[arg.substr(2)] = ""; // empty string means that options is set but has no value
                    } else {
                        if (args.at(next)[0] == '-')  { // current option without explicit value
                            options[arg.substr(2)] = ""; // empty string means that options is set but has no value
                        } else {
                            options[arg.substr(2)] = args.at(next);
                            i++; // jump over option value which is already processed
                        }
                    }
                } else if (arg.size() == 2) { // short option name like -p
                    if (next >= as) { // option without explicit value
                        options[arg.substr(1)] = ""; // empty string means that options is set but has no value
                    } else {
                        if (args.at(next)[0] == '-')  { // current option without explicit value
                            options[arg.substr(1)] = "";// empty string means that options is set but has no value
                        } else {
                            options[arg.substr(1)] = args.at(next);
                            i++; // jump over option value which is already processed
                        }
                    }
                } else {
                    SysLog()->LogError(LOG_CONTEXT, "bad option name: %s", arg.c_str());
                    return EATLErrorInvalidParameters;
                }
            } else { // either a command or an argument
                arguments.push_back(arg);
                if (arguments.size() == 1) {
                    command = arguments.at(0);
                }
            }

        }

        return EATLErrorOk;

    }

    CATLError CATLConfig::Merge(const std::string& prefix, const std::map<std::string,std::string>& defaults) {
        CATLError res;
        for (std::map<std::string, std::string>::const_iterator it = defaults.begin();
             it != defaults.end(); ++it) {
            if (HasOption(prefix + it->first)) {} else {
                res = SetOption(prefix + it->first, it->second);
                if (res.IsError()) {
                    break;
                }
            }
        }
        return res;
    }

    CATLError CATLConfig::Merge(const std::map<std::string,std::string>& defaults) {
        CATLError res;
        for (std::map<std::string, std::string>::const_iterator it = defaults.begin();
             it != defaults.end(); ++it) {
            if (HasOption(it->first)) {} else {
                res = SetOption(it->first, it->second);
                if (res.IsError()) {
                    break;
                }
            }
        }
        return res;
    }

    CATLError CATLConfig::Override(const CATLConfig& other) {
        CATLError res;
        for (std::map<std::string, std::string>::const_iterator it = other.options.begin();
             it != other.options.end(); ++it) {
            res = SetOption(it->first, it->second);
            if (res.IsError()) {
                break;
            }
        }
        return res;
    }

    const std::string CATLConfig::GetFullCommandLine() const {
        std::string line;
        const static std::string dashes = "--";
        const static std::string space = " ";
        const static std::string equals = "=";
        const static std::string quote = "\"";

        for (std::map<std::string, std::string>::const_iterator it = options.begin();
             it != options.end(); ++it) { // list options with values (if any)
            if (it->second.size() > 0) { // option has explicit value
                line += (dashes + it->first + equals + it->second + space);
            } else {
                line += (dashes + it->first + space);
            }
        }

        for (std::vector<std::string>::const_iterator it = arguments.begin();
             it != arguments.end(); ++it) { // list command (which is first argument) and its arguments
            if (it->find(space) != std::string::npos) { // argument contains space
                line += (quote + *it + quote + space);
            } else {
                line += (*it + space);
            }
        }

        return line.substr(0, line.size() - 1); // remove trailing space
    }

    const std::string CATLConfig::GetUserCommandLine() const {
        return user_command_line.substr(0, user_command_line.size() - 1); // remove trailing space
    }

} // atl namespace
