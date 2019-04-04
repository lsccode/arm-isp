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

#include <ATL/ATLLogger.h>
#include <ATL/ATLError.h>

#include <sstream>
#include <iomanip>

const char* EInfoLevels[] = {
        "",
        "ERROR",
        " WARN",
        " INFO",
        "DEBUG",
        NULL
    };

#include <ARMCPP11/Chrono.h>

inline std::string getTimeStamp() {
    std::stringstream buf;
    std::time_t t = std::time(NULL);
    char buffer[16];
    std::strftime(buffer, 16, "%H:%M:%S", std::localtime(&t));
    buf << std::string(buffer) << "." << std::setfill('0');
    armcpp11::TimePoint<armcpp11::SystemClock> now = armcpp11::SystemClock::Now();
    armcpp11::TimePoint<armcpp11::SystemClock> epoch;
    buf << std::setw(3) << (armcpp11::DurationCast<armcpp11::MilliSeconds>(now - epoch).Count() % 1000);
    return buf.str();
}

using namespace std;

namespace atl {

    const static basesize LOG_BUFFER_SIZE = 4096;
    static char __log_buffer [LOG_BUFFER_SIZE];

    CATLLogger CATLLogger::logger;

    CATLLogger::~CATLLogger() {
        if (logStream.is_open()) {
            logStream.close();
        }
    }

    std::string CATLLogger::getPrefix(const ELogLevels &lvl, const char *context) {
        if( context == NULL ) {
            context = "" ;
        }
        std::stringstream buf;
        buf << getTimeStamp() << " ";
        buf << std::setfill(' ') << std::right << " ";
        buf << EInfoLevels[lvl] << " > " << context << ": ";
        return buf.str();
    }

    void CATLLogger::ALog(const ELogLevels& lvl, const char * context, const char * format, va_list va ) {
        armcpp11::LockGuard<armcpp11::RecursiveMutex> lock( sync ) ;
        char * buffer = __log_buffer ;
        if( buffer != NULL ) {
            stringstream bs;
            string prefix = getPrefix(lvl, context);
            bs << prefix;
            basesize yi = __vsnprintf(buffer, LOG_BUFFER_SIZE - prefix.size() - 1, format, va);
            buffer[yi] = '\0';
            bs << buffer;
#ifndef _WIN32
            if (lvl == ELogError) { // for red error messages in linux console
                cout << "\033[1;31m";
            }
#endif
            cout << bs.str();
#ifndef _WIN32
            if (lvl == ELogError) {
                cout << "\033[0m";
            }
#endif
            cout << endl;
        } else {
            CATLError::SetLastError(EATLErrorNoMemory);
        }
    }

    void CATLLogger::SetLogLevel(const ELogLevels& lvl) {
        level = lvl ;
    }

    void CATLLogger::LogError(const char* context, const char* format, ... ) {
        if( level >= ELogError ) {
            va_list va ;
            va_start( va, format ) ;
            ALog( ELogError, context, format, va ) ;
            va_end( va ) ;
        }
    }

    void CATLLogger::LogWarning(const char* context, const char* format, ... ) {
        if( level >= ELogWarning ) {
            va_list va ;
            va_start( va, format ) ;
            ALog( ELogWarning, context, format, va ) ;
            va_end( va ) ;
        }
    }

    void CATLLogger::LogInfo(const char* context, const char* format, ... ) {
        if( level >= ELogInfo ) {
            va_list va ;
            va_start( va, format ) ;
            ALog( ELogInfo, context, format, va ) ;
            va_end( va ) ;
        }
    }

    void CATLLogger::LogDebug(const char* context, const char* format, ... ) {
        if( level >= ELogDebug ) {
            va_list va ;
            va_start( va, format ) ;
            ALog( ELogDebug, context, format, va ) ;
            va_end( va ) ;
        }
    }


} // atl namespace
