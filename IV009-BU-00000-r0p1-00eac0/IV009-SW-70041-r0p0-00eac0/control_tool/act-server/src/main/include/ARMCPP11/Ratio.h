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

#ifndef RATIO_H
#define RATIO_H

#include <ctime>

namespace armcpp11
{

typedef unsigned long long  UIntMax;
typedef long long           IntMax;

template <IntMax A, IntMax B>
struct GCD
{
    static IntMax const value = GCD<B, A % B>::value;
};

template <IntMax A>
struct GCD<A, 0>
{
    static UIntMax const value = A;
};

template <IntMax N>
struct ABS
{
    static const UIntMax value = (N < 0) ? -N : N;
};

template <IntMax N>
struct SIGN
{
    static const IntMax value = (N < 0) ? -1 : 1;
};

template <IntMax N, IntMax D = 1>
struct Ratio
{
    static const IntMax Num = SIGN<N>::value * SIGN<D>::value *
                              ABS<N>::value / GCD<N, D>::value;
    static const IntMax Den = ABS<D>::value / GCD<N, D>::value;
};

typedef Ratio<1, 1000000000>       Nano;
typedef Ratio<1, 1000000>          Micro;
typedef Ratio<1, 1000>             Milli;
typedef Ratio<1, 100>              Centi;
typedef Ratio<1, 10>               Deci;
typedef Ratio<10, 1>               Deca;
typedef Ratio<100, 1>              Hecto;
typedef Ratio<1000, 1>             Kilo;
typedef Ratio<1000000, 1>          Mega;
typedef Ratio<1000000000, 1>       Giga;

template <class R1, class R2>
struct RatioDivide
{
private:
    typedef GCD<R1::Num * R2::Den, R1::Den * R2::Num> GCDType;
    typedef Ratio<((R1::Num * R2::Den) / GCDType::value),
                  ((R1::Den * R2::Num) / GCDType::value)> RatioType;

public:
    typedef RatioType type;
    static const IntMax Num = RatioType::Num;
    static const IntMax Den = RatioType::Den;
};

}

#endif // RATIO_H
