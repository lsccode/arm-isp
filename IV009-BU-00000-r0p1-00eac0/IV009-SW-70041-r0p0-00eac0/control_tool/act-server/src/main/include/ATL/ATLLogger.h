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

#ifndef __ATL_LOGGER__
#define __ATL_LOGGER__

#include "ATLTypes.h"
#include <stdarg.h>
#include <fstream>
#include <iostream>
#include <ARMCPP11/Mutex.h>

namespace atl {

    typedef enum __ELogLevels {
        ELogNothing = 0,    // 0
        ELogError,          // 1
        ELogWarning,        // 2
        ELogInfo,           // 3
        ELogDebug           // 4
    } ELogLevels;

    class CATLLogger {
    private:
        armcpp11::RecursiveMutex sync;
        std::string filePath;
        std::ofstream logStream;
        static CATLLogger logger;

    public:
        ELogLevels level;

    private:
        std::string getPrefix(const ELogLevels& lvl, const char* context);
        void ALog(const ELogLevels& lvl, const char* context, const char* format, va_list va);

    public:
        CATLLogger(const ELogLevels& level = ELogError) : level(level) {}
        ~CATLLogger();

        void SetLogLevel(const ELogLevels& lvl);

        void LogError   (const char* context, const char* format, ... );
        void LogWarning (const char* context, const char* format, ... );
        void LogInfo    (const char* context, const char* format, ... );
        void LogDebug   (const char* context, const char* format, ... );

    public:
        static inline CATLLogger* GetSysLog() { return &logger; }
    };

    inline CATLLogger* SysLog() {
        return CATLLogger::GetSysLog();
    }

} // atl namespace

#endif // __ATL_LOGGER__
