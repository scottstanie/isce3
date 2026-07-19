#pragma once

#include <isce3/core/Common.h>
#include <isce3/core/DoubleFloat.h>
#include <isce3/core/Vector.h>
#include <isce3/core/forward.h>

namespace isce3 { namespace focus {

/**
 * Compute the two-way propagation delay between the radar antenna phase center
 * and a target scatterer.
 *
 * The delay model applies a bistatic correction for the displacement of the
 * radar between transmit and receive time.
 *
 * Speed of light in vacuum is assumed. No corrections are applied for delays
 * due to atmospheric effects.
 *
 * \param[in] p Antenna phase center position (m) at transmit time
 * \param[in] v Antenna phase center velocity (m/s) at transmit time
 * \param[in] x Target position (m)
 * \returns     The bistatic propagation delay (s)
 */
CUDA_HOSTDEV
double bistaticDelay(const isce3::core::Vec3 & p,
                     const isce3::core::Vec3 & v,
                     const isce3::core::Vec3 & x);

/**
 * A 3-vector of double-float components.
 *
 * Carries ECEF coordinates with sub-micrometer precision using only float32
 * storage and arithmetic. (A single float32 ECEF coordinate is only good to
 * ~0.5 m - about two carrier wavelengths of round-trip phase at L-band.)
 */
struct DoubleFloatVec3 {
    isce3::core::DoubleFloat x, y, z;

    DoubleFloatVec3() = default;

    /**
     * Split each component of a double-precision vector.
     *
     * Involves float64 arithmetic - call during setup, not in inner loops.
     */
    CUDA_HOSTDEV explicit DoubleFloatVec3(const isce3::core::Vec3 & v)
        : x(v[0]), y(v[1]), z(v[2])
    {}
};

/**
 * Precompute the pulse-invariant scale factor of the bistatic delay,
 * w = 2 / (|v|^2 - c^2), for the double-float bistaticDelay overload.
 *
 * \param[in] v Antenna phase center velocity (m/s) at transmit time
 */
CUDA_HOSTDEV
isce3::core::DoubleFloat bistaticDelayScale(const isce3::core::Vec3 & v);

/**
 * Compute the two-way propagation delay in double-float arithmetic.
 *
 * Computes the same quantity as the double-precision overload,
 * 2 (r.v - c |r|) / (|v|^2 - c^2) with r = x - p, using only float32
 * operations (see isce3::core::DoubleFloat). The denominator is passed in
 * as the precomputed per-pulse constant w (see bistaticDelayScale), which
 * also eliminates the division from the integration loop.
 *
 * Delay error is ~1e-14 relative (micrometers in range units), versus
 * several cm for naive float32 - sufficient for interferometric-quality
 * carrier phase, unlike naive float32 which produces O(1 rad) phase error.
 *
 * \param[in] p Antenna phase center position (m) at transmit time
 * \param[in] v Antenna phase center velocity (m/s) at transmit time
 * \param[in] x Target position (m)
 * \param[in] w Precomputed scale factor 2 / (|v|^2 - c^2) (s^2/m^2)
 * \returns     The bistatic propagation delay (s)
 */
CUDA_HOSTDEV
isce3::core::DoubleFloat bistaticDelay(const DoubleFloatVec3 & p,
                                       const DoubleFloatVec3 & v,
                                       const DoubleFloatVec3 & x,
                                       isce3::core::DoubleFloat w);

}}

#include "BistaticDelay.icc"
