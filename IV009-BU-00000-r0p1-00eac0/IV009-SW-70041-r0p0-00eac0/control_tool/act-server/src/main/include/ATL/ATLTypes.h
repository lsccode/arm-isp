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

#ifndef __ATL_BASE_TYPES__
#define __ATL_BASE_TYPES__

#include <string>
#include <sstream>
#include <cctype>

#include <ATL/ATLPlatform.h>

namespace atl {

    typedef short               Int16;
    typedef long                Int32;
    typedef long long           IntMax;

    typedef unsigned char       UInt8;
    typedef unsigned char       UChar;
    typedef unsigned short      UInt16;
    typedef unsigned long        UInt32;
    typedef unsigned long long  UIntMax;

    typedef bool        basebool;
    typedef UInt32    baseaddr;
    typedef size_t      basesize;
    typedef float       basefloat;
    typedef long        refcounter;
    typedef UInt32    flags32;
    typedef size_t      baseid;

    UInt8  stou8 (const std::string& data);
    UInt16 stou16(const std::string& data);
    UInt32 stou32(const std::string& data);

    std::string utos(const UInt8& value);
    std::string utos(const UInt16& value);
    std::string utos(const UInt32& value);


    static const int MIN_BASE = 2;
    static const int MAX_BASE = 36;
    // Decimal conversion map for ASCII characters
    static const int digit_map[256] = {
        -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1,
         0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
        -1, -1, -1, -1, -1, -1, -1,
        10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
        20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
        30, 31, 32, 33, 34, 35,
        -1, -1, -1, -1, -1, -1,
        10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
        20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
        30, 31, 32, 33, 34, 35,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1
    };

    UIntMax convert(char const* pstr, int const base);
    UIntMax strtonum(std::string const& str, int base);
} // atl name space

#define array_size(a) (sizeof(a)/sizeof(a[0]))

#endif // !__ATL_BASE_TYPES__
