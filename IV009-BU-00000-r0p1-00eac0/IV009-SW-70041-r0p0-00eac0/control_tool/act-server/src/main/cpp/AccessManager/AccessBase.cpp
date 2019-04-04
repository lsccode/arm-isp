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

#define LOG_CONTEXT "AccessBase"

#include <AccessManager/AccessBase.h>

using namespace std;
using namespace atl;

namespace act {

    CATLError CAccessBase::PostFWRequest(TSmartPtr<CFWRequest>)
    {
        // this virtual function says that transaction is not supported: derived classes has to implement support
        SysLog()->LogWarning(LOG_CONTEXT, "unsupported firmware transaction requested");
        return EATLErrorUnsupported;
    }

    CATLError CAccessBase::PostRegRequest(TSmartPtr<CRegRequest>)
    {
        // this virtual function says that transaction is not supported: derived classes has to implement support
        SysLog()->LogError(LOG_CONTEXT, "unsupported hardware transaction requested");
        return EATLErrorUnsupported;
    }

    TSmartPtr<CFWRequest> CAccessBase::GetFWResult(baseid id, UInt32 timeout_ms)
    {
        TSmartPtr<CFWRequest> res;
        SysLog()->LogDebug(LOG_CONTEXT, "trying to get result for firmware request id " FSIZE_T,id);
        try { // try to get result from list
            res = fw_list.get(id,timeout_ms);    // look up for result
            CATLError::ClearLastError();        // exeption is not thrown - OK
            SysLog()->LogDebug(LOG_CONTEXT, "firmware request id " FSIZE_T " received respose", id);
        }
        catch (TimeoutException) {    // timeout exeption
            SysLog()->LogWarning(LOG_CONTEXT, "firmware access timeout");
            CATLError::SetLastError(EATLErrorTimeout); // no result within timeout
        }
        return res;
    }

    TSmartPtr<CRegRequest> CAccessBase::GetRegResult(baseid id, UInt32 timeout_ms)
    {
        TSmartPtr<CRegRequest> res;
        SysLog()->LogDebug(LOG_CONTEXT, "trying to get result for hardware request id " FSIZE_T,id);
        try { // try to get result from list
            res = reg_list.get(id,timeout_ms);    // look up for result
            CATLError::ClearLastError();        // exeption is not thrown - OK
            SysLog()->LogDebug(LOG_CONTEXT, "Result size: " FSIZE_T, res->data.size());
            SysLog()->LogDebug(LOG_CONTEXT, "hardware request id " FSIZE_T " received respose", id);
        }
        catch (TimeoutException) {    // timeout exeption
            SysLog()->LogError(LOG_CONTEXT, "hardware access timeout") ;
            CATLError::SetLastError(EATLErrorTimeout); // no result within timeout
        }
        return res;
    }
}
