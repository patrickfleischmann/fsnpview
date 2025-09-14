#include "networklumped.h"
#include <complex>

NetworkLumped::NetworkLumped(NetworkType type, double value, QObject *parent)
    : Network(parent), m_type(type), m_value(value)
{
    m_fmin = 1e6;
    m_fmax = 50e9;
    m_is_visible = false;
}

Network* NetworkLumped::clone(QObject* parent) const
{
    NetworkLumped* copy = new NetworkLumped(m_type, m_value, parent);
    copy->setColor(m_color);
    copy->setVisible(m_is_visible);
    copy->setUnwrapPhase(m_unwrap_phase);
    copy->setActive(m_is_active);
    copy->setFmin(m_fmin);
    copy->setFmax(m_fmax);
    return copy;
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
    return name + "_" + QString::number(m_value);
}

Eigen::MatrixXcd NetworkLumped::abcd(const Eigen::VectorXd& freq) const
{
    Eigen::MatrixXcd abcd_matrix(freq.size(), 4);
    std::complex<double> j(0, 1);

    for (int i = 0; i < freq.size(); ++i) {
        double w = 2 * M_PI * freq(i);
        Eigen::Matrix2cd abcd_point;
        abcd_point.setIdentity();

        switch (m_type) {
        case NetworkType::R_series:
            abcd_point(0, 1) = m_value;
            break;
        case NetworkType::R_shunt:
            abcd_point(1, 0) = 1.0 / m_value;
            break;
        case NetworkType::C_series:
            abcd_point(0, 1) = 1.0 / (j * w * m_value);
            break;
        case NetworkType::C_shunt:
            abcd_point(1, 0) = j * w * m_value;
            break;
        case NetworkType::L_series:
            abcd_point(0, 1) = j * w * m_value;
            break;
        case NetworkType::L_shunt:
            abcd_point(1, 0) = 1.0 / (j * w * m_value);
            break;
        }
        abcd_matrix.row(i) << abcd_point(0, 0), abcd_point(0, 1), abcd_point(1, 0), abcd_point(1, 1);
    }

    return abcd_matrix;
}

QPair<QVector<double>, QVector<double>> NetworkLumped::getPlotData(int s_param_idx, PlotType type)
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

        switch (type) {
        case PlotType::Magnitude:
            xValues.append(freq(i));
            yValues.append(20 * std::log10(std::abs(s_param)));
            break;
        case PlotType::Phase:
            xValues.append(freq(i));
            yValues.append(std::arg(s_param) * 180.0 / M_PI);
            break;
        case PlotType::VSWR:
            xValues.append(freq(i));
            yValues.append((1 + std::abs(s_param)) / (1 - std::abs(s_param)));
            break;
        case PlotType::Smith:
            xValues.append(std::real(s_param));
            yValues.append(std::imag(s_param));
            break;
        }
    }

    if (type == PlotType::Phase && m_unwrap_phase) {
        Eigen::Map<Eigen::ArrayXd> yValuesEigen(yValues.data(), yValues.size());
        Eigen::ArrayXd unwrapped_y = unwrap(yValuesEigen);
        for(int i = 0; i < unwrapped_y.size(); ++i) {
            yValues[i] = unwrapped_y[i];
        }
    }

    return qMakePair(xValues, yValues);
}
