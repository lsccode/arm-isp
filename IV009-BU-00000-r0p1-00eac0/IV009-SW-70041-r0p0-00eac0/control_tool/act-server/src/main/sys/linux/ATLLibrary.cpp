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

#include <ATL/ATLLibrary.h>

#include <dlfcn.h>

namespace atl {

    void* CATLLibrary::LoadSharedLibrary(const char *name, ELoadLibraryMode mode) {
       return dlopen(name, mode ) ;
    }

    void* CATLLibrary::GetFunction(void *hLib, const char *Fnname) {
         return dlsym(hLib, Fnname ) ;
    }

    basebool CATLLibrary::FreeSharedLibrary(void *hLib) {
        return ( 0 == dlclose( hLib ) ) ; // returns 0 on success
    }

} // atl namespace

