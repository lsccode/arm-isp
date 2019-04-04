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

#if !defined(__ATL_PLATFORM__)
#define __ATL_PLATFORM__

#if defined(_MSC_VER)
#pragma warning(disable: 4250) // inherits class::member via dominance
#endif

#ifdef _WIN32
    #include <stdlib.h>
    #include <stdarg.h>
    #define _WINDOWS
//    #define WIN32 // this is for libevent headers only
    #if !defined( WIN32_LEAN_AND_MEAN )
        #define WIN32_LEAN_AND_MEAN
    #endif
#elif defined(linux) || defined(__linux) || defined(__linux__)
    // normally you should not use it
    #define _LINUX
#else
    #error Unsupported platform
#endif


// redifinitions
#ifdef _WIN32
    #define _STDCALL __stdcall
    #define _CDECL __cdecl
    const char FILE_SEPARATOR = '\\';
    #define FSIZE_T    "%Iu"
    const char * __strerror(int errnum, char *buf, size_t buflen);
    int __vsnprintf(char* buffer, size_t buf_size, const char* format, va_list vlist);
#else // _WIN32
    #define _STDCALL
    #define _CDECL
    #define _stricmp strcasecmp
    #define __strerror strerror_r
    typedef int SOCKET;
    const char FILE_SEPARATOR = '/';
    #define FSIZE_T    "%zu"
    #define __vsnprintf vsnprintf
#endif // _WIN32

#endif //!defined(__ATL_PLATFORM__)
