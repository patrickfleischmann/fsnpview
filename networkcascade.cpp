#include "networkcascade.h"
#include "tdrcalculator.h"
#include <limits>
#include <algorithm>
#include <optional>
#include <vector>

namespace {
constexpr double pi = 3.14159265358979323846;

int sanitizePort(int requestedPort, int portCount)
{
    if (portCount <= 0)
        return 1;
    return std::clamp(requestedPort, 1, portCount);
}

int defaultFromPort(int /*portCount*/)
{
    return 1;
}

int defaultToPort(int portCount)
{
    if (portCount >= 2)
        return 2;
    return 1;
}

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
    const int portCount = network ? network->portCount() : 0;
    const int toPort = defaultToPort(portCount);
    const int fromPort = defaultFromPort(portCount);
    m_toPorts.insert(index, toPort);
    m_fromPorts.insert(index, fromPort);
    setNetworkPortSelection(index, toPort, fromPort);
    updateFrequencyRange();
}

void NetworkCascade::moveNetwork(int from, int to)
{
    if (from < 0 || from >= m_networks.size() || to < 0 || to >= m_networks.size())
        return;
    if (from == to)
        return;
    m_networks.insert(to, m_networks.takeAt(from));
    m_toPorts.insert(to, m_toPorts.takeAt(from));
    m_fromPorts.insert(to, m_fromPorts.takeAt(from));
    updateFrequencyRange();
}

void NetworkCascade::removeNetwork(int index)
{
    if (index >= 0 && index < m_networks.size()) {
        m_networks.removeAt(index);
        m_toPorts.removeAt(index);
        m_fromPorts.removeAt(index);
        updateFrequencyRange();
    }
}

void NetworkCascade::clearNetworks()
{
    m_networks.clear();
    m_toPorts.clear();
    m_fromPorts.clear();
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

    struct StageData
    {
        Eigen::MatrixXcd response;
        int ports = 0;
        int inputPort = 0;
        int outputPort = 0;
    };

    std::vector<StageData> stages;
    stages.reserve(m_networks.size());

    for (int idx = 0; idx < m_networks.size(); ++idx) {
        Network* network = m_networks.at(idx);
        if (!network || !network->isActive())
            continue;

        StageData stage;
        stage.response = network->sparameters(freq);
        stage.ports = std::max(network->portCount(), 1);

        const auto selection = networkPortSelection(idx);
        stage.outputPort = sanitizePort(selection.first, stage.ports) - 1;
        stage.inputPort = sanitizePort(selection.second, stage.ports) - 1;
        if (stage.outputPort < 0)
            stage.outputPort = 0;
        if (stage.inputPort < 0)
            stage.inputPort = 0;

        stages.push_back(std::move(stage));
    }

    Eigen::MatrixXcd total(freq.size(), 4);
    for (int row = 0; row < freq.size(); ++row) {
        Eigen::Matrix2cd accumulated;
        accumulated << 0.0, 1.0,
                        1.0, 0.0;

        for (const auto& stage : stages) {
            const Eigen::MatrixXcd& response = stage.response;
            const int ports = stage.ports;
            if (response.rows() != freq.size())
                continue;

            const Eigen::Index requiredCols = static_cast<Eigen::Index>(ports) * static_cast<Eigen::Index>(ports);
            if (response.cols() < requiredCols)
                continue;

            const Eigen::Index s11Index = static_cast<Eigen::Index>(stage.inputPort * ports + stage.inputPort);
            const Eigen::Index s12Index = static_cast<Eigen::Index>(stage.outputPort * ports + stage.inputPort);
            const Eigen::Index s21Index = static_cast<Eigen::Index>(stage.inputPort * ports + stage.outputPort);
            const Eigen::Index s22Index = static_cast<Eigen::Index>(stage.outputPort * ports + stage.outputPort);

            if (s11Index >= response.cols() || s12Index >= response.cols() ||
                s21Index >= response.cols() || s22Index >= response.cols()) {
                continue;
            }

            Eigen::Matrix2cd s_matrix;
            s_matrix << response(row, s11Index), response(row, s12Index),
                         response(row, s21Index), response(row, s22Index);

            accumulated = redhefferStar(accumulated, s_matrix);
        }

        total.row(row) << accumulated(0, 0), accumulated(1, 0),
                           accumulated(0, 1), accumulated(1, 1);
    }

    return total;
}

void NetworkCascade::setNetworkPortSelection(int index, int toPort, int fromPort)
{
    if (index < 0 || index >= m_networks.size())
        return;

    Network* network = m_networks.at(index);
    const int portCount = network ? network->portCount() : 0;

    const int sanitizedFrom = sanitizePort(fromPort, portCount);
    int sanitizedTo = sanitizePort(toPort, portCount);

    if (portCount == 1)
        sanitizedTo = sanitizedFrom;

    if (index >= m_fromPorts.size())
        m_fromPorts.resize(m_networks.size(), defaultFromPort(portCount));
    if (index >= m_toPorts.size())
        m_toPorts.resize(m_networks.size(), defaultToPort(portCount));

    m_fromPorts[index] = sanitizedFrom;
    m_toPorts[index] = sanitizedTo;
}

int NetworkCascade::toPort(int index) const
{
    return networkPortSelection(index).first;
}

int NetworkCascade::fromPort(int index) const
{
    return networkPortSelection(index).second;
}

QPair<int, int> NetworkCascade::networkPortSelection(int index) const
{
    if (index < 0 || index >= m_networks.size())
        return qMakePair(1, 1);

    Network* network = m_networks.at(index);
    const int portCount = network ? network->portCount() : 0;

    int storedTo = (index < m_toPorts.size()) ? m_toPorts.at(index) : defaultToPort(portCount);
    int storedFrom = (index < m_fromPorts.size()) ? m_fromPorts.at(index) : defaultFromPort(portCount);

    int sanitizedFrom = sanitizePort(storedFrom, portCount);
    int sanitizedTo = sanitizePort(storedTo, portCount);

    if (portCount == 1)
        sanitizedTo = sanitizedFrom;

    if (sanitizedTo != storedTo && index < m_toPorts.size())
        const_cast<NetworkCascade*>(this)->m_toPorts[index] = sanitizedTo;
    if (sanitizedFrom != storedFrom && index < m_fromPorts.size())
        const_cast<NetworkCascade*>(this)->m_fromPorts[index] = sanitizedFrom;

    return qMakePair(sanitizedTo, sanitizedFrom);
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
        Eigen::ArrayXd realPart = sparam.real();
        Eigen::ArrayXd imagPart = sparam.imag();
        QVector<double> xValues(realPart.data(), realPart.data() + realPart.size());
        QVector<double> yValues(imagPart.data(), imagPart.data() + imagPart.size());
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
    for (int i = 0; i < m_networks.size(); ++i) {
        Network* net = m_networks.at(i);
        if (!net)
            continue;
        Network* cloned = net->clone(copy);
        copy->addNetwork(cloned);
        const auto selection = networkPortSelection(i);
        const int insertedIndex = copy->getNetworks().size() - 1;
        copy->setNetworkPortSelection(insertedIndex, selection.first, selection.second);
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

