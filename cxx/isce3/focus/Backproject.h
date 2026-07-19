#pragma once

#include <isce3/container/forward.h>
#include <isce3/core/forward.h>
#include <isce3/geometry/forward.h>

#include <complex>

#include <isce3/error/ErrorCode.h>
#include <isce3/geometry/detail/Geo2Rdr.h>
#include <isce3/geometry/detail/Rdr2Geo.h>

#include "DryTroposphereModel.h"

namespace isce3 {
namespace focus {

/**
 * Floating-point arithmetic used for the delay & carrier phase computation
 * in the backprojection integration loop
 */
enum class PhaseArithmetic {
    /** IEEE double precision (the reference implementation) */
    Double,

    /**
     * Double-float ("df64") arithmetic built from float32 operations only
     * (see isce3::core::DoubleFloat). Carrier phase error is on the order
     * of a microradian. Intended for GPUs whose float64 throughput is a
     * small fraction of float32; on CPUs it is typically not faster than
     * Double.
     */
    DoubleFloat,
};

/**
 * Focus in azimuth via time-domain backprojection
 *
 * \param[out] out             Output focused signal data
 * \param[in]  out_geometry    Target output grid, orbit, & doppler to focus to
 * \param[in]  in              Input range-compressed signal data
 * \param[in]  in_geometry     Input data grid, orbit, & doppler
 * \param[in]  dem             DEM
 * \param[in]  fc              Center frequency (Hz)
 * \param[in]  ds              Desired azimuth resolution (m)
 * \param[in]  kernel          1-D interpolation kernel
 * \param[in]  dry_tropo_model Dry troposphere path delay model
 * \param[in]  r2g_params      rdr2geo configuration parameters
 * \param[in]  g2r_params      geo2rdr configuration parameters
 * \param[out] height          Height of each pixel in meters above ellipsoid
 * \param[in]  phase_arithmetic Arithmetic used for the delay & carrier phase
 *                              computation in the integration loop
 *
 * \returns Non-zero error code if geometry fails to converge for any pixel,
 *          and the values for these pixels are set to NaN.
 */
isce3::error::ErrorCode
backproject(std::complex<float>* out,
        const isce3::container::RadarGeometry& out_geometry,
        const std::complex<float>* in,
        const isce3::container::RadarGeometry& in_geometry,
        const isce3::geometry::DEMInterpolator& dem, double fc, double ds,
        const isce3::core::Kernel<float>& kernel,
        DryTroposphereModel dry_tropo_model = DryTroposphereModel::TSX,
        const isce3::geometry::detail::Rdr2GeoBracketParams& r2g_params = {},
        const isce3::geometry::detail::Geo2RdrBracketParams& g2r_params = {},
        float* height = nullptr,
        PhaseArithmetic phase_arithmetic = PhaseArithmetic::Double);

} // namespace focus
} // namespace isce3
