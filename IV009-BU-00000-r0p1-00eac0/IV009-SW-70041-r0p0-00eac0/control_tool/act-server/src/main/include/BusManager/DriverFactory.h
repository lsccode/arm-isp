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

#ifndef __BUS_FACTORY_H__
#define __BUS_FACTORY_H__

#include <ATL/ATLTemplates.h>

#include "BaseDriverInterface.h"

#include <string>
#include <vector>

using namespace atl ;

namespace act {
    class CDriverFactory {
    public:
        static TSmartPtr<IDriver> CreateDriver(const std::string &provider);
        const static std::vector<std::string> GetSupportedDrivers();
    };
}

#endif // __BUS_FACTORY_H__
