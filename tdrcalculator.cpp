#include "tdrcalculator.h"

#include <unsupported/Eigen/FFT>
#include <algorithm>
#include <climits>
#include <cmath>
#include <complex>
#include <limits>
#include <numeric>
#include <optional>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kC0 = 299792458.0;

struct TransformContext
{
    Eigen::ArrayXd freqSorted;
    Eigen::ArrayXcd reflectionSorted;
    std::vector<int> permutation; // sorted index -> original index
    Eigen::ArrayXd freqBins;
    Eigen::ArrayXcd spectrumPositive;
    std::vector<std::complex<double>> spectrumFull;
    Eigen::ArrayXd impulse;
    double df = 0.0;
    std::size_t Nfft = 0;
    double dt = 0.0;
    double velocity = 0.0;
};

inline double safeEpsilon(double eps)
{
    return (eps > 1.0) ? eps : 1.0;
}

// Next power of two >= n
inline std::size_t NextPow2(std::size_t n)
{
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
inline void ApplyHighEndCosineTaper(Eigen::ArrayXcd& oneSided, int edgeCount)
{
    const int n = static_cast<int>(oneSided.size());
    if (n <= 2 || edgeCount <= 0 || edgeCount >= n) return;

    for (int i = n - edgeCount; i < n; ++i) {
        const double x = double(i - (n - edgeCount)) / double(edgeCount - 1); // 0..1
        const double w = 0.5 * (1.0 + std::cos(kPi * x)); // 1 → 0
        oneSided(i) *= w;
    }
}

std::optional<TransformContext> PrepareTransform(const Eigen::ArrayXd& frequencyHz,
                                                 const Eigen::ArrayXcd& reflection,
                                                 const TDRCalculator::Parameters& params)
{
    TransformContext ctx;

    const Eigen::Index m = std::min(frequencyHz.size(), reflection.size());
    if (m < 4) {
        qDebug("Basic validation failed");
        return std::nullopt;
    }
    qDebug("Basic validation ok");

    Eigen::ArrayXd f = frequencyHz.head(m);
    Eigen::ArrayXcd s11 = reflection.head(m);
    std::vector<int> idx(static_cast<std::size_t>(m));
    std::iota(idx.begin(), idx.end(), 0);
    std::stable_sort(idx.begin(), idx.end(),
                     [&](int a, int b) { return f(a) < f(b); });

    ctx.freqSorted.resize(m);
    ctx.reflectionSorted.resize(m);
    ctx.permutation.resize(static_cast<std::size_t>(m));

    for (Eigen::Index i = 0; i < m; ++i) {
        const int originalIndex = idx[static_cast<std::size_t>(i)];
        ctx.freqSorted(i) = f(originalIndex);
        ctx.reflectionSorted(i) = s11(originalIndex);
        ctx.permutation[static_cast<std::size_t>(i)] = originalIndex;
    }

    Eigen::ArrayXd df = ctx.freqSorted.tail(m - 1) - ctx.freqSorted.head(m - 1);
    ctx.df = df.mean();
    if (!(ctx.df > 0.0)) {
        qDebug("error: df_est <= 0");
        return std::nullopt;
    }
    qDebug("ok df_est > 0");

    const std::size_t minimalNfft = static_cast<std::size_t>(2 * (m - 1));
    ctx.Nfft = NextPow2(minimalNfft);
    ctx.Nfft = std::max<std::size_t>(ctx.Nfft, (1u << 17));
    const std::size_t nBins = ctx.Nfft / 2 + 1;

    ctx.freqBins.resize(static_cast<int>(nBins));
    for (std::size_t i = 0; i < nBins; ++i)
        ctx.freqBins(static_cast<Eigen::Index>(i)) = ctx.df * double(i);

    ctx.spectrumPositive.resize(static_cast<int>(nBins));
    const double fmaxMeas = ctx.freqSorted(m - 1);
    for (std::size_t i = 0; i < nBins; ++i) {
        const double fb = ctx.freqBins(static_cast<Eigen::Index>(i));
        if (fb > fmaxMeas) {
            ctx.spectrumPositive(static_cast<Eigen::Index>(i)) = std::complex<double>(0.0, 0.0);
        } else {
            auto it = std::lower_bound(ctx.freqSorted.data(), ctx.freqSorted.data() + m, fb);
            if (it == ctx.freqSorted.data()) {
                ctx.spectrumPositive(static_cast<Eigen::Index>(i)) = ctx.reflectionSorted(0);
            } else if (it == ctx.freqSorted.data() + m) {
                ctx.spectrumPositive(static_cast<Eigen::Index>(i)) = ctx.reflectionSorted(m - 1);
            } else {
                const Eigen::Index hi = static_cast<Eigen::Index>(it - ctx.freqSorted.data());
                const Eigen::Index lo = hi - 1;
                const double f0 = ctx.freqSorted(lo);
                const double f1 = ctx.freqSorted(hi);
                const double t = (fb - f0) / (f1 - f0 + 1e-30);
                const std::complex<double> y0 = ctx.reflectionSorted(lo);
                const std::complex<double> y1 = ctx.reflectionSorted(hi);
                ctx.spectrumPositive(static_cast<Eigen::Index>(i)) = y0 + (y1 - y0) * t;
            }
        }
    }

    if (params.risetime > 0.0 && params.filter != TDRCalculator::Parameters::FilterType::None) {
        const double fc = 0.35 / params.risetime;
        for (std::size_t i = 0; i < nBins; ++i) {
            const double fcur = ctx.freqBins(static_cast<Eigen::Index>(i));
            double H = 1.0;
            if (params.filter == TDRCalculator::Parameters::FilterType::Gaussian) {
                H = std::exp(-std::pow(fcur / fc, 2.0));
            } else if (params.filter == TDRCalculator::Parameters::FilterType::RaisedCosine) {
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
            ctx.spectrumPositive(static_cast<Eigen::Index>(i)) *= H;
        }
    }

    {
        const int edge = std::max<int>(1, int(0.10 * (nBins - 1)));
        ApplyHighEndCosineTaper(ctx.spectrumPositive, edge);
    }

    ctx.spectrumFull.assign(ctx.Nfft, std::complex<double>(0.0, 0.0));
    for (std::size_t i = 0; i < nBins; ++i)
        ctx.spectrumFull[i] = ctx.spectrumPositive(static_cast<Eigen::Index>(i));
    for (std::size_t i = 1; i < nBins; ++i)
        ctx.spectrumFull[ctx.Nfft - i] = std::conj(ctx.spectrumPositive(static_cast<Eigen::Index>(i)));

    Eigen::FFT<double> fft;
    std::vector<std::complex<double>> timeCmplx(ctx.Nfft);
    fft.inv(timeCmplx, ctx.spectrumFull);

    ctx.impulse.resize(static_cast<int>(ctx.Nfft));
    for (std::size_t i = 0; i < ctx.Nfft; ++i)
        ctx.impulse(static_cast<Eigen::Index>(i)) = timeCmplx[i].real();

    const double fmax = ctx.df * double(nBins - 1);
    const double fs = 2.0 * fmax;
    ctx.dt = (fs > 0.0) ? (1.0 / fs) : 0.0;
    const double erEff = safeEpsilon(params.effectivePermittivity);
    ctx.velocity = kC0 / std::sqrt(erEff);

    return ctx;
}

Eigen::ArrayXd ComputeStepResponse(const Eigen::ArrayXd& impulse)
{
    Eigen::ArrayXd rho(impulse.size());
    double acc = 0.0;
    for (Eigen::Index i = 0; i < impulse.size(); ++i) {
        acc += impulse(i);
        rho(i) = acc;
    }
    return rho;
}

void BaselineCorrect(Eigen::ArrayXd& rho)
{
    const std::size_t count = static_cast<std::size_t>(rho.size());
    const std::size_t k = std::min<std::size_t>(count, 64);
    double base = 0.0;
    for (std::size_t i = 0; i < k; ++i)
        base += rho(static_cast<Eigen::Index>(i));
    base /= double(k);
    for (std::size_t i = 0; i < count; ++i)
        rho(static_cast<Eigen::Index>(i)) -= base;
}

void ClampStep(Eigen::ArrayXd& rho)
{
    for (Eigen::Index i = 0; i < rho.size(); ++i) {
        if (rho(i) > 0.999) rho(i) = 0.999;
        if (rho(i) < -0.999) rho(i) = -0.999;
    }
}

Eigen::ArrayXd StepToImpedance(const Eigen::ArrayXd& rho, double referenceImpedance)
{
    Eigen::ArrayXd Z(rho.size());
    for (Eigen::Index i = 0; i < rho.size(); ++i) {
        const double g = rho(i);
        const double den = 1.0 - g;
        if (std::abs(den) < 1e-14) {
            Z(i) = std::numeric_limits<double>::quiet_NaN();
        } else {
            Z(i) = referenceImpedance * (1.0 + g) / den;
        }
    }
    return Z;
}

QVector<double> DistanceVector(std::size_t Nfft, double dt, double velocity)
{
    QVector<double> distance(static_cast<int>(Nfft));
    for (std::size_t i = 0; i < Nfft; ++i) {
        const double t = dt * double(i);
        distance[static_cast<int>(i)] = 0.5 * velocity * t;
    }
    return distance;
}

Eigen::ArrayXcd MapSpectrumToOriginal(const TransformContext& ctx,
                                      const Eigen::ArrayXcd& positiveSpectrum)
{
    const Eigen::Index m = ctx.freqSorted.size();
    Eigen::ArrayXcd sortedValues(m);
    const double df = ctx.df;
    if (!(df > 0.0) || positiveSpectrum.size() == 0)
        return Eigen::ArrayXcd::Zero(m);

    const Eigen::Index lastIndex = positiveSpectrum.size() - 1;
    for (Eigen::Index i = 0; i < m; ++i) {
        const double f = ctx.freqSorted(i);
        double pos = f / df;
        if (pos <= 0.0) {
            sortedValues(i) = positiveSpectrum(0);
            continue;
        }
        const double floorIndex = std::floor(pos);
        Eigen::Index idx0 = static_cast<Eigen::Index>(floorIndex);
        Eigen::Index idx1 = idx0 + 1;
        double frac = pos - floorIndex;
        if (idx0 < 0) {
            sortedValues(i) = positiveSpectrum(0);
        } else if (idx0 >= lastIndex) {
            sortedValues(i) = positiveSpectrum(lastIndex);
        } else {
            if (idx1 > lastIndex) {
                idx1 = lastIndex;
                frac = 0.0;
            }
            sortedValues(i) = (1.0 - frac) * positiveSpectrum(idx0) + frac * positiveSpectrum(idx1);
        }
    }

    Eigen::ArrayXcd result(m);
    for (Eigen::Index i = 0; i < m; ++i) {
        const int originalIndex = ctx.permutation[static_cast<std::size_t>(i)];
        result(originalIndex) = sortedValues(i);
    }
    return result;
}

Eigen::ArrayXd GateWindow(std::size_t size, double dt, double velocity,
                          double startDistance, double stopDistance)
{
    Eigen::ArrayXd window = Eigen::ArrayXd::Zero(static_cast<Eigen::Index>(size));
    if (size == 0 || !(dt > 0.0) || !(velocity > 0.0))
        return window;

    const double startDist = std::max(0.0, startDistance);
    const double stopDist = std::max(startDist, stopDistance);

    const double startTime = 2.0 * startDist / velocity;
    const double stopTime = 2.0 * stopDist / velocity;

    int startIndex = static_cast<int>(std::floor(startTime / dt));
    int stopIndex = static_cast<int>(std::ceil(stopTime / dt));

    const int last = static_cast<int>(size) - 1;
    if (startIndex > last) startIndex = last;
    if (stopIndex > last) stopIndex = last;
    if (startIndex < 0) startIndex = 0;
    if (stopIndex < startIndex) stopIndex = startIndex;

    if (startIndex == 0 && stopIndex == last) {
        window.setOnes();
        return window;
    }

    const int width = stopIndex - startIndex + 1;
    if (width <= 1) {
        window(startIndex) = 1.0;
        return window;
    }

    const double alpha = 0.3;
    const double usedAlpha = std::clamp(alpha, 0.0, 1.0);
    const double boundary = usedAlpha / 2.0;
    const double denom = double(width - 1);

    for (int n = 0; n < width; ++n) {
        double ratio = (denom > 0.0) ? (double(n) / denom) : 0.0;
        double weight = 1.0;
        if (usedAlpha > 0.0) {
            if (ratio < boundary) {
                weight = 0.5 * (1.0 + std::cos(kPi * ((2.0 * ratio / usedAlpha) - 1.0)));
            } else if (ratio > 1.0 - boundary) {
                weight = 0.5 * (1.0 + std::cos(kPi * ((2.0 * ratio / usedAlpha) - (2.0 / usedAlpha) + 1.0)));
            }
        }
        window(startIndex + n) = weight;
    }

    return window;
}

} // namespace

TDRCalculator::Result TDRCalculator::compute(const Eigen::ArrayXd& frequencyHz,
                                             const Eigen::ArrayXcd& reflection,
                                             const Parameters& params) const
{
    Result result;

    auto ctxOpt = PrepareTransform(frequencyHz, reflection, params);
    if (!ctxOpt)
        return result;

    TransformContext& ctx = *ctxOpt;

    Eigen::ArrayXd rho = ComputeStepResponse(ctx.impulse);
    BaselineCorrect(rho);
    ClampStep(rho);
    Eigen::ArrayXd impedance = StepToImpedance(rho, params.referenceImpedance);

    result.distance = DistanceVector(ctx.Nfft, ctx.dt, ctx.velocity);
    result.impedance = QVector<double>(impedance.data(), impedance.data() + impedance.size());

    return result;
}

std::optional<TDRCalculator::GateResult> TDRCalculator::applyGate(const Eigen::ArrayXd& frequencyHz,
                                                                  const Eigen::ArrayXcd& reflection,
                                                                  double gateStartDistance,
                                                                  double gateStopDistance,
                                                                  double epsilonR,
                                                                  const Parameters& params) const
{
    Parameters gateParams = params;
    gateParams.effectivePermittivity = safeEpsilon(epsilonR);

    auto ctxOpt = PrepareTransform(frequencyHz, reflection, gateParams);
    if (!ctxOpt)
        return std::nullopt;

    TransformContext ctx = *ctxOpt;

    Eigen::ArrayXd window = GateWindow(ctx.Nfft, ctx.dt, ctx.velocity,
                                       gateStartDistance, gateStopDistance);
    if (window.size() != ctx.impulse.size()) {
        window.conservativeResize(ctx.impulse.size());
    }

    Eigen::ArrayXd gatedImpulse = ctx.impulse * window;

    Eigen::ArrayXd rho = ComputeStepResponse(gatedImpulse);
    BaselineCorrect(rho);
    ClampStep(rho);
    Eigen::ArrayXd impedance = StepToImpedance(rho, gateParams.referenceImpedance);

    Eigen::FFT<double> fft;
    std::vector<std::complex<double>> specFull(ctx.Nfft);
    fft.fwd(specFull, gatedImpulse);

    Eigen::ArrayXcd positive(ctx.freqBins.size());
    for (Eigen::Index i = 0; i < positive.size(); ++i)
        positive(i) = specFull[static_cast<std::size_t>(i)];

    GateResult result;
    result.gatedReflection = MapSpectrumToOriginal(ctx, positive);
    result.distance = DistanceVector(ctx.Nfft, ctx.dt, ctx.velocity);
    result.impedance = QVector<double>(impedance.data(), impedance.data() + impedance.size());

    return result;
}
