#include "networklumped.h"
#include <complex>

namespace {
    double unit_to_multiplier(const QString& unit) {
        if (unit == "f") return 1e-15;
        if (unit == "p") return 1e-12;
        if (unit == "n") return 1e-9;
        if (unit == "u") return 1e-6;
        if (unit == "m") return 1e-3;
        if (unit == "k") return 1e3;
        if (unit == "M") return 1e6;
        if (unit == "G") return 1e9;
        return 1.0;
    }
}

NetworkLumped::NetworkLumped(NetworkType type, double value, const QString& unit, QObject *parent)
    : Network(parent), m_type(type), m_value(value), m_unit(unit)
{
    m_fmin = 1e6;
    m_fmax = 10e9;
    m_is_visible = false;
}

QString NetworkLumped::name() const
{
    switch (m_type) {
    case NetworkType::R_series: return "R_series";
    case NetworkType::R_shunt:  return "R_shunt";
    case NetworkType::C_series: return "C_series";
    case NetworkType::C_shunt:  return "C_shunt";
    case NetworkType::L_series: return "L_series";
    case NetworkType::L_shunt:  return "L_shunt";
    }
    return "";
}

double NetworkLumped::value() const {
    return m_value;
}

void NetworkLumped::setValue(double value) {
    if (m_value != value) {
        m_value = value;
        emit valueChanged();
    }
}

QString NetworkLumped::unit() const {
    return m_unit;
}

void NetworkLumped::setUnit(const QString &unit) {
    if (m_unit != unit) {
        m_unit = unit;
        emit valueChanged();
    }
}

Eigen::MatrixXcd NetworkLumped::abcd(const Eigen::VectorXd& freq) const
{
    Eigen::MatrixXcd abcd_matrix(freq.size(), 4);
    std::complex<double> j(0, 1);
    double val = m_value * unit_to_multiplier(m_unit);

    for (int i = 0; i < freq.size(); ++i) {
        double w = 2 * M_PI * freq(i);
        Eigen::Matrix2cd abcd_point;
        abcd_point.setIdentity();

        switch (m_type) {
        case NetworkType::R_series:
            abcd_point(0, 1) = val;
            break;
        case NetworkType::R_shunt:
            abcd_point(1, 0) = 1.0 / val;
            break;
        case NetworkType::C_series:
            abcd_point(0, 1) = 1.0 / (j * w * val);
            break;
        case NetworkType::C_shunt:
            abcd_point(1, 0) = j * w * val;
            break;
        case NetworkType::L_series:
            abcd_point(0, 1) = j * w * val;
            break;
        case NetworkType::L_shunt:
            abcd_point(1, 0) = 1.0 / (j * w * val);
            break;
        }
        abcd_matrix.row(i) << abcd_point(0, 0), abcd_point(0, 1), abcd_point(1, 0), abcd_point(1, 1);
    }

    return abcd_matrix;
}

QPair<QVector<double>, QVector<double>> NetworkLumped::getPlotData(int s_param_idx, bool isPhase)
{
    if (s_param_idx < 0 || s_param_idx > 3) {
        return {};
    }
    Eigen::VectorXd freq = Eigen::VectorXd::LinSpaced(1001, m_fmin, m_fmax);
    Eigen::MatrixXcd abcd_matrix = abcd(freq);
    QVector<double> xValues, yValues;

    for (int i = 0; i < freq.size(); ++i) {
        Eigen::Matrix2cd abcd_point;
        abcd_point << abcd_matrix(i, 0), abcd_matrix(i, 1), abcd_matrix(i, 2), abcd_matrix(i, 3);
        Eigen::Vector4cd s = abcd2s(abcd_point);
        std::complex<double> s_param = s(s_param_idx);

        xValues.append(freq(i));
        if (isPhase) {
            yValues.append(std::arg(s_param) * 180.0 / M_PI);
        } else {
            yValues.append(20 * std::log10(std::abs(s_param)));
        }
    }

    if (isPhase && m_unwrap_phase) {
        Eigen::Map<Eigen::ArrayXd> yValuesEigen(yValues.data(), yValues.size());
        Eigen::ArrayXd unwrapped_y = unwrap(yValuesEigen);
        for(int i = 0; i < unwrapped_y.size(); ++i) {
            yValues[i] = unwrapped_y[i];
        }
    }

    return qMakePair(xValues, yValues);
}
