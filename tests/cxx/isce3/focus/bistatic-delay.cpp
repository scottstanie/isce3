#include <cmath>
#include <complex>

#include <gtest/gtest.h>

#include <isce3/core/Constants.h>
#include <isce3/core/DoubleFloat.h>
#include <isce3/core/Vector.h>
#include <isce3/focus/BistaticDelay.h>

using isce3::core::DoubleFloat;
using isce3::core::Vec3;
using isce3::focus::bistaticDelay;
using isce3::focus::bistaticDelayScale;
using isce3::focus::DoubleFloatVec3;

/** Analytical linear orbit with constant velocity */
class LinearOrbit {
public:
    LinearOrbit(const Vec3 & initial_position, const Vec3 & velocity)
        : _initial_position(initial_position), _velocity(velocity) {}

    /** Get position at time t */
    Vec3 position(double t) const { return _initial_position + _velocity * t; }

    /** Get velocity at time t */
    Vec3 velocity(double /*t*/) const { return _velocity; }

private:
    Vec3 _initial_position;
    Vec3 _velocity;
};

TEST(BistaticDelay, BistaticDelay)
{
    // platform orbit
    Vec3 initial_position = {0., 0., 700'000.};
    Vec3 velocity = {0., 8000., 0.};
    LinearOrbit orbit(initial_position, velocity);

    // target position
    Vec3 x = {50'000., 20'000., 0.};

    for (int i = 0; i < 11; ++i) {
        auto t = static_cast<double>(i);

        // compute bistatic delay term (tau)
        Vec3 p = orbit.position(t);
        Vec3 v = orbit.velocity(t);
        double tau = bistaticDelay(p, v, x);

        // compare to roundtrip delay using platform position at time = t + tau
        Vec3 p1 = orbit.position(t + tau);
        double d = (x - p).norm() + (p1 - x).norm();
        double dt = d / isce3::core::speed_of_light;

        EXPECT_DOUBLE_EQ(tau, dt);
    }
}

// The double-float delay must support interferometric-quality carrier phase
// over a full synthetic aperture with NISAR-like L-band geometry. This is
// the precision requirement that rules out naive float32 (which produces
// O(1 rad) phase error) and would otherwise force float64 GPU hardware.
TEST(BistaticDelay, DoubleFloatDelayPhase)
{
    constexpr double c = isce3::core::speed_of_light;
    constexpr double fc = 1.2575e9;         // L-band center frequency (Hz)
    constexpr double GM = 3.986004418e14;   // Earth gravitational parameter
    constexpr double a_orbit = 7.124137e6;  // orbit radius (m)
    constexpr double Re = 6.371e6;          // spherical Earth radius (m)
    constexpr double R0 = 900e3;            // slant range at closest approach
    constexpr double prf = 1650.0;
    constexpr double azimuth_res = 7.0;

    // platform on a circular orbit in the equatorial plane; target on the
    // Earth sphere, positioned for zero Doppler at aperture center
    const double omega = std::sqrt(GM / (a_orbit * a_orbit * a_orbit));
    const double vs = a_orbit * omega;
    const double cosb =
            (Re * Re + a_orbit * a_orbit - R0 * R0) / (2. * a_orbit * Re);
    const Vec3 x = {Re * cosb, 0., Re * std::sqrt(1. - cosb * cosb)};
    const DoubleFloatVec3 x_df(x);
    const DoubleFloat fc_df(fc);

    // full synthetic aperture for the desired azimuth resolution
    const double wvl = c / fc;
    const double cpi = wvl * R0 / (2. * azimuth_res) / vs;
    const int n_pulses = static_cast<int>(cpi * prf);

    double max_phase_err = 0.;
    double max_delay_err = 0.;
    std::complex<double> coherent_sum(0., 0.);

    for (int k = 0; k < n_pulses; ++k) {
        const double t = (k - 0.5 * n_pulses) / prf;
        const double ct = std::cos(omega * t), st = std::sin(omega * t);
        const Vec3 p = {a_orbit * ct, a_orbit * st, 0.};
        const Vec3 v = {-vs * st, vs * ct, 0.};

        const double tau = bistaticDelay(p, v, x);
        const DoubleFloat tau_df = bistaticDelay(
                DoubleFloatVec3(p), DoubleFloatVec3(v), x_df,
                bistaticDelayScale(v));

        max_delay_err = std::max(
                max_delay_err, std::abs(static_cast<double>(tau_df) - tau));

        // carrier phase error, wrapped (whole cycles are unobservable)
        const double cycles_err = fc * tau - static_cast<double>(fc_df * tau_df);
        const double phase_err =
                2. * M_PI * (cycles_err - std::round(cycles_err));
        max_phase_err = std::max(max_phase_err, std::abs(phase_err));
        coherent_sum += std::exp(std::complex<double>(0., phase_err));
    }

    // ~5e6 pulses/s * 2 s aperture
    ASSERT_GT(n_pulses, 3000);

    // delay accurate to ~1e-14 relative: micrometers in range units
    EXPECT_LT(max_delay_err * c / 2., 1e-6);

    // phase accurate to microradians (measured ~2e-6 rad; the requirement
    // for interferometry is ~1e-3 rad)
    EXPECT_LT(max_phase_err, 1e-4);

    // focused peak amplitude loss must be negligible
    EXPECT_GT(std::abs(coherent_sum) / n_pulses, 0.999999);
}

int main(int argc, char * argv[])
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
