#include "networkcascade.h"
#include <limits>

NetworkCascade::NetworkCascade(QObject *parent) : Network(parent)
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

Eigen::MatrixXcd NetworkCascade::abcd(const Eigen::VectorXd& freq) const
{
    Eigen::MatrixXcd total_abcd(freq.size(), 4);
    for (int i = 0; i < freq.size(); ++i) {
        total_abcd.row(i) << 1, 0, 0, 1;
    }

    QList<Network*> active_networks;
    for (const auto& network : m_networks) {
        if (network->isActive()) {
            active_networks.append(network);
        }
    }

    for (const auto& network : active_networks) {
        Eigen::MatrixXcd network_abcd = network->abcd(freq);
        for (int i = 0; i < freq.size(); ++i) {
            Eigen::Matrix2cd abcd_point_mat;
            abcd_point_mat << network_abcd(i, 0), network_abcd(i, 1),
                              network_abcd(i, 2), network_abcd(i, 3);

            Eigen::Matrix2cd total_abcd_mat;
            total_abcd_mat << total_abcd(i, 0), total_abcd(i, 1),
                              total_abcd(i, 2), total_abcd(i, 3);

            Eigen::Matrix2cd result = total_abcd_mat * abcd_point_mat;
            total_abcd.row(i) << result(0, 0), result(0, 1), result(1, 0), result(1, 1);
        }
    }

    return total_abcd;
}

QPair<QVector<double>, QVector<double>> NetworkCascade::getPlotData(int s_param_idx, PlotType type)
{
    updateFrequencyRange();
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

Network* NetworkCascade::clone(QObject* parent) const
{
    NetworkCascade* copy = new NetworkCascade(parent);
    copy->setColor(m_color);
    copy->setVisible(m_is_visible);
    copy->setUnwrapPhase(m_unwrap_phase);
    copy->setActive(m_is_active);
    copy->setFmin(m_fmin);
    copy->setFmax(m_fmax);
    for (const auto& net : m_networks) {
        copy->addNetwork(net->clone(copy));
    }
    return copy;
}

QVector<double> NetworkCascade::frequencies() const
{
    const_cast<NetworkCascade*>(this)->updateFrequencyRange();
    Eigen::VectorXd freq = Eigen::VectorXd::LinSpaced(1001, m_fmin, m_fmax);
    return QVector<double>(freq.data(), freq.data() + freq.size());
}

