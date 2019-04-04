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

#ifndef __ATL_LIBRARY__
#define __ATL_LIBRARY__

#include "ATLError.h"
#include "ATLTypes.h"

namespace atl {

    typedef enum _ELoadLibraryMode {
        RTLD_LAZY = 1,
        RTLD_NOW  = 2,
        RTLD_GLOBAL =  4
    } ELoadLibraryMode ;

    class CATLLibrary  {
    public:
        static void* LoadSharedLibrary(const char *name, ELoadLibraryMode mode = RTLD_NOW ) ;
        static void* GetFunction(void *hLib, const char *Fnname) ;
        static basebool FreeSharedLibrary(void *hLib) ;
    };

} // atl namespace

#endif // __ATL_CONFIG_STRING__
