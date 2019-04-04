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

#define LOG_CONTEXT "CDriverFactory"

#include <BusManager/DriverFactory.h>

#ifndef _ANDROID // this is not supported on Android platform
#include "Driver/I2CDriver.h"
#include "Driver/MdcMdioDriver.h"
#endif // !_ANDROID

#include "Driver/UARTDriver.h"
#include "Driver/OfflineDriver.h"

#ifndef _WIN32 // Linux-only drivers

#include "Driver/SocketDriver.h"
#include "Driver/CharDeviceDriver.h"

#ifndef _ANDROID // this is not supported on Android platform
#include "Driver/SharedMemoryDriver.h"
#endif // !_ANDROID

#include "Driver/DevMemDriver.h"

#endif // !_WIN32

namespace act {

    TSmartPtr<IDriver> CDriverFactory::CreateDriver(const std::string& channel) {
        TSmartPtr<IDriver> result;
        CATLError err = atl::EATLErrorOk;
        if (false) {
#ifndef _ANDROID // this is not supported on Android platform
        } else if( channel == "i2c" ) {
            try {
                result = new CI2CDriver();
            } catch (...) {
                err = atl::EATLErrorDeviceError;
            }
         } else if( channel == "mdcmdio" ) {
            try {
                result = new CMdcMdioDriver();
            } catch (...) {
                err = atl::EATLErrorDeviceError;
            }
#endif // !_ANDROID
         } else if( channel == "offline" ) {
            try {
                result = new COfflineDriver();
            } catch (...) {
                err = atl::EATLErrorDeviceError;
            }
         } else if( channel == "uart" ) {
            try {
                result = new CUARTDriver();
            } catch (...) {
                err = atl::EATLErrorDeviceError;
            }
#ifndef _WIN32 // Linux-only drivers
         } else if( channel == "socket" ) {
            try {
                result = new CSocketDriver();
            } catch (...) {
                err = atl::EATLErrorDeviceError;
            }
#ifndef _ANDROID // this is not supported on Android platform
         } else if( channel == "sharedmem" ) {
            try {
                result = new CSharedMemoryDriver();
            } catch (...) {
                err = atl::EATLErrorDeviceError;
            }
#endif // !_ANDROID
         } else if( channel == "devmem" ) {
            try {
                result = new CDevMemDriver();
            } catch (...) {
                err = atl::EATLErrorDeviceError;
            }
         } else if( channel == "chardev" ) {
            try {
                result = new CCharDeviceDriver();
            } catch (...) {
                err = atl::EATLErrorDeviceError;
            }
#endif // !_WIN32
        } else {
            err = atl::EATLErrorNotImplemented;
            SysLog()->LogError(LOG_CONTEXT, "driver %s is not supported", channel.c_str());
        }
        if (err.IsOk()) {
            SysLog()->LogDebug(LOG_CONTEXT, "communication driver is %s", channel.c_str());
        } else {
            SysLog()->LogError(LOG_CONTEXT, "failed to create driver %s", channel.c_str());
        }
        CATLError::SetLastError(err);
        return result;
    }

    const std::vector<std::string> CDriverFactory::GetSupportedDrivers( ) {
        std::vector<std::string> result;
#ifndef _ANDROID // this is not supported on Android platform
        result.push_back( "i2c" );
        result.push_back( "mdcmdio" );
#endif // !_ANDROID
        result.push_back( "uart" );
        result.push_back( "offline" );
        result.push_back( "external" );
#ifndef _WIN32 // Linux-only drivers
        result.push_back( "socket" );
#ifndef _ANDROID // this is not supported on Android platform
        result.push_back( "sharedmem" );
#endif // !_ANDROID
        result.push_back( "devmem" );
        result.push_back( "chardev" );
#endif // !_WIN32
        return result;
    }
 }
