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

#ifndef __ATL_ERROR__
#define __ATL_ERROR__

#include "ATLTypes.h"

#ifndef _WIN32
    #include <errno.h>
#endif

namespace atl {

    typedef enum __EATLErrors {
        EATLErrorOk = 0,
        EATLErrorFatal,
        EATLErrorInvalidParameters,
        EATLErrorNoMemory,
        EATLErrorNoMoreData,
        EATLErrorNoFreeSpace,
        EATLErrorNoEnoughBuffer,
        EATLErrorUnsupported,
        EATLErrorNotImplemented,
        EATLErrorDeviceError,
        EATLErrorInvalidValue,
        EATLErrorNotInitialized,
        EATLErrorTimeout,
        EATLErrorNotChanged,
        EATLErrorImmediateActionHandled
    } EATLErrors;

    typedef enum __EATLErrorTypes {
        EATLMessage = 0,
        EATLError,
        EATLWarning,
        EATLInfo
    } EATLErrorTypes;


    class CATLError {

    protected :
        static UInt32 lastError;

    protected:
        UInt32 errorData;

    public:

        inline CATLError( atl::Int32 err ): errorData (err) {}

        inline CATLError( const CATLError& err ): errorData(err.GetErrorCode()) {}

        inline CATLError()  {
            errorData = EATLErrorOk;
        }

        inline ~CATLError() {
        }

        inline basebool IsOk() const { return( errorData == EATLErrorOk ); }

        inline basebool IsError() const { return( !IsOk() ); }

        CATLError& operator |=( const CATLError& err ) {
            if( IsOk() ) {
                errorData |=  err.GetErrorCode();
            }
            return( *this );
        }

        inline UInt32 GetErrorCode() const { return ( errorData ); }

        inline CATLError& operator = (const CATLError& err) {
            errorData = err.GetErrorCode();
            return( *this );
        }

        inline CATLError& operator = (UInt32 err_code) {
            errorData = err_code;
            return( *this );
        }

        inline CATLError& operator = (atl::Int32 err_code) {
            *this = UInt32(err_code);
            return( *this );
        }

       UInt32 operator==( UInt32 err_code )  const {
            UInt32 res = errorData == err_code;
            return res;
        }

       UInt32 operator!=( UInt32 err_code )  const {
            UInt32 res = errorData != err_code;
            return res;
        }

        // Overloads C++11 Compatibility for C++0x
       inline CATLError& operator = (EATLErrors err_code) {
           errorData = static_cast<UInt32>(err_code);
           return( *this );
       }

       UInt32 operator!=( EATLErrors err_code )  const {
            UInt32 res = errorData != (UInt32)err_code;
            return res;
        }

       inline CATLError& operator = (EATLErrorTypes err_code) {
           errorData = static_cast<UInt32>(err_code);
           return( *this );
       }

       UInt32 operator!=( EATLErrorTypes err_code )  const {
            UInt32 res = errorData != (UInt32)err_code;
            return res;
        }

    public:

        inline static CATLError GetLastError() { return lastError; }
        static void SetLastError( UInt32 err ) { CATLError::lastError = err; }
        static void SetLastError( CATLError err ) { CATLError::lastError = err.GetErrorCode(); }
        static void ClearLastError() { CATLError::lastError = EATLErrorOk; }

    };

} // atl namespace

#endif // __ATL_ERROR__
