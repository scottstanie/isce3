#include <cmath>
#include <random>

#include <gtest/gtest.h>

#include <isce3/core/DoubleFloat.h>

using isce3::core::DoubleFloat;
using isce3::core::roundedRemainder;

// Effective double-float precision is ~2^-48; allow a few ulps of slack for
// accumulated rounding in compound operations.
constexpr double kRelTol = 1e-13;

TEST(DoubleFloat, SplitRoundTrip)
{
    std::mt19937 rng(12345);
    std::uniform_real_distribution<double> mantissa(-1.0, 1.0);
    std::uniform_int_distribution<int> exponent(-20, 20);

    for (int i = 0; i < 1000; ++i) {
        const double d = std::ldexp(mantissa(rng), exponent(rng));
        const auto df = DoubleFloat(d);
        // hi is d rounded to float; lo captures the remainder
        EXPECT_EQ(df.hi, static_cast<float>(d));
        EXPECT_NEAR(static_cast<double>(df), d, std::abs(d) * kRelTol);
    }
}

TEST(DoubleFloat, AddMul)
{
    std::mt19937 rng(54321);
    std::uniform_real_distribution<double> uniform(0.1, 10.0);
    std::uniform_int_distribution<int> exponent(-10, 10);

    for (int i = 0; i < 1000; ++i) {
        const double a = std::ldexp(uniform(rng), exponent(rng));
        const double b = std::ldexp(uniform(rng), exponent(rng));
        const auto A = DoubleFloat(a);
        const auto B = DoubleFloat(b);

        EXPECT_NEAR(static_cast<double>(A + B), a + b, (a + b) * kRelTol);
        EXPECT_NEAR(static_cast<double>(A - B), a - b,
                    std::abs(a - b) * kRelTol + (a + b) * 1e-15);
        EXPECT_NEAR(static_cast<double>(A * B), a * b, a * b * kRelTol);
    }
}

TEST(DoubleFloat, Sqrt)
{
    std::mt19937 rng(2468);
    std::uniform_real_distribution<double> uniform(0.1, 10.0);
    std::uniform_int_distribution<int> exponent(-10, 10);

    for (int i = 0; i < 1000; ++i) {
        const double a = std::ldexp(uniform(rng), exponent(rng));
        const double s = std::sqrt(a);
        EXPECT_NEAR(static_cast<double>(sqrt(DoubleFloat(a))), s, s * kRelTol);
    }
    EXPECT_EQ(static_cast<double>(sqrt(DoubleFloat(0.0f))), 0.0);
}

TEST(DoubleFloat, RoundedRemainder)
{
    // Wrapping millions of whole cycles must preserve the fractional part to
    // double-float precision: ~2^-48 relative to the total, i.e. ~3e-8 cycles
    // absolute for values near 1e7.
    std::mt19937 rng(1357);
    std::uniform_int_distribution<int> whole(1, 15'000'000);
    std::uniform_real_distribution<double> part(-0.5, 0.5);

    for (int i = 0; i < 1000; ++i) {
        const double f = part(rng);
        const double cycles = whole(rng) + f;
        // the result may differ from f by a whole cycle, which is invisible
        // to sin/cos of the phase - compare modulo 1
        const double diff = roundedRemainder(DoubleFloat(cycles)) - f;
        EXPECT_NEAR(diff - std::round(diff), 0.0, 1e-6);
    }
}

int main(int argc, char* argv[])
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
