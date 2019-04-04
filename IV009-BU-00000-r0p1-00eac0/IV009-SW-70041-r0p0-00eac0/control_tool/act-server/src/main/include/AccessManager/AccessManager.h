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

#ifndef __ACCESS_MANAGER_H__
#define __ACCESS_MANAGER_H__

#include <ATL/ATLTypes.h>
#include <ATL/ATLError.h>
#include <ATL/ATLObject.h>
#include <ATL/ATLTemplates.h>
#include <BusManager/BusManager.h>

#include "AccessRequest.h"
#include "RegisterDirectAccess.h"
#include "FWDebugAccess.h"
#include "FWStreamAccess.h"

#include <string>
#include <deque>
#include <list>

using namespace atl;

namespace act {

    class CAccessManager :  public virtual CATLObject, public virtual IATLComponent {
    public:
        CAccessManager();
        virtual ~CAccessManager();

        CATLError PostFWRequest(TSmartPtr<CFWRequest> request);                    // post FW requests
        CATLError PostRegRequest(TSmartPtr<CRegRequest> request);                  // post register and LUT requests
        TSmartPtr<CFWRequest> GetFWResult(baseid id, UInt32 timeout_ms = 0);     // get FW request result by id, timeout = 0 means wait infinite
        TSmartPtr<CRegRequest> GetRegResult(baseid id, UInt32 timeout_ms = 0);   // get FW request result by id, timeout = 0 means wait infinite

        basesize GetRegSize() const;    // returns sizes of queues
        basesize GetFWSize() const;     // returns sizes of queues
        basebool isConfigured() const;
        const std::string GetHWAccessType() const;
        const std::string GetFWAccessType() const;

        // IATLObject
        virtual const std::string GetObjectStaticName();

        // IATLComponent
        virtual const std::string GetName();
        virtual CATLError Initialize(const flags32& options = 0);
        virtual CATLError Open();
        virtual CATLError Close();
        virtual CATLError Terminate();

    private:
        TSmartPtr<CBusManager> reg_bus_manager; // bus manager for direct register access
        TSmartPtr<CBusManager> fw_bus_manager;  // bus manager for streamed FW access

        // channel types
        enum EAccessType {NotConfigured, Direct, Stream, None};

        EAccessType hw_access_type;                 // harware channel type
        EAccessType fw_access_type;                 // firmware channel type

        CRegisterDirectAccess reg_dir_acc;      // direct access to registers
        CFWDebugAccess fw_dbg_acc;              // access to FW through debug registers
        CFWStreamAccess fw_str_acc;             // access to FW through stream interface (includes 1K buffer)

        basebool initialized;
        basebool opened;
    };

}

#endif // __ACCESS_MANAGER_H__
