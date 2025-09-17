#include "networklumped.h"
#include "tdrcalculator.h"
#include <complex>
#include <cmath>
#include <QStringList>

namespace {
constexpr double pi = 3.14159265358979323846;

QVector<double> toVector(std::initializer_list<double> list)
{
    QVector<double> values;
    values.reserve(static_cast<int>(list.size()));
    for (double v : list) {
        values.append(v);
    }
    return values;
}
}

NetworkLumped::NetworkLumped(NetworkType type, QObject *parent)
    : NetworkLumped(type, QVector<double>(), parent)
{
}

NetworkLumped::NetworkLumped(NetworkType type, const QVector<double>& values, QObject *parent)
    : Network(parent), m_type(type)
{
    initializeParameters(values);
    m_fmin = 1e6;
    m_fmax = 50e9;
    m_is_visible = false;
}

NetworkLumped::NetworkLumped(NetworkType type, std::initializer_list<double> values, QObject *parent)
    : NetworkLumped(type, toVector(values), parent)
{
}

Network* NetworkLumped::clone(QObject* parent) const
{
    QVector<double> values;
    values.reserve(m_parameters.size());
    for (const auto& parameter : m_parameters) {
        values.append(parameter.value);
    }

    NetworkLumped* copy = new NetworkLumped(m_type, values, parent);
    copy->setColor(m_color);
    copy->setVisible(m_is_visible);
    copy->setUnwrapPhase(m_unwrap_phase);
    copy->setActive(m_is_active);
    copy->setFmin(m_fmin);
    copy->setFmax(m_fmax);
    return copy;
}

QString NetworkLumped::typeName() const
{
    switch (m_type) {
    case NetworkType::R_series: return QStringLiteral("R_series");
    case NetworkType::R_shunt:  return QStringLiteral("R_shunt");
    case NetworkType::C_series: return QStringLiteral("C_series");
    case NetworkType::C_shunt:  return QStringLiteral("C_shunt");
    case NetworkType::L_series: return QStringLiteral("L_series");
    case NetworkType::L_shunt:  return QStringLiteral("L_shunt");
    case NetworkType::TransmissionLine: return QStringLiteral("TL_50Ω");
    }
    return QString();
}

QString NetworkLumped::displayName() const
{
    return typeName();
}

QString NetworkLumped::name() const
{
    QString name = typeName();
    QStringList parameterParts;
    for (const auto& parameter : m_parameters) {
        parameterParts.append(QStringLiteral("%1=%2").arg(parameter.description,
                                                          Network::formatEngineering(parameter.value)));
    }

    if (!parameterParts.isEmpty()) {
        name += QLatin1Char('_') + parameterParts.join(QLatin1Char('_'));
    }

    return name;
}

Eigen::MatrixXcd NetworkLumped::abcd(const Eigen::VectorXd& freq) const
{
    Eigen::MatrixXcd abcd_matrix(freq.size(), 4);
    std::complex<double> j(0, 1);
    constexpr double z0 = 50.0;
    constexpr double c0 = 299792458.0; // Speed of light in vacuum (m/s)

    for (int i = 0; i < freq.size(); ++i) {
        double w = 2.0 * pi * freq(i);
        Eigen::Matrix2cd abcd_point;
        abcd_point.setIdentity();

        switch (m_type) {
        case NetworkType::R_series:
            abcd_point(0, 1) = parameterValueSI(0);
            break;
        case NetworkType::R_shunt:
            abcd_point(1, 0) = 1.0 / parameterValueSI(0);
            break;
        case NetworkType::C_series: {
            std::complex<double> impedance = 1.0 / (j * w * parameterValueSI(0));
            abcd_point(0, 1) = impedance;
            break;
        }
        case NetworkType::C_shunt:
            abcd_point(1, 0) = j * w * parameterValueSI(0);
            break;
        case NetworkType::L_series: {
            std::complex<double> impedance = parameterValueSI(1) + j * w * parameterValueSI(0);
            abcd_point(0, 1) = impedance;
            break;
        }
        case NetworkType::L_shunt: {
            std::complex<double> impedance = parameterValueSI(1) + j * w * parameterValueSI(0);
            abcd_point(1, 0) = 1.0 / impedance;
            break;
        }
        case NetworkType::TransmissionLine: {
            double beta = w / c0;
            double theta = beta * parameterValueSI(0);
            double cos_theta = std::cos(theta);
            double sin_theta = std::sin(theta);
            abcd_point(0, 0) = cos_theta;
            abcd_point(0, 1) = j * z0 * sin_theta;
            abcd_point(1, 0) = j * (sin_theta / z0);
            abcd_point(1, 1) = cos_theta;
            break;
        }
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
    if (type == PlotType::TDR) {
        if (s_param_idx != 0 && s_param_idx != 3)
            return {};
        Eigen::ArrayXcd sparam(freq.size());
        for (int i = 0; i < freq.size(); ++i) {
            Eigen::Matrix2cd abcd_point;
            abcd_point << abcd_matrix(i, 0), abcd_matrix(i, 1), abcd_matrix(i, 2), abcd_matrix(i, 3);
            Eigen::Vector4cd s = abcd2s(abcd_point);
            sparam(i) = s(s_param_idx);
        }
        TDRCalculator calculator;
        auto result = calculator.compute(freq.array(), sparam);
        return qMakePair(result.distance, result.impedance);
    }

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
            yValues.append(std::arg(s_param) * 180.0 / pi);
            break;
        case PlotType::VSWR:
            xValues.append(freq(i));
            yValues.append((1 + std::abs(s_param)) / (1 - std::abs(s_param)));
            break;
        case PlotType::Smith:
            xValues.append(std::real(s_param));
            yValues.append(std::imag(s_param));
            break;
        case PlotType::TDR:
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

QVector<double> NetworkLumped::frequencies() const
{
    Eigen::VectorXd freq = Eigen::VectorXd::LinSpaced(1001, m_fmin, m_fmax);
    return QVector<double>(freq.data(), freq.data() + freq.size());
}

double NetworkLumped::value() const
{
    if (m_parameters.isEmpty())
        return 0.0;
    return m_parameters.first().value;
}

void NetworkLumped::setValue(double value)
{
    setParameterValue(0, value);
}

int NetworkLumped::parameterCount() const
{
    return m_parameters.size();
}

QString NetworkLumped::parameterDescription(int index) const
{
    if (index < 0 || index >= m_parameters.size())
        return QString();
    return m_parameters.at(index).description;
}

double NetworkLumped::parameterValue(int index) const
{
    if (index < 0 || index >= m_parameters.size())
        return 0.0;
    return m_parameters.at(index).value;
}

void NetworkLumped::setParameterValue(int index, double value)
{
    if (index < 0 || index >= m_parameters.size())
        return;
    m_parameters[index].value = value;
}

void NetworkLumped::initializeParameters(const QVector<double>& values)
{
    m_parameters.clear();

    const QString resistanceLabel = QStringLiteral("R_Ω");
    const QString seriesResistanceLabel = QStringLiteral("R_ser_Ω");

    switch (m_type) {
    case NetworkType::R_series:
        m_parameters.append({resistanceLabel, 50.0, 1.0});
        break;
    case NetworkType::R_shunt:
        m_parameters.append({resistanceLabel, 50.0, 1.0});
        break;
    case NetworkType::C_series:
        m_parameters.append({QStringLiteral("C_pF"), 1.0, 1e-12});
        break;
    case NetworkType::C_shunt:
        m_parameters.append({QStringLiteral("C_pF"), 1.0, 1e-12});
        break;
    case NetworkType::L_series:
        m_parameters.append({QStringLiteral("L_nH"), 1.0, 1e-9});
        m_parameters.append({seriesResistanceLabel, 1.0, 1.0});
        break;
    case NetworkType::L_shunt:
        m_parameters.append({QStringLiteral("L_nH"), 1.0, 1e-9});
        m_parameters.append({seriesResistanceLabel, 1.0, 1.0});
        break;
    case NetworkType::TransmissionLine:
        m_parameters.append({QStringLiteral("Len_m"), 1e-3, 1.0});
        break;
    }

    for (int i = 0; i < values.size() && i < m_parameters.size(); ++i) {
        m_parameters[i].value = values.at(i);
    }
}

double NetworkLumped::parameterValueSI(int index) const
{
    if (index < 0 || index >= m_parameters.size())
        return 0.0;
    const auto &parameter = m_parameters.at(index);
    return parameter.value * parameter.scale;
}
