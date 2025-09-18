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

    std::vector<Sample> samples;
    samples.reserve(static_cast<std::size_t>(frequencyHz.size()));
    for (Eigen::Index i = 0; i < frequencyHz.size(); ++i)
    {
        double freq = frequencyHz(i);
        std::complex<double> value = reflection(i);
        if (!isFiniteSample(freq, value))
            continue;
        samples.push_back({freq, value});
    }

    if (samples.size() < 2)
        return result;

    std::sort(samples.begin(), samples.end(), [](const Sample& a, const Sample& b) {
        return a.frequency < b.frequency;
    });

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

    Eigen::ArrayXd uniformFreq(n);
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

    if (n > 1)
    {
        for (int i = 0; i < n; ++i)
        {
            double w = 0.5 * (1.0 - std::cos(2.0 * kPi * static_cast<double>(i) / denomCount));
            if (i == 0)
                w = 1.0;
            uniformReflection(i) *= w;
        }
    }

    if (n < 2)
        return result;

    const int m = n;
    const int nTime = 2 * m - 1;
    std::vector<std::complex<double>> spectrum(static_cast<std::size_t>(nTime), std::complex<double>(0.0, 0.0));
    for (int i = 0; i < m; ++i)
        spectrum[static_cast<std::size_t>(i)] = uniformReflection(i);
    for (int i = 1; i < m; ++i)
        spectrum[static_cast<std::size_t>(nTime - i)] = std::conj(uniformReflection(i));

    Eigen::FFT<double> fft;
    std::vector<std::complex<double>> timeDomain;
    fft.inv(timeDomain, spectrum);

    if (timeDomain.size() != static_cast<std::size_t>(nTime))
        return result;

    const double df = (m > 1) ? (uniformFreq(1) - uniformFreq(0)) : 0.0;
    if (df <= 0.0)
        return result;

    const double dt = 1.0 / (static_cast<double>(nTime) * df);
    if (!(dt > 0.0))
        return result;

    std::vector<std::complex<double>> shifted(static_cast<std::size_t>(nTime));
    const int shift = nTime / 2;
    for (int i = 0; i < nTime; ++i)
        shifted[static_cast<std::size_t>(i)] = timeDomain[static_cast<std::size_t>((i + shift) % nTime)];

    const int positiveCount = nTime - shift;
    if (positiveCount <= 0)
        return result;

    std::vector<std::complex<double>> stepResponse(static_cast<std::size_t>(positiveCount));
    stepResponse[0] = std::complex<double>(0.0, 0.0);
    std::complex<double> cumulative(0.0, 0.0);
    for (int i = 1; i < positiveCount; ++i)
    {
        const std::complex<double>& y0 = shifted[static_cast<std::size_t>(shift + i - 1)];
        const std::complex<double>& y1 = shifted[static_cast<std::size_t>(shift + i)];
        cumulative += 0.5 * (y0 + y1) * dt;
        stepResponse[static_cast<std::size_t>(i)] = cumulative;
    }

    const std::complex<double> dcReflection = uniformReflection(0);
    const std::complex<double>& finalValue = stepResponse.back();
    if (std::abs(finalValue) > std::numeric_limits<double>::epsilon())
    {
        const std::complex<double> scale = dcReflection / finalValue;
        for (std::complex<double>& value : stepResponse)
            value *= scale;
    }
    double effPerm = params.effectivePermittivity;
    if (!(effPerm > 0.0))
        effPerm = 1.0;
    const double velocity = params.speedOfLight / std::sqrt(effPerm);

    result.distance.reserve(static_cast<int>(positiveCount));
    result.impedance.reserve(static_cast<int>(positiveCount));

    for (int i = 0; i < positiveCount; ++i)
    {
        double time = dt * static_cast<double>(i);
        double distance = 0.5 * velocity * time;
        const std::complex<double>& gamma = stepResponse[static_cast<std::size_t>(i)];
        std::complex<double> numerator = std::complex<double>(1.0, 0.0) + gamma;
        std::complex<double> denominator = std::complex<double>(1.0, 0.0) - gamma;
        double impedanceValue;
        if (std::abs(denominator) < 1e-12)
        {
            impedanceValue = std::numeric_limits<double>::quiet_NaN();
        }
        else
        {
            std::complex<double> impedance = params.referenceImpedance * numerator / denominator;
            impedanceValue = impedance.real();
        }
        result.distance.append(distance);
        result.impedance.append(impedanceValue);
    }

    return result;
}
