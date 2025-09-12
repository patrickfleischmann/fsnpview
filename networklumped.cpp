#include "networklumped.h"
#include <complex>
#include <QDebug>

NetworkLumped::NetworkLumped(NetworkType type, double value, QObject *parent)
    : Network(parent), m_type(type), m_value(value)
{
    m_fmin = 1e6;
    m_fmax = 10e9;
    m_is_visible = false;
    switch (type) {
        case NetworkType::R_series:
        case NetworkType::R_shunt:
            m_unit = "Ohm";
            break;
        case NetworkType::C_series:
        case NetworkType::C_shunt:
            m_unit = "F";
            break;
        case NetworkType::L_series:
        case NetworkType::L_shunt:
            m_unit = "H";
            break;
    }
}

NetworkLumped::NetworkLumped(const NetworkLumped& other)
    : Network(other.parent())
    , m_type(other.m_type)
    , m_value(other.m_value)
    , m_unit(other.m_unit)
{
    m_fmin = other.m_fmin;
    m_fmax = other.m_fmax;
    m_color = other.m_color;
    m_is_visible = other.m_is_visible;
    m_unwrap_phase = other.m_unwrap_phase;
    m_is_active = other.m_is_active;
}

Network* NetworkLumped::clone() const
{
    qDebug() << "cloning lumped";
    return new NetworkLumped(*this);
}

QString NetworkLumped::name() const
{
    QString name;
    switch (m_type) {
    case NetworkType::R_series: name = "R_series"; break;
    case NetworkType::R_shunt:  name = "R_shunt";  break;
    case NetworkType::C_series: name = "C_series"; break;
    case NetworkType::C_shunt:  name = "C_shunt";  break;
    case NetworkType::L_series: name = "L_series"; break;
    case NetworkType::L_shunt:  name = "L_shunt";  break;
    }
    return name;
}

double NetworkLumped::value() const {
    return m_value;
}

void NetworkLumped::setValue(double value) {
    m_value = value;
}

QString NetworkLumped::unit() const {
    return m_unit;
}

void NetworkLumped::setUnit(const QString& unit) {
    m_unit = unit;
}

NetworkLumped::NetworkType NetworkLumped::type() const {
    return m_type;
}

Eigen::MatrixXcd NetworkLumped::abcd(const Eigen::VectorXd& freq) const
{
    Eigen::MatrixXcd abcd_matrix(freq.size(), 4);
    std::complex<double> j(0, 1);

    double value_scaled = m_value;
    if (m_unit == "f") value_scaled *= 1e-15;
    else if (m_unit == "p") value_scaled *= 1e-12;
    else if (m_unit == "n") value_scaled *= 1e-9;
    else if (m_unit == "u") value_scaled *= 1e-6;
    else if (m_unit == "m") value_scaled *= 1e-3;
    else if (m_unit == "k") value_scaled *= 1e3;
    else if (m_unit == "M") value_scaled *= 1e6;
    else if (m_unit == "G") value_scaled *= 1e9;

    for (int i = 0; i < freq.size(); ++i) {
        double w = 2 * M_PI * freq(i);
        Eigen::Matrix2cd abcd_point;
        abcd_point.setIdentity();

        switch (m_type) {
        case NetworkType::R_series:
            abcd_point(0, 1) = value_scaled;
            break;
        case NetworkType::R_shunt:
            abcd_point(1, 0) = 1.0 / value_scaled;
            break;
        case NetworkType::C_series:
            abcd_point(0, 1) = 1.0 / (j * w * value_scaled);
            break;
        case NetworkType::C_shunt:
            abcd_point(1, 0) = j * w * value_scaled;
            break;
        case NetworkType::L_series:
            abcd_point(0, 1) = j * w * value_scaled;
            break;
        case NetworkType::L_shunt:
            abcd_point(1, 0) = 1.0 / (j * w * value_scaled);
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
