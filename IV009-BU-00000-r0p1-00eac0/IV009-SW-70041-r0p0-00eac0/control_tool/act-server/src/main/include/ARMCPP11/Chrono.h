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

#ifndef CHRONO_H
#define CHRONO_H

#ifdef _WIN32
#include <windows.h>
#endif

#include <ARMCPP11/Ratio.h>
#include <limits>
#include <ctime>
#include <cassert>

namespace armcpp11
{

template <bool> struct StaticAssert;
template <> struct StaticAssert<true> {};

//############################################################################//
//                                                                            //
//                             DURATION VALUES                                //
//                                                                            //
//############################################################################//
template <class Type>
struct DurationValues
{
    static const Type Zero() { return (Type)0; }
    static const Type Max() { return std::numeric_limits<Type>::max(); }
    static const Type Min() { return std::numeric_limits<Type>::min(); }
};

//############################################################################//
//                                                                            //
//                                 DURATION                                   //
//                                                                            //
//############################################################################//
template <class Type, class Period = Ratio<1> >
class Duration
{
public:
    typedef Type type;
    typedef Period period;

    Duration(const Type& t) : mTicks(t)
    { }

    template <class Type2, class Period2>
    Duration(const Duration<Type2, Period2>& d)
        : mTicks(d.Count() * RatioDivide<Period2, Period>::Num)
    {
        StaticAssert<RatioDivide<Period2, Period>::Den == 1>();
    }

    static const Duration Zero() { return Duration(DurationValues<Type>::Zero()); }
    static const Duration Max() { return Duration(DurationValues<Type>::Max()); }
    static const Duration Min() { return Duration(DurationValues<Type>::Min()); }

    template <class Type2>
    Duration<UIntMax, Period>& operator*=(const Type2& t)
    {
        mTicks *= t;
        return *this;
    }

    Duration& operator+=(const Duration& d)
    {
        mTicks += d.Count();
        return *this;
    }

    Duration& operator-=(const Duration& d)
    {
        mTicks -= d.Count();
        return *this;
    }

    Duration& operator*=(const Duration& d)
    {
        mTicks *= d.Count();
        return *this;
    }

    const Type Count() const
    {
        return mTicks;
    }

private:
    Type mTicks;
};

template <class Type, class Period, class Type2>
Duration<UIntMax, Period> operator*(const Type2& t,
                                    const Duration<Type, Period>& d)
{
    return Duration<UIntMax, Period>(d.Count() * t);
}

template <class Type, class Period, class Type2>
Duration<UIntMax, Period> operator*(const Duration<Type, Period>& d,
                                    const Type2& t)
{
    return Duration<UIntMax, Period>(d.Count() * t);
}

template <class Type, class Period, class Type2>
Duration<UIntMax, Period> operator+(const Type2& t,
                                    const Duration<Type, Period>& d)
{
    return Duration<UIntMax, Period>(d.Count() + t);
}

template <class Type, class Period, class Type2>
Duration<UIntMax, Period> operator+(const Duration<Type, Period>& d,
                                    const Type2& t)
{
    return Duration<UIntMax, Period>(d.Count() + t);
}

template <class Duration>
Duration operator-(const Duration& d1, const Duration& d2)
{
    return Duration(d2.Count() - d1.Count());
}


//############################################################################//
//                                                                            //
//                                DURATION CAST                               //
//                                                                            //
//############################################################################//
template <class ResDuration,
          class Duration,
          class Period,
          bool  PeriodNumEq1,
          bool  PeriodDenEq1>
struct DurationCastHelper;

template <class ResDuration, class Duration, class Period>
struct DurationCastHelper<ResDuration, Duration, Period, true, true>
{
    const ResDuration operator()(const Duration& d) const
    {
        return ResDuration((typename ResDuration::type) d.Count());
    }
};

template <class ResDuration, class Duration, class Period>
struct DurationCastHelper<ResDuration, Duration, Period, true, false>
{
    const ResDuration operator()(const Duration& d) const
    {
        return ResDuration(((typename ResDuration::type) d.Count()) /
                           ((typename ResDuration::type) Period::Den));
    }
};

template <class ResDuration, class Duration, class Period>
struct DurationCastHelper<ResDuration, Duration, Period, false, true>
{
    const ResDuration operator()(const Duration& d) const
    {
        return ResDuration(((typename ResDuration::type) d.Count()) *
                           ((typename ResDuration::type) Period::Num));
    }
};

template <class ResDuration, class Duration, class Period>
struct DurationCastHelper<ResDuration, Duration, Period, false, false>
{
    const ResDuration operator()(const Duration& d) const
    {
        return ResDuration(((typename ResDuration::type) d.Count()) *
                           ((typename ResDuration::type) Period::Num) /
                           ((typename ResDuration::type) Period::Den));
    }
};

template <class OutDuration, class InDuration>
const OutDuration DurationCast(const InDuration& d)
{
    typedef typename RatioDivide<typename InDuration::period,
                                 typename OutDuration::period>::type Period;

    typedef DurationCastHelper<OutDuration,
                               InDuration,
                               Period,
                               Period::Num == 1,
                               Period::Den == 1> Helper;

    return Helper()(d);
}

typedef Duration<UIntMax, Nano>             NanoSeconds;
typedef Duration<UIntMax, Micro>            MicroSeconds;
typedef Duration<UIntMax, Milli>            MilliSeconds;
typedef Duration<UIntMax>                   Seconds;
typedef Duration<UIntMax, Ratio<60> >       Minutes;
typedef Duration<UIntMax, Ratio<3600> >     Hours;

//############################################################################//
//                                                                            //
//                                  TIME POINT                                //
//                                                                            //
//############################################################################//
template <class Clock, class Duration = typename Clock::duration>
class TimePoint
{
public:
    typedef Clock                       clock;
    typedef Duration                    duration;
    typedef typename Duration::type     type;
    typedef typename Duration::period   period;

    TimePoint() : mDuration(Duration::Zero())
    { }

    explicit TimePoint(const Duration& d) : mDuration(d)
    { }

    template <class Duration2>
    TimePoint(const TimePoint<clock, Duration2>& t)
        : mDuration(t.TimeSinceEpoch())
    { }

    duration TimeSinceEpoch() const { return mDuration; }

    static const TimePoint Max() { return TimePoint(Duration::Max()); }
    static const TimePoint Min() { return TimePoint(Duration::Min()); }

    TimePoint& operator +=(const duration& d) { mDuration += d; return *this; }
    TimePoint& operator -=(const duration& d) { mDuration -= d; return *this; }

private:
    Duration mDuration;
};

template <class Clock, class Duration>
Duration operator -(const TimePoint<Clock, Duration>& t1,
                    const TimePoint<Clock, Duration>& t2)
{
    return t2.TimeSinceEpoch() - t1.TimeSinceEpoch();
}

template <class ToDuration, class Clock, class Duration>
TimePoint<Clock, ToDuration> TimePointCast(const TimePoint<Clock, Duration>& t)
{
    return TimePoint<Clock, ToDuration>(
                                DurationCast<ToDuration>(t.TimeSinceEpoch()));
}

//############################################################################//
//                                                                            //
//                                    CLOCKS                                  //
//                                                                            //
//############################################################################//
//############################################################################//
//                                                                            //
//                                 SYSTEM CLOCK                               //
//                                                                            //
//############################################################################//
typedef time_t      Time_t;
class SystemClock
{
public:
    typedef NanoSeconds             duration;
    typedef duration::type          type;
    typedef duration::period        period;
    typedef TimePoint<SystemClock>  timepoint;
    static const bool IsSteady    = false;

    static inline timepoint Now()
    {
#ifndef _WIN32
        timespec    ts;
        assert(clock_gettime(CLOCK_REALTIME, &ts) == 0);
        return timepoint(
                    duration(
                             static_cast<type>(ts.tv_sec * 1000000000L +
                                               ts.tv_nsec)));
#else
        FILETIME ft;
        GetSystemTimeAsFileTime(&ft);
        return timepoint(duration(
                             (((static_cast<IntMax>(ft.dwHighDateTime) << 32) |
                               ft.dwLowDateTime) - 116444736000000000LL)
                             * 100));
#endif
    }

    static inline Time_t ToTime(const timepoint& t);
    static inline timepoint FromTime(const Time_t t);
};

typedef SystemClock HighResolutionClock;

}

#endif // CHRONO_H
