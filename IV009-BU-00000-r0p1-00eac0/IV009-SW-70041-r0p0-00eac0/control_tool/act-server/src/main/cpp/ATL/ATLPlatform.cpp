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

#include <ATL/ATLPlatform.h>

#ifdef _WIN32
#include <string.h>
#include <stdio.h>
const char * __strerror(int errnum, char *buf, size_t buflen) {
    strerror_s(buf, buflen, errnum);
    return buf;
}
int __vsnprintf(char* buffer, size_t buf_size, const char* format, va_list vlist) {
    int res = vsnprintf_s(buffer, buf_size, buf_size, format, vlist);
    return res;
}
#endif
