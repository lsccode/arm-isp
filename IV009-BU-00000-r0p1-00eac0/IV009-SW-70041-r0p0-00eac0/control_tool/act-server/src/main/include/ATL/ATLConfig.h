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

#ifndef __ATL_CONFIG__
#define __ATL_CONFIG__

#include "ATLError.h"

#include <string>
#include <vector>
#include <map>

namespace atl {

    class CATLConfig  {

    private:
        std::string exec;
        std::map<std::string, std::string> options;
        std::string command;
        std::vector<std::string> arguments;
        std::string user_command_line;
        static CATLConfig global;

    public:

        inline CATLConfig() {}
        inline virtual ~CATLConfig() {}

        CATLError Parse(const std::vector<std::string>& args);

        /**
         * @brief Merge
         *
         * Add options to the current config if they are not set yet
         *
         * @param defaults map with default options
         * @return CATLError
         */
        CATLError Merge(const std::map<std::string,std::string>& defaults);

        /**
         * @brief Merge
         *
         * Add options to the current config if they are not set yet
         * using prefix for each option name
         *
         * @param prefix
         * @param defaults
         * @return
         */
        CATLError Merge(const std::string& prefix, const std::map<std::string,std::string>& defaults);

        /**
         * @brief Override
         *
         * Overwrite all options specified in other config
         *
         * @param other config
         * @return CATLError
         */
        CATLError Override(const CATLConfig& other);

        const std::string GetFullCommandLine() const;
        const std::string GetUserCommandLine() const;

        const std::string GetExec() const;
        const basebool HasOption(const std::string &name) const;
        const basebool HasCommand() const;
        const std::string GetCommand() const;
        const std::string GetCommand(const std::string &fallback) const;

        const std::string GetValue(const std::string &name) const;
        const UInt8 GetValueAsU8(const std::string &name) const;
        const UInt16 GetValueAsU16(const std::string &name) const;
        const UInt32 GetValueAsU32(const std::string &name) const;

        CATLError SetOption(const std::string &key, const UInt8& value);
        CATLError SetOption(const std::string &key, const UInt16& value);
        CATLError SetOption(const std::string &key, const UInt32& value);
        CATLError SetOption(const std::string &key, const std::string& value);


    public:
        static inline CATLConfig* GetSysConfig() { return &global; }

    };

    inline CATLConfig* SysConfig() {
        return CATLConfig::GetSysConfig();
    }

} // atl namespace

#endif // __ATL_CONFIG__
