#include "tdrcalculator.h"

#include <Eigen/Dense>
#include <QtGlobal>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <complex>
#include <iostream>

namespace
{
constexpr double kPi = 3.14159265358979323846;
}

void test_step_response_has_plateau()
{
    const int sampleCount = 1024;
    const double frequencySpacing = 10e6; // 10 MHz spacing
    const double maxFrequency = frequencySpacing * static_cast<double>(sampleCount - 1);

    Eigen::ArrayXd frequency = Eigen::ArrayXd::LinSpaced(sampleCount, 0.0, maxFrequency);
    Eigen::ArrayXcd reflection(sampleCount);

    const double delay = 10e-9; // 10 ns delay
    const double amplitude = 0.5; // 50 % reflection

    for (int i = 0; i < sampleCount; ++i)
    {
        double phase = -2.0 * kPi * frequency(i) * delay;
        reflection(i) = std::polar(amplitude, phase);
    }

    TDRCalculator calculator;
    TDRCalculator::Parameters params(50.0, 1.0, 299792458.0);
    auto result = calculator.compute(frequency, reflection, params);

    assert(!result.distance.isEmpty());
    assert(result.distance.size() == result.impedance.size());

    double velocity = params.speedOfLight / std::sqrt(params.effectivePermittivity);
    double expectedDistance = 0.5 * velocity * delay;

    int transitionIndex = -1;
    for (int i = 0; i < result.distance.size(); ++i)
    {
        if (result.distance[i] >= expectedDistance)
        {
            transitionIndex = i;
            break;
        }
    }

    assert(transitionIndex >= 0);

    int baselineSamples = std::min(transitionIndex, 20);
    double baselineSum = 0.0;
    for (int i = 0; i < baselineSamples; ++i)
        baselineSum += result.impedance[i];
    double baselineAverage = baselineSamples > 0 ? baselineSum / baselineSamples : params.referenceImpedance;

    double plateauSum = 0.0;
    int plateauSamples = 0;
    for (int i = transitionIndex; i < result.impedance.size() && plateauSamples < 40; ++i)
    {
        plateauSum += result.impedance[i];
        ++plateauSamples;
    }
    double plateauAverage = plateauSamples > 0 ? plateauSum / plateauSamples : params.referenceImpedance;

    std::cout << "Baseline impedance: " << baselineAverage << '\n';
    std::cout << "Plateau impedance: " << plateauAverage << '\n';

    assert(std::abs(baselineAverage - params.referenceImpedance) < 10.0);
    assert((plateauAverage - baselineAverage) > 5.0);
    std::cout << "TDR calculator step response test passed." << std::endl;
}

int main()
{
    test_step_response_has_plateau();
    return 0;
}

