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

#include <AccessManager/AccessRequest.h>
#include <ATL/ATLLogger.h>
#include <ARMCPP11/Atomic.h>

#define LOG_CONTEXT ()

namespace act {

    armcpp11::Atomic<baseid> CFWRequest::id_counter(0); // atomic counter

    CFWRequest::CFWRequest() : id(id_counter.FetchAdd(1)),
                               cmd_type(0), cmd(0), value(0), status(0)
    {
        type = FWUnknown;     // initialy type is unknown
        state = Processing;  // and state is processing
        timeout_ms = 10000;                 // timeout 10 seconds
    }

    CFWRequest::~CFWRequest() {
        data.clear();   // clear data just in case
    }

    const std::string CFWRequest::GetObjectStaticName() {
        return "FWRequest" ;
    }

    armcpp11::Atomic<baseid> CRegRequest::id_counter(0); // atomic counter

    CRegRequest::CRegRequest() : id(id_counter.FetchAdd(1))  {
        type = RegUnknown;     // initialy type is unknown
        state = Processing;  // and state is processing
    }

    CRegRequest::~CRegRequest() {
        data.clear();   // clear data and mask just in case
        mask.clear();
    }

    const std::string CRegRequest::GetObjectStaticName() {
        return "RegRequest" ;
    }

    const char* EFWRequestTypeStr(const EFWRequestType& type) {
        switch (type) {
        case FWUnknown:   return "unknown";
        case APIRead:   return "API READ";
        case APIWrite:  return "API WRITE";
        case BUFRead:   return "BUF READ";
        case BUFWrite:  return "BUF WRITE";
        default: return "unknown";
        }
    }

    const char* ERegRequestTypeStr(const ERegRequestType &type) {
        switch (type) {
        case RegUnknown:  return "unknown";
        case RegRead:  return "REG READ";
        case RegWrite: return "REG WRITE";
        case LUTRead:  return "LUT READ";
        case LUTWrite: return "LUT WRITE";
        default: return "unknown";
        }
    }

    const char* ERequestStateStr(const ERequestState& state) {
        switch (state) {
        case Processing: return "processing";
        case Success:    return "success";
        case NoAnswer:   return "no answer";
        case Timeout:    return "timeout";
        case Fail:       return "failed";
        default: return "unknown";
        }
    }
}
