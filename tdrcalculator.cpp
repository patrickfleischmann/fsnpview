#include "tdrcalculator.h"

#include <unsupported/Eigen/FFT>
#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#include <numeric>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kC0 = 299792458.0;

// Next power of two >= n
inline std::size_t NextPow2(std::size_t n) {
    if (n == 0) return 1;
    --n;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
#if ULONG_MAX > 0xffffffffUL
    n |= n >> 32;
#endif
    return n + 1;
}

// Apply a cosine taper only at the high-frequency tail
inline void ApplyHighEndCosineTaper(Eigen::ArrayXcd& oneSided, int edgeCount) {
    const int n = static_cast<int>(oneSided.size());
    if (n <= 2 || edgeCount <= 0 || edgeCount >= n) return;

    for (int i = n - edgeCount; i < n; ++i) {
        const double x = double(i - (n - edgeCount)) / double(edgeCount - 1); // 0..1
        const double w = 0.5 * (1.0 + std::cos(kPi * x)); // 1 → 0
        oneSided(i) *= w;
    }
}

} // namespace

TDRCalculator::Result TDRCalculator::compute(const Eigen::ArrayXd& frequencyHz,
                                             const Eigen::ArrayXcd& reflection,
                                             const Parameters& params) const
{
    Result result;

    // Basic validation
    const Eigen::Index m = std::min(frequencyHz.size(), reflection.size());
    if (m < 4) {
        return result;
    }

    // Copy + sort by frequency
    Eigen::ArrayXd f = frequencyHz.head(m);
    Eigen::ArrayXcd s11 = reflection.head(m);
    std::vector<int> idx(m);
    std::iota(idx.begin(), idx.end(), 0);
    std::stable_sort(idx.begin(), idx.end(),
                     [&](int a, int b){ return f(a) < f(b); });

    Eigen::ArrayXd f_sorted(m);
    Eigen::ArrayXcd s11_sorted(m);
    for (Eigen::Index i = 0; i < m; ++i) {
        f_sorted(i) = f(idx[i]);
        s11_sorted(i) = s11(idx[i]);
    }

    // Estimate df
    Eigen::ArrayXd df = f_sorted.tail(m - 1) - f_sorted.head(m - 1);
    const double df_est = df.mean();
    if (!(df_est > 0.0)) return result;

    // Nfft size
    const std::size_t minimalNfft = static_cast<std::size_t>(2 * (m - 1));
    std::size_t Nfft = NextPow2(minimalNfft);
    Nfft = std::max<std::size_t>(Nfft, (1u << 17));
    const std::size_t nBins = Nfft / 2 + 1;

    // Interpolate onto uniform bins
    Eigen::ArrayXd f_bins(nBins);
    for (std::size_t i = 0; i < nBins; ++i) f_bins(i) = df_est * double(i);

    Eigen::ArrayXcd S11_bins(nBins);
    const double fmax_meas = f_sorted(m - 1);
    for (std::size_t i = 0; i < nBins; ++i) {
        const double fb = f_bins(i);
        if (fb > fmax_meas) {
            S11_bins(i) = std::complex<double>(0.0, 0.0);
        } else {
            auto it = std::lower_bound(f_sorted.data(), f_sorted.data() + m, fb);
            if (it == f_sorted.data()) {
                S11_bins(i) = s11_sorted(0);
            } else if (it == f_sorted.data() + m) {
                S11_bins(i) = s11_sorted(m - 1);
            } else {
                const Eigen::Index hi = static_cast<Eigen::Index>(it - f_sorted.data());
                const Eigen::Index lo = hi - 1;
                const double f0 = f_sorted(lo), f1 = f_sorted(hi);
                const double t  = (fb - f0) / (f1 - f0 + 1e-30);
                const std::complex<double> y0 = s11_sorted(lo);
                const std::complex<double> y1 = s11_sorted(hi);
                S11_bins(i) = y0 + (y1 - y0) * t;
            }
        }
    }

    // === Optional filter (safe) ===
    if (params.risetime > 0.0 && params.filter != Parameters::FilterType::None) {
        const double fc = 0.35 / params.risetime;
        for (std::size_t i = 0; i < nBins; ++i) {
            const double fcur = f_bins(i);
            double H = 1.0;
            if (params.filter == Parameters::FilterType::Gaussian) {
                H = std::exp(-std::pow(fcur / fc, 2.0));
            } else if (params.filter == Parameters::FilterType::RaisedCosine) {
                const double roll = std::clamp(params.rolloff, 0.0, 1.0);
                const double f0 = (1.0 - roll) * fc;
                const double f1 = (1.0 + roll) * fc;
                if (fcur <= f0) {
                    H = 1.0;
                } else if (fcur >= f1) {
                    H = 0.0;
                } else {
                    H = 0.5 * (1.0 + std::cos(kPi * (fcur - f0) / (2.0 * roll * fc)));
                }
            }
            S11_bins(i) *= H;
        }
    }

    // === High-end cosine taper ===
    {
        const int edge = std::max<int>(1, int(0.10 * (nBins - 1)));
        ApplyHighEndCosineTaper(S11_bins, edge);
    }

    // Build Hermitian spectrum
    std::vector<std::complex<double>> specFull(Nfft, {0.0, 0.0});
    for (std::size_t i = 0; i < nBins; ++i) {
        specFull[i] = S11_bins(i);
    }
    for (std::size_t i = 1; i < nBins; ++i) {
        specFull[Nfft - i] = std::conj(S11_bins(i));
    }

    // IFFT → impulse response
    Eigen::FFT<double> fft;
    std::vector<std::complex<double>> timeCmplx(Nfft);
    fft.inv(timeCmplx, specFull);

    Eigen::ArrayXd h(Nfft);
    for (std::size_t i = 0; i < Nfft; ++i) {
        h(i) = timeCmplx[i].real();
    }

    // Step response = cumulative sum
    Eigen::ArrayXd rho(Nfft);
    double acc = 0.0;
    for (std::size_t i = 0; i < Nfft; ++i) {
        acc += h(i);
        rho(i) = acc;
    }

    // Baseline correction
    {
        const std::size_t k = std::min<std::size_t>(Nfft, 64);
        double base = 0.0;
        for (std::size_t i = 0; i < k; ++i) base += rho(i);
        base /= double(k);
        for (std::size_t i = 0; i < Nfft; ++i) rho(i) -= base;
    }

    // Clamp
    for (std::size_t i = 0; i < Nfft; ++i) {
        if (rho(i) > 0.999) rho(i) = 0.999;
        if (rho(i) < -0.999) rho(i) = -0.999;
    }

    // Convert to impedance
    const double Z0 = params.referenceImpedance;
    Eigen::ArrayXd Z(Nfft);
    for (std::size_t i = 0; i < Nfft; ++i) {
        const double g = rho(i);
        const double den = 1.0 - g;
        if (std::abs(den) < 1e-14) {
            Z(i) = std::numeric_limits<double>::quiet_NaN();
        } else {
            Z(i) = Z0 * (1.0 + g) / den;
        }
    }

    // Time and distance axes
    const double fmax = df_est * double(nBins - 1);
    const double fs   = 2.0 * fmax;
    const double dt   = (fs > 0.0) ? (1.0 / fs) : 0.0;

    const double er_eff = std::max(params.effectivePermittivity, 1.0);
    const double v = kC0 / std::sqrt(er_eff);

    result.distance.reserve(static_cast<int>(Nfft));
    result.impedance.reserve(static_cast<int>(Nfft));
    for (std::size_t i = 0; i < Nfft; ++i) {
        const double t  = dt * double(i);
        const double d  = 0.5 * v * t; // round-trip
        result.distance.append(d);
        result.impedance.append(Z(i));
    }

    return result;
}
