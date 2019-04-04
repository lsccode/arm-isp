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

#include <ATL/ATLObject.h>
#include <ATL/ATLTemplates.h>
#include <ATL/ATLLogger.h>

#include <cassert>

namespace atl {

    CATLObject::~CATLObject() {
        assert (refCounter == 0);
    }

    refcounter CATLObject::AddRef() {
        return refCounter.FetchAdd(1);
    }

    refcounter CATLObject::Release() {
        refcounter result = refCounter.FetchSub(1);
        if (result == 1) {
            delete this;
        } else if ( (result == 0) || (result == (refcounter)0xFEFEFEFE) ) {
            throw "ATL object: invalid reference";
        }
        return result;
    }

} // atl namespace
