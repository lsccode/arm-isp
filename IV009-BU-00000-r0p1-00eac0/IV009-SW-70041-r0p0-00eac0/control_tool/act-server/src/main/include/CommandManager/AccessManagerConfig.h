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

#ifndef ACCESSMANAGERCONFIG_H
#define ACCESSMANAGERCONFIG_H

#include <ATL/ATLObject.h>
#include <ATL/ATLTypes.h>

namespace act {

class CAccessManagerConfig : public virtual atl::CATLObject {
public:
    virtual const std::string GetObjectStaticName();
    atl::basebool initialized;
    atl::basebool hasDebugOffset;
    atl::baseaddr debugOffset;
    atl::basebool has1K;
    atl::baseaddr hwBufOffset;
    atl::UInt32 hwBufSize;
};

}

#endif // ACCESSMANAGERCONFIG_H
