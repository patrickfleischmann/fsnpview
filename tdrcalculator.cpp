#include "tdrcalculator.h"

#include <unsupported/Eigen/FFT>
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace
{
constexpr double kPi = 3.14159265358979323846;

struct Sample
{
    double frequency;
    std::complex<double> reflection;
};

bool isFiniteSample(double frequency, const std::complex<double>& value)
{
    return std::isfinite(frequency) && std::isfinite(value.real()) && std::isfinite(value.imag());
}
}

TDRCalculator::Result TDRCalculator::compute(const Eigen::ArrayXd& frequencyHz,
                                             const Eigen::ArrayXcd& reflection,
                                             const Parameters& params) const
{
    Result result;
    if (frequencyHz.size() == 0 || reflection.size() == 0 || frequencyHz.size() != reflection.size())
        return result;

    // Gather finite samples
    std::vector<Sample> samples;
    samples.reserve(static_cast<std::size_t>(frequencyHz.size()));
    for (Eigen::Index i = 0; i < frequencyHz.size(); ++i)
    {
        double f = frequencyHz(i);
        std::complex<double> r = reflection(i);
        if (!isFiniteSample(f, r))
            continue;
        samples.push_back({f, r});
    }
    if (samples.size() < 2)
        return result;

    // Sort by frequency
    std::sort(samples.begin(), samples.end(),
              [](const Sample& a, const Sample& b){ return a.frequency < b.frequency; });

    // Ensure a DC bin exists (duplicate first sample’s value at 0 Hz if needed)
    if (samples.front().frequency > 0.0)
        samples.insert(samples.begin(), Sample{0.0, samples.front().reflection});

    if (samples.back().frequency <= samples.front().frequency)
        return result;

    const int n = static_cast<int>(samples.size());
    std::vector<double> freq(n);
    std::vector<std::complex<double>> refl(n);
    for (int i = 0; i < n; ++i)
    {
        freq[i] = samples[static_cast<std::size_t>(i)].frequency;
        refl[i] = samples[static_cast<std::size_t>(i)].reflection;
    }

    // Uniform linear re-sampling from fmin..fmax (simple linear interpolation)
    Eigen::ArrayXd  uniformFreq(n);
    Eigen::ArrayXcd uniformReflection(n);

    const double fmin = freq.front();
    const double fmax = freq.back();
    const double denomCount = static_cast<double>(n - 1);

    for (int i = 0; i < n; ++i)
    {
        double alpha = (n == 1) ? 0.0 : static_cast<double>(i) / denomCount;
        double targetFreq = fmin + alpha * (fmax - fmin);
        uniformFreq(i) = targetFreq;

        auto lower = std::lower_bound(freq.begin(), freq.end(), targetFreq);
        if (lower == freq.begin())
        {
            uniformReflection(i) = refl.front();
        }
        else if (lower == freq.end())
        {
            uniformReflection(i) = refl.back();
        }
        else
        {
            int idx = static_cast<int>(std::distance(freq.begin(), lower));
            double f2 = freq[static_cast<std::size_t>(idx)];
            double f1 = freq[static_cast<std::size_t>(idx - 1)];
            std::complex<double> r2 = refl[static_cast<std::size_t>(idx)];
            std::complex<double> r1 = refl[static_cast<std::size_t>(idx - 1)];
            double denom = f2 - f1;
            double t = (std::abs(denom) > std::numeric_limits<double>::epsilon()) ? (targetFreq - f1) / denom : 0.0;
            uniformReflection(i) = r1 + (r2 - r1) * t;
        }
    }

    // Optional low-pass (models source risetime)
    if (params.risetime > 0.0 && params.filter != Parameters::FilterType::None)
    {
        double fc = 0.35 / params.risetime;
        for (int i = 0; i < n; ++i)
        {
            double f = uniformFreq(i);
            double H = 1.0;
            if (params.filter == Parameters::FilterType::Gaussian)
            {
                H = std::exp(-std::pow(f / fc, 2));
            }
            else if (params.filter == Parameters::FilterType::RaisedCosine)
            {
                double roll = std::clamp(params.rolloff, 0.0, 1.0);
                double f0 = (1.0 - roll) * fc;
                double f1 = (1.0 + roll) * fc;
                if (f <= f0)       H = 1.0;
                else if (f >= f1)  H = 0.0;
                else               H = 0.5 * (1.0 + std::cos(kPi * (f - f0) / (2.0 * roll * fc)));
            }
            uniformReflection(i) *= H;
        }
    }

    // Hann taper (keep DC untouched)
    if (n > 1)
    {
        for (int i = 0; i < n; ++i)
        {
            double w = 0.5 * (1.0 - std::cos(2.0 * kPi * static_cast<double>(i) / denomCount));
            if (i == 0) w = 1.0;
            uniformReflection(i) *= w;
        }
    }

    // Build a Hermitian spectrum (length 2*m-1) so impulse is real
    const int m = n;
    const int nTime = 2 * m - 1;
    std::vector<std::complex<double>> spectrum(static_cast<std::size_t>(nTime), std::complex<double>(0.0, 0.0));
    for (int i = 0; i < m; ++i)
        spectrum[static_cast<std::size_t>(i)] = uniformReflection(i);
    for (int i = 1; i < m; ++i)
        spectrum[static_cast<std::size_t>(nTime - i)] = std::conj(uniformReflection(i));

    // IFFT → impulse reflection
    Eigen::FFT<double> fft;
    std::vector<std::complex<double>> timeDomain;
    fft.inv(timeDomain, spectrum);
    if (timeDomain.size() != static_cast<std::size_t>(nTime))
        return result;

    // Time grid
    const double df = (m > 1) ? (uniformFreq(1) - uniformFreq(0)) : 0.0;
    if (!(df > 0.0))
        return result;
    const double dt = 1.0 / (static_cast<double>(nTime) * df);
    if (!(dt > 0.0))
        return result;

    // Use the causal half of the impulse (no circular shifts)
    const int positiveCount = nTime / 2;
    if (positiveCount <= 1)
        return result;

    std::vector<std::complex<double>> gammaImpulse(static_cast<std::size_t>(positiveCount));
    for (int i = 0; i < positiveCount; ++i)
        gammaImpulse[static_cast<std::size_t>(i)] = timeDomain[static_cast<std::size_t>(i)];

    // ---- CRITICAL: Step = cumulative sum WITHOUT multiplying by dt ----
    std::vector<std::complex<double>> gammaStep(static_cast<std::size_t>(positiveCount));
    gammaStep[0] = std::complex<double>(0.0, 0.0);
    std::complex<double> acc(0.0, 0.0);
    for (int i = 1; i < positiveCount; ++i)
    {
        const auto& y0 = gammaImpulse[static_cast<std::size_t>(i - 1)];
        const auto& y1 = gammaImpulse[static_cast<std::size_t>(i)];
        acc += 0.5 * (y0 + y1);           // trapezoid, NO * dt
        gammaStep[static_cast<std::size_t>(i)] = acc;
    }

    // Convert step reflection → impedance
    double effPerm = params.effectivePermittivity;
    if (!(effPerm > 0.0)) effPerm = 1.0;
    const double velocity = params.speedOfLight / std::sqrt(effPerm);

    result.distance.reserve(static_cast<int>(positiveCount));
    result.impedance.reserve(static_cast<int>(positiveCount));

    for (int i = 0; i < positiveCount; ++i)
    {
        double time = dt * static_cast<double>(i);
        double distance = 0.5 * velocity * time;

        const std::complex<double>& g = gammaStep[static_cast<std::size_t>(i)];
        std::complex<double> num = std::complex<double>(1.0, 0.0) + g;
        std::complex<double> den = std::complex<double>(1.0, 0.0) - g;

        double Z;
        if (std::abs(den) < 1e-12)
            Z = std::numeric_limits<double>::quiet_NaN();
        else
            Z = (params.referenceImpedance * num / den).real();

        result.distance.append(distance);
        result.impedance.append(Z);
    }

    return result;
}
