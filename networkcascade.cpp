#include "networkcascade.h"
#include "tdrcalculator.h"
#include <limits>
#include <algorithm>
#include <optional>
#include <vector>

namespace {
constexpr double pi = 3.14159265358979323846;

Eigen::Matrix2cd redhefferStar(const Eigen::Matrix2cd& left, const Eigen::Matrix2cd& right)
{
    const std::complex<double> s11_left = left(0, 0);
    const std::complex<double> s12_left = left(0, 1);
    const std::complex<double> s21_left = left(1, 0);
    const std::complex<double> s22_left = left(1, 1);

    const std::complex<double> s11_right = right(0, 0);
    const std::complex<double> s12_right = right(0, 1);
    const std::complex<double> s21_right = right(1, 0);
    const std::complex<double> s22_right = right(1, 1);

    std::complex<double> denominator = 1.0 - s22_left * s11_right;
    if (std::abs(denominator) < 1e-18)
    {
        // Regularize near-singular connections to avoid numerical blow-up
        denominator = std::complex<double>(std::numeric_limits<double>::epsilon(), 0.0);
    }
    const std::complex<double> inv = 1.0 / denominator;

    Eigen::Matrix2cd result;
    result(0, 0) = s11_left + s12_left * s11_right * s21_left * inv;
    result(0, 1) = s12_left * s12_right * inv;
    result(1, 0) = s21_left * s21_right * inv;
    result(1, 1) = s22_right + s21_right * s22_left * s12_right * inv;
    return result;
}
}

NetworkCascade::NetworkCascade(QObject *parent)
    : Network(parent)
    , m_pointCount(2001)
    , m_manualFrequencyRange(false)
{
    m_fmin = 1e6;
    m_fmax = 10e9;
}

NetworkCascade::~NetworkCascade()
{
    for (auto net : m_networks) {
        if (net->parent() == this) {
            net->setParent(nullptr);
        }
    }
}

void NetworkCascade::addNetwork(Network* network)
{
    insertNetwork(m_networks.size(), network);
}

void NetworkCascade::insertNetwork(int index, Network* network)
{
    if (index < 0 || index > m_networks.size())
        index = m_networks.size();
    m_networks.insert(index, network);
    updateFrequencyRange();
}

void NetworkCascade::moveNetwork(int from, int to)
{
    if (from < 0 || from >= m_networks.size() || to < 0 || to >= m_networks.size())
        return;
    if (from == to)
        return;
    m_networks.insert(to, m_networks.takeAt(from));
    updateFrequencyRange();
}

void NetworkCascade::removeNetwork(int index)
{
    if (index >= 0 && index < m_networks.size()) {
        m_networks.removeAt(index);
        updateFrequencyRange();
    }
}

void NetworkCascade::clearNetworks()
{
    m_networks.clear();
    updateFrequencyRange();
}

const QList<Network*>& NetworkCascade::getNetworks() const
{
    return m_networks;
}


void NetworkCascade::updateFrequencyRange()
{
    if (m_manualFrequencyRange)
        return;

    if (m_networks.isEmpty()) {
        m_fmin = 1e6;
        m_fmax = 10e9;
        return;
    }

    m_fmin = std::numeric_limits<double>::max();
    m_fmax = std::numeric_limits<double>::min();

    for (const auto& network : m_networks) {
        if (network->fmin() < m_fmin) {
            m_fmin = network->fmin();
        }
        if (network->fmax() > m_fmax) {
            m_fmax = network->fmax();
        }
    }
}

QString NetworkCascade::name() const
{
    return "Cascade";
}

int NetworkCascade::portCount() const
{
    return 2;
}

Eigen::MatrixXcd NetworkCascade::sparameters(const Eigen::VectorXd& freq) const
{
    if (freq.size() == 0)
        return {};

    QList<Network*> activeNetworks;
    for (const auto& network : m_networks) {
        if (network->isActive())
            activeNetworks.append(network);
    }

    std::vector<Eigen::MatrixXcd> networkResponses;
    networkResponses.reserve(activeNetworks.size());
    for (const auto& network : activeNetworks) {
        networkResponses.push_back(network->sparameters(freq));
    }

    Eigen::MatrixXcd total(freq.size(), 4);
    for (int row = 0; row < freq.size(); ++row) {
        Eigen::Matrix2cd accumulated;
        accumulated << 0.0, 1.0,
                        1.0, 0.0;

        for (const auto& response : networkResponses) {
            if (response.rows() != freq.size() || response.cols() < 4)
                continue;

            Eigen::Matrix2cd s_matrix;
            s_matrix << response(row, 0), response(row, 1),
                         response(row, 2), response(row, 3);

            accumulated = redhefferStar(accumulated, s_matrix);
        }

        total.row(row) << accumulated(0, 0), accumulated(0, 1),
                           accumulated(1, 0), accumulated(1, 1);
    }

    return total;
}

QPair<QVector<double>, QVector<double>> NetworkCascade::getPlotData(int s_param_idx, PlotType type)
{
    updateFrequencyRange();
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

Network* NetworkCascade::clone(QObject* parent) const
{
    NetworkCascade* copy = new NetworkCascade(parent);
    copy->setColor(m_color);
    copy->setVisible(m_is_visible);
    copy->setUnwrapPhase(m_unwrap_phase);
    copy->setActive(m_is_active);
    copy->setFrequencyRange(m_fmin, m_fmax, m_manualFrequencyRange);
    copy->setPointCount(m_pointCount);
    copy->copyStyleSettingsFrom(this);
    for (const auto& net : m_networks) {
        copy->addNetwork(net->clone(copy));
    }
    return copy;
}

QVector<double> NetworkCascade::frequencies() const
{
    const_cast<NetworkCascade*>(this)->updateFrequencyRange();
    const int points = std::max(m_pointCount, 2);
    Eigen::VectorXd freq = Eigen::VectorXd::LinSpaced(points, m_fmin, m_fmax);
    return QVector<double>(freq.data(), freq.data() + freq.size());
}

void NetworkCascade::setFrequencyRange(double fmin, double fmax, bool manualOverride)
{
    if (fmax <= fmin)
        return;

    m_manualFrequencyRange = manualOverride;
    m_fmin = fmin;
    m_fmax = fmax;

    if (!m_manualFrequencyRange)
        updateFrequencyRange();
}

void NetworkCascade::clearManualFrequencyRange()
{
    if (!m_manualFrequencyRange)
        return;

    m_manualFrequencyRange = false;
    updateFrequencyRange();
}

bool NetworkCascade::hasManualFrequencyRange() const
{
    return m_manualFrequencyRange;
}

void NetworkCascade::setPointCount(int pointCount)
{
    if (pointCount < 2)
        pointCount = 2;
    m_pointCount = pointCount;
}

int NetworkCascade::pointCount() const
{
    return m_pointCount;
}

