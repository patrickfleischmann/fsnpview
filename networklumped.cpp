#include "networklumped.h"
#include "tdrcalculator.h"
#include <complex>
#include <cmath>
#include <QStringList>
#include <algorithm>
#include <optional>
#include <limits>

namespace {
constexpr double pi = 3.14159265358979323846;
constexpr double defaultTransmissionLineImpedance = 50.0;
constexpr double dbToNepers = std::log(10.0) / 20.0;

QVector<double> toVector(std::initializer_list<double> list)
{
    QVector<double> values;
    values.reserve(static_cast<int>(list.size()));
    for (double v : list) {
        values.append(v);
    }
    return values;
}

Eigen::Matrix2cd abcdToSParameterMatrix(const Eigen::Matrix2cd& abcd)
{
    Eigen::Vector4cd s = Network::abcd2s(abcd);
    Eigen::Matrix2cd scattering;
    scattering << s(0), s(1),
                  s(2), s(3);
    return scattering;
}
}

NetworkLumped::NetworkLumped(NetworkType type, QObject *parent)
    : NetworkLumped(type, QVector<double>(), parent)
{
}

NetworkLumped::NetworkLumped(NetworkType type, const QVector<double>& values, QObject *parent)
    : Network(parent), m_type(type), m_pointCount(1001)
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

NetworkLumped::NetworkType NetworkLumped::type() const
{
    return m_type;
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
    copy->setPointCount(m_pointCount);
    copy->copyStyleSettingsFrom(this);
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
    case NetworkType::TransmissionLine: return QStringLiteral("TL");
    case NetworkType::TransmissionLineLossy: return QStringLiteral("TL_lossy");
    case NetworkType::RLC_series_shunt: return QStringLiteral("RLC_ser_shunt");
    case NetworkType::RLC_parallel_series: return QStringLiteral("RLC_par_ser");
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

Eigen::MatrixXcd NetworkLumped::sparameters(const Eigen::VectorXd& freq) const
{
    Eigen::MatrixXcd scattering_matrix(freq.size(), 4);
    std::complex<double> j(0, 1);
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
        case NetworkType::TransmissionLine:
        case NetworkType::TransmissionLineLossy: {
            const double length = parameterValueSI(0);
            double z0_value = parameterValueSI(1);
            if (z0_value == 0.0)
                z0_value = defaultTransmissionLineImpedance;
            const double er_eff = std::max(parameterValueSI(2), 0.0);
            const double sqrt_er_eff = er_eff > 0.0 ? std::sqrt(er_eff) : 0.0;
            const double beta = (sqrt_er_eff * w) / c0;
            std::complex<double> gamma_line(0.0, beta);

            if (m_type == NetworkType::TransmissionLineLossy) {
                const double a = parameterValueSI(3);
                const double a_d = parameterValueSI(4);
                const double fa = parameterValueSI(5);
                const double freq_hz = freq(i);
                double conductorLoss = a;
                double dielectricLoss = a_d;
                if (fa > 0.0) {
                    const double ratio = std::max(freq_hz / fa, 0.0);
                    conductorLoss = a * std::sqrt(ratio);
                    dielectricLoss = a_d * ratio;
                }
                const double alpha_db_per_m = conductorLoss + dielectricLoss;
                const double alpha_nepers_per_m = alpha_db_per_m * dbToNepers;
                gamma_line = {alpha_nepers_per_m, beta};
            }

            const std::complex<double> zc(z0_value, 0.0);
            const std::complex<double> cosh_term = std::cosh(gamma_line * length);
            const std::complex<double> sinh_term = std::sinh(gamma_line * length);
            abcd_point(0, 0) = cosh_term;
            abcd_point(0, 1) = zc * sinh_term;
            abcd_point(1, 0) = sinh_term / zc;
            abcd_point(1, 1) = cosh_term;
            break;
        }
        case NetworkType::RLC_series_shunt: {
            const double resistance = parameterValueSI(0);
            const double inductance = parameterValueSI(1);
            const double capacitance = parameterValueSI(2);
            std::complex<double> impedance = resistance;
            impedance += j * w * inductance;
            bool infiniteImpedance = false;
            if (capacitance > 0.0) {
                if (w == 0.0) {
                    infiniteImpedance = true;
                } else {
                    impedance += 1.0 / (j * w * capacitance);
                }
            }
            if (infiniteImpedance) {
                abcd_point(1, 0) = 0.0;
            } else if (std::abs(impedance) > 0.0) {
                abcd_point(1, 0) = 1.0 / impedance;
            } else {
                abcd_point(1, 0) = std::numeric_limits<double>::infinity();
            }
            break;
        }
        case NetworkType::RLC_parallel_series: {
            const double resistance = parameterValueSI(0);
            const double inductance = parameterValueSI(1);
            const double capacitance = parameterValueSI(2);
            std::complex<double> admittance = 0.0;
            bool infiniteAdmittance = false;

            if (resistance == 0.0) {
                infiniteAdmittance = true;
            } else if (resistance != 0.0) {
                admittance += 1.0 / resistance;
            }

            if (!infiniteAdmittance) {
                if (inductance == 0.0) {
                    infiniteAdmittance = true;
                } else {
                    if (w == 0.0) {
                        infiniteAdmittance = true;
                    } else {
                        admittance += 1.0 / (j * w * inductance);
                    }
                }
            }

            if (!infiniteAdmittance && capacitance > 0.0) {
                admittance += j * w * capacitance;
            }

            if (infiniteAdmittance) {
                abcd_point(0, 1) = 0.0;
            } else if (std::abs(admittance) > 0.0) {
                abcd_point(0, 1) = 1.0 / admittance;
            } else {
                abcd_point(0, 1) = std::numeric_limits<double>::infinity();
            }
            break;
        }
        }

        Eigen::Matrix2cd scattering = abcdToSParameterMatrix(abcd_point);
        scattering_matrix.row(i) << scattering(0, 0), scattering(0, 1),
                                    scattering(1, 0), scattering(1, 1);
    }

    return scattering_matrix;
}

QPair<QVector<double>, QVector<double>> NetworkLumped::getPlotData(int s_param_idx, PlotType type)
{
    if (s_param_idx < 0 || s_param_idx > 3) {
        return {};
    }

    const int points = std::max(m_pointCount, 2);
    Eigen::VectorXd freq = Eigen::VectorXd::LinSpaced(points, m_fmin, m_fmax);
    Eigen::MatrixXcd s_matrix = sparameters(freq);

    Eigen::ArrayXcd sparam(freq.size());
    for (int i = 0; i < freq.size(); ++i) {
        sparam(i) = s_matrix(i, s_param_idx);
    }

    const int ports = portCount();
    const int outputPort = s_param_idx % ports;
    const int inputPort = s_param_idx / ports;
    const bool isReflectionParam = (outputPort == inputPort);

    Network::TimeGateSettings gateSettings = Network::timeGateSettings();
    TDRCalculator calculator;
    TDRCalculator::Parameters tdrParams;
    tdrParams.effectivePermittivity = std::max(gateSettings.epsilonR, 1.0);

    std::optional<TDRCalculator::GateResult> gateResult;
    if (gateSettings.enabled && isReflectionParam) {
        auto gated = calculator.applyGate(freq.array(), sparam,
                                         gateSettings.startDistance,
                                         gateSettings.stopDistance,
                                         gateSettings.epsilonR,
                                         tdrParams);
        if (gated) {
            sparam = gated->gatedReflection;
            gateResult = std::move(*gated);
        }
    }

    QVector<double> freqVector(freq.data(), freq.data() + freq.size());

    switch (type) {
    case PlotType::Magnitude: {
        Eigen::ArrayXd magnitude = 20 * sparam.abs().log10();
        QVector<double> values(magnitude.data(), magnitude.data() + magnitude.size());
        return qMakePair(freqVector, values);
    }
    case PlotType::Phase: {
        Eigen::ArrayXd phase_rad = Network::wrapToMinusPiPi(sparam.arg());
        if (m_unwrap_phase)
            phase_rad = unwrap(phase_rad);
        Eigen::ArrayXd phase_deg = phase_rad * (180.0 / pi);
        QVector<double> values(phase_deg.data(), phase_deg.data() + phase_deg.size());
        return qMakePair(freqVector, values);
    }
    case PlotType::GroupDelay: {
        Eigen::ArrayXd phase_rad = Network::wrapToMinusPiPi(sparam.arg());
        if (m_unwrap_phase)
            phase_rad = unwrap(phase_rad);
        Eigen::ArrayXd delay = Network::computeGroupDelay(phase_rad, freq.array());
        QVector<double> values(delay.data(), delay.data() + delay.size());
        return qMakePair(freqVector, values);
    }
    case PlotType::VSWR: {
        Eigen::ArrayXd vswr = (1 + sparam.abs()) / (1 - sparam.abs());
        QVector<double> values(vswr.data(), vswr.data() + vswr.size());
        return qMakePair(freqVector, values);
    }
    case PlotType::Smith: {
        QVector<double> xValues(sparam.real().data(), sparam.real().data() + sparam.real().size());
        QVector<double> yValues(sparam.imag().data(), sparam.imag().data() + sparam.imag().size());
        return qMakePair(xValues, yValues);
    }
    case PlotType::TDR:
        if (!isReflectionParam)
            return {};
        if (gateResult)
            return qMakePair(gateResult->distance, gateResult->impedance);
        {
            auto result = calculator.compute(freq.array(), sparam, tdrParams);
            return qMakePair(result.distance, result.impedance);
        }
    }

    return {};
}

QVector<double> NetworkLumped::frequencies() const
{
    const int points = std::max(m_pointCount, 2);
    Eigen::VectorXd freq = Eigen::VectorXd::LinSpaced(points, m_fmin, m_fmax);
    return QVector<double>(freq.data(), freq.data() + freq.size());
}

void NetworkLumped::setPointCount(int pointCount)
{
    if (pointCount < 2)
        pointCount = 2;
    m_pointCount = pointCount;
}

int NetworkLumped::pointCount() const
{
    return m_pointCount;
}

int NetworkLumped::portCount() const
{
    return 2;
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
        m_parameters.append({QStringLiteral("Len_mm"), 1.0, 1e-3});
        m_parameters.append({QStringLiteral("Z0_Ω"), defaultTransmissionLineImpedance, 1.0});
        m_parameters.append({QStringLiteral("er_eff"), 1.0, 1.0});
        break;
    case NetworkType::TransmissionLineLossy:
        m_parameters.append({QStringLiteral("Len_mm"), 1.0, 1e-3});
        m_parameters.append({QStringLiteral("Z0_Ω"), defaultTransmissionLineImpedance, 1.0});
        m_parameters.append({QStringLiteral("er_eff"), 1.0, 1.0});
        m_parameters.append({QStringLiteral("a_dBpm"), 10.0, 1.0});
        m_parameters.append({QStringLiteral("a_d_dBpm"), 1.0, 1.0});
        m_parameters.append({QStringLiteral("fa_Hz"), 1e9, 1.0});
        break;
    case NetworkType::RLC_series_shunt:
        m_parameters.append({resistanceLabel, 1e-3, 1.0});
        m_parameters.append({QStringLiteral("L_nH"), 1.0, 1e-9});
        m_parameters.append({QStringLiteral("C_pF"), 1.0, 1e-12});
        break;
    case NetworkType::RLC_parallel_series:
        m_parameters.append({resistanceLabel, 1e6, 1.0});
        m_parameters.append({QStringLiteral("L_nH"), 1.0, 1e-9});
        m_parameters.append({QStringLiteral("C_pF"), 1.0, 1e-12});
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
