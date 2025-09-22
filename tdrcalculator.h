#ifndef TDRCALCULATOR_H
#define TDRCALCULATOR_H

#include <QVector>
#include <Eigen/Dense>
#include <complex>
#include <optional>

class TDRCalculator
{
public:

    struct Parameters
    {
        enum class FilterType { None, Gaussian, RaisedCosine };
        constexpr Parameters(double referenceImpedance = 50.0,
                             double effectivePermittivity = 2.9,
                             double speedOfLight = 299792458.0,
                             double risetime = 5e-12,
                             enum FilterType filter = Parameters::FilterType::Gaussian,
                             double rolloff = 0.5)
            : referenceImpedance(referenceImpedance),
              effectivePermittivity(effectivePermittivity),
              speedOfLight(speedOfLight),
              risetime(risetime),
              filter(filter),
              rolloff(rolloff)
        {
        }

        double referenceImpedance;
        double effectivePermittivity;
        double speedOfLight;

        double risetime;   // source risetime [s] (0 = ideal step)
        enum FilterType filter;
        double rolloff;    // only used for RaisedCosine, range 0..1
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
