#ifndef TDRCALCULATOR_H
#define TDRCALCULATOR_H

#include <QVector>
#include <Eigen/Dense>
#include <complex>

class TDRCalculator
{
public:
    struct Parameters
    {
        constexpr Parameters(double referenceImpedance = 50.0,
                             double effectivePermittivity = 2.9,
                             double speedOfLight = 299792458.0)
            : referenceImpedance(referenceImpedance),
              effectivePermittivity(effectivePermittivity),
              speedOfLight(speedOfLight)
        {
        }

        double referenceImpedance;
        double effectivePermittivity;
        double speedOfLight;
    };

    struct Result
    {
        QVector<double> distance;
        QVector<double> impedance;
    };

    Result compute(const Eigen::ArrayXd& frequencyHz,
                   const Eigen::ArrayXcd& reflection,
                   const Parameters& params = Parameters()) const;
};

#endif // TDRCALCULATOR_H
