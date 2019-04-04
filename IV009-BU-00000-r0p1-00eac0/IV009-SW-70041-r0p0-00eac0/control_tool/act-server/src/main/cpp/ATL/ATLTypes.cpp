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

#include <cstdio>
#include <string>
#include <limits>
#include <stdexcept>

#include <ATL/ATLTypes.h>

using namespace std;

atl::UIntMax atl::convert(char const* pstr, int const base)
{
    UIntMax res = 0;

    // Proceed only if the base is within valid limits
    if (base >= MIN_BASE && base <= MAX_BASE)
    {
        while (*pstr)
        {
            // Get the decimal conversion of the current char from the map
            const int numeric_digit = digit_map[static_cast<UChar>(*pstr)];
            // If the returned conversion is -1 or not less than base, break
            if (numeric_digit == -1 || numeric_digit >= base)
            {
                break;
            }
            // Add the next digit after shifting the current result
            res = res * base + numeric_digit;
            // Position to the next char
            ++pstr;
        }
    }
    return res;
}

atl::UIntMax atl::strtonum(std::string const& str, int base)
{
    const char *pstr = str.c_str();

    // Skip all leading whitespaces
    for (; std::isspace(static_cast<UChar>(*pstr)); ++pstr);

    // Check if the number is signed
    bool const is_signed = *pstr == '+' || *pstr == '-';
    // sign is true if positive and false if negative
    bool const sign = is_signed && *pstr++ == '+';

    /**
     * Check if the first digit is 0. This is required to determine if the
     * number is hex and octal base when user specified base is 0.
     */
    bool const zero_prefix = *pstr == '0';
    // Skip over the 0, if present
    pstr += zero_prefix;

    // If zero prefixed and base == 0, determine if the number is hex or oct
    bool const is_hex = base == 16 ||
                        (base == 0 && zero_prefix &&
                         (*pstr == 'x' || *pstr == 'X'));
    bool const is_oct = base == 8 || (!is_hex && base == 0 && zero_prefix);

    // Skip the iterator over x, if present
    pstr += is_hex;

    // Determine the base if not user specified
    base = is_hex ? 16 : is_oct ? 8 : base == 0 ? 10 : base;

    // Convert the digits to number
    UIntMax res = convert(pstr, base);

    /**
     * If the input number is signed and the sign is negative, then we need to
     * apply the unsinged number wrap around logic by subtracting the result
     * from the maximum of uintmax_t.
     */
    if (is_signed && !sign /* Negative */ && res)
    {
        res = std::numeric_limits<UIntMax>::max() - res + 1;
    }

    return res;
}

atl::UInt8  atl::stou8 (const std::string& data) {
    if (data == "null" || data == "NaN") {
        return 0;
    }
    if (data == "null" || data == "NaN") {
        return 0;
    }
    UIntMax z = strtonum(data, 0);
    if (z < 0 || z > numeric_limits<UInt8>::max()) {
        return 0xff;
    }
    return (UInt8)z;
}

atl::UInt16 atl::stou16(const std::string& data) {
    if (data == "null" || data == "NaN") {
        return 0;
    }
    UIntMax z = strtonum(data, 0);
    if (z < 0 || z > numeric_limits<UInt16>::max()) {
        return 0xffff;
    }
    return (UInt16)z;
}

atl::UInt32 atl::stou32(const string& data) {
    if (data == "null" || data == "NaN") {
        return 0;
    }
    UIntMax z = strtonum(data, 0);
    if (z < 0 || z > numeric_limits<UInt32>::max()) {
        return 0xffffffff;
    }
    return (UInt32)z;
}

string atl::utos(const UInt8& value) {
    char buf[32];
    std::sprintf(buf, "%u", value);
    return buf;
}

string atl::utos(const UInt16& value) {
    char buf[32];
    std::sprintf(buf, "%u", value);
    return buf;
}

string atl::utos(const UInt32& value) {
    char buf[32];
    std::sprintf(buf, "%lu", value);
    return buf;
}

