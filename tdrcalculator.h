#ifndef TDRCALCULATOR_H
#define TDRCALCULATOR_H

#include <QVector>
#include <Eigen/Dense>
#include <complex>
#include <optional>

class TDRCalculator
{
public:
    enum class WindowType { Square, Hann, Blackman };

    struct Parameters
    {
        constexpr Parameters(double referenceImpedance = 50.0,
                             double effectivePermittivity = 2.9,
                             double speedOfLight = 299792458.0,
                             WindowType window = WindowType::Hann)
            : referenceImpedance(referenceImpedance),
              effectivePermittivity(effectivePermittivity),
              speedOfLight(speedOfLight),
              window(window)
        {
        }

        double referenceImpedance;
        double effectivePermittivity;
        double speedOfLight;

        WindowType window;
    };

    struct Result
    {
        QVector<double> distance;
        QVector<double> impedance;
    };

    struct GateResult
    {
        Eigen::ArrayXcd gatedReflection;
        QVector<double> distance;
        QVector<double> impedance;
    };

    Result compute(const Eigen::ArrayXd& frequencyHz,
                   const Eigen::ArrayXcd& reflection,
                   const Parameters& params = Parameters()) const;

    std::optional<GateResult> applyGate(const Eigen::ArrayXd& frequencyHz,
                                        const Eigen::ArrayXcd& reflection,
                                        double gateStartDistance,
                                        double gateStopDistance,
                                        double epsilonR,
                                        const Parameters& params = Parameters()) const;
};

#endif // TDRCALCULATOR_H
