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

#ifndef __ATL_BASE_OBJECT__
#define __ATL_BASE_OBJECT__

#include "ATLTypes.h"

#include <string>
#include <ARMCPP11/Atomic.h>

namespace atl {

    class IATLObject {
    public:
        virtual ~IATLObject() {}
        virtual refcounter AddRef() = 0;
        virtual refcounter Release() = 0;
        virtual const std::string GetObjectStaticName() = 0;
    };

    class CATLObject : public virtual IATLObject {
    protected:
        armcpp11::Atomic< refcounter > refCounter ;
    protected:
        inline CATLObject() : refCounter(0) {}
    public:
        virtual ~CATLObject();
        virtual refcounter AddRef();
        virtual refcounter Release();

    } ;

} // atl namespace

#endif // __ATL_BASE_OBJECT__
