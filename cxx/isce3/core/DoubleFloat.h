#pragma once

#include <cmath>
#include <limits>

#include "Common.h"

namespace isce3 { namespace core {

static_assert(std::numeric_limits<float>::is_iec559,
        "DoubleFloat requires IEEE 754 binary32 floats");

/**
 * Extended-precision scalar represented as the unevaluated sum of two floats
 * ("double-float" or "df64" arithmetic).
 *
 * The value is hi + lo with |lo| <= 0.5 ulp(hi), giving an effective
 * significand of about 48 bits (vs 24 for float and 53 for double). All
 * operations use only binary32 add/mul/FMA, so they run at full rate on GPUs
 * whose native float64 throughput is 1/32 to 1/64 of float32 (e.g. NVIDIA
 * T4, A10G, L4). Intended for the delay/carrier-phase computation in
 * time-domain backprojection, where plain float32 is far too coarse but the
 * full 53-bit significand of double is not needed.
 *
 * References:
 * Dekker (1971), "A floating-point technique for extending the available
 * precision", doi:10.1007/BF01397083.
 * Hida, Li & Bailey (2001), "Algorithms for quad-double precision floating
 * point arithmetic" (the QD library).
 * Thall (2006), "Extended-precision floating-point numbers for GPU
 * computation".
 */
struct DoubleFloat {
    float hi;
    float lo;

    DoubleFloat() = default;

    /** Construct from a float (exact). */
    CUDA_HOSTDEV constexpr explicit DoubleFloat(float f) : hi(f), lo(0.0f) {}

    /** Construct from a hi/lo pair. Requires |lo| <= 0.5 ulp(hi). */
    CUDA_HOSTDEV constexpr DoubleFloat(float hi, float lo) : hi(hi), lo(lo) {}

    /**
     * Split a double into hi + lo floats, preserving its leading 48
     * significand bits.
     *
     * This involves float64 arithmetic, so in performance-critical GPU code
     * call it once per pulse/target during setup, never in the integration
     * loop.
     */
    CUDA_HOSTDEV constexpr explicit DoubleFloat(double d)
        : hi(static_cast<float>(d)),
          lo(static_cast<float>(d - static_cast<double>(static_cast<float>(d))))
    {}

    CUDA_HOSTDEV constexpr explicit operator double() const
    {
        return static_cast<double>(hi) + static_cast<double>(lo);
    }
};

namespace detail {

/** Compute a + b = s + e exactly (Knuth two-sum, 6 flops). */
CUDA_HOSTDEV inline DoubleFloat twoSum(float a, float b)
{
    const float s = a + b;
    const float bb = s - a;
    const float e = (a - (s - bb)) + (b - bb);
    return {s, e};
}

/**
 * Compute a + b = s + e exactly (Dekker fast two-sum, 3 flops).
 * Requires |a| >= |b| or a == 0.
 */
CUDA_HOSTDEV inline DoubleFloat quickTwoSum(float a, float b)
{
    const float s = a + b;
    const float e = b - (s - a);
    return {s, e};
}

/**
 * Compute a * b = p + e exactly (2 flops).
 *
 * The FMA computes the exact rounding error of the product. Explicit
 * round-to-nearest intrinsics are used in device code so that compiler
 * contraction settings (or --use_fast_math) cannot break the error-free
 * transformation.
 */
CUDA_HOSTDEV inline DoubleFloat twoProd(float a, float b)
{
#ifdef __CUDA_ARCH__
    const float p = __fmul_rn(a, b);
    const float e = __fmaf_rn(a, b, -p);
#else
    const float p = a * b;
    const float e = std::fmaf(a, b, -p);
#endif
    return {p, e};
}

} // namespace detail

CUDA_HOSTDEV inline DoubleFloat operator-(DoubleFloat a)
{
    return {-a.hi, -a.lo};
}

/** Add (IEEE-style accurate double-float sum, ~20 flops). */
CUDA_HOSTDEV inline DoubleFloat operator+(DoubleFloat a, DoubleFloat b)
{
    DoubleFloat s = detail::twoSum(a.hi, b.hi);
    const DoubleFloat t = detail::twoSum(a.lo, b.lo);
    s.lo += t.hi;
    s = detail::quickTwoSum(s.hi, s.lo);
    s.lo += t.lo;
    return detail::quickTwoSum(s.hi, s.lo);
}

CUDA_HOSTDEV inline DoubleFloat operator-(DoubleFloat a, DoubleFloat b)
{
    return a + (-b);
}

/** Multiply (~9 flops). */
CUDA_HOSTDEV inline DoubleFloat operator*(DoubleFloat a, DoubleFloat b)
{
    DoubleFloat p = detail::twoProd(a.hi, b.hi);
    p.lo += a.hi * b.lo + a.lo * b.hi;
    return detail::quickTwoSum(p.hi, p.lo);
}

/**
 * Square root via one Newton step from a full-precision float32 seed
 * (Karp & Markstein style). Each step doubles the significand, so the
 * result carries the full ~48-bit double-float precision.
 */
CUDA_HOSTDEV inline DoubleFloat sqrt(DoubleFloat a)
{
    if (a.hi == 0.0f) {
        return DoubleFloat(0.0f);
    }
#ifdef __CUDA_ARCH__
    const float x = __frsqrt_rn(a.hi);
#else
    const float x = 1.0f / std::sqrt(a.hi);
#endif
    const float y = x * a.hi;
    const DoubleFloat y2 = detail::twoProd(y, y);
    const float diff = (a - y2).hi;
    return detail::quickTwoSum(y, diff * (x * 0.5f));
}

/**
 * Return a minus a nearby integer, as a float with |result| < ~1.
 *
 * The result equals a modulo 1 (up to a whole integer, which the intended
 * consumer - sin/cos of a phase in cycles - cannot observe). The integer
 * subtraction is exact (Sterbenz lemma), so this wraps values with millions
 * of whole cycles down to a small residual without any loss of precision
 * beyond that of the double-float input itself.
 */
CUDA_HOSTDEV inline float roundedRemainder(DoubleFloat a)
{
#ifdef __CUDA_ARCH__
    const float n = rintf(a.hi);
#else
    const float n = std::rint(a.hi);
#endif
    return (a.hi - n) + a.lo;
}

}} // namespace isce3::core
