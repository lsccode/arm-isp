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

#ifndef __ACCESS_REQUEST_H__
#define __ACCESS_REQUEST_H__

#include <ATL/ATLTypes.h>
#include <ATL/ATLError.h>
#include <ATL/ATLObject.h>

#include <ARMCPP11/Atomic.h>
#include <vector>

using namespace atl ;

namespace act {

    enum EFWRequestType {
        FWUnknown = 0,    // unknown request
        APIRead,        // API read request
        APIWrite,        // API write request
        BUFRead,        // Buffer read request
        BUFWrite        // Buffer write request
    };
    const char* EFWRequestTypeStr(const EFWRequestType &type);

    enum ERegRequestType {
        RegUnknown = 0,    // unknown request
        RegRead,        // register read request
        RegWrite,        // register write request
        LUTRead,        // LUT read request
        LUTWrite        // LUT write request
    };
    const char* ERegRequestTypeStr(const ERegRequestType& type);

    enum ERequestState {
        Processing = 0, // Request in processing
        Success,        // successfully processed
        NoAnswer,       // No answer from device
        Timeout,        // Timeout of waiting for reply
        Fail            // Request is failed
    };
    const char* ERequestStateStr(const ERequestState& state);

    class CFWRequest : public CATLObject {
    public :
        CFWRequest();
        virtual const std::string GetObjectStaticName() ;

        const baseid id;            // unique request id
        ERequestState state;        // current state of the request
        EFWRequestType type;        // reqest type - API, BUF etc.

        UInt8 cmd_type;       // command type
        UInt8 cmd;            // api command number
        UInt32 value;         // value sent to or returned by FW
        UInt16 status;        // command status returned by FW
        std::vector<UInt8> data;  // input/output array for buffers

        UInt32 timeout_ms;        // timeout value
    protected:
        ~CFWRequest();                // usage only with Smart Pointers

    private:
        static armcpp11::Atomic<baseid> id_counter; // atomic counter for requests
    };

    class CRegRequest : public CATLObject {
    public :
        CRegRequest();
        virtual const std::string GetObjectStaticName() ;

        const baseid id;            // unique request id
        ERequestState state;        // current state of the request
        ERegRequestType type;        // reqest type - reg, LUT etc.

        baseaddr address;           // address to read/write opeartion
        std::vector<UInt8> data;  // input/output array for data
        std::vector<UInt8> mask;  // input/output array for mask

    protected:
        ~CRegRequest();                // usage only with Smart Pointers

    private:
        static armcpp11::Atomic<baseid> id_counter; // atomic counter for requests
    };
}

#endif // __ACCESS_REQUEST_H__
