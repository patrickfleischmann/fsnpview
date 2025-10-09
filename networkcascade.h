#ifndef NETWORKCASCADE_H
#define NETWORKCASCADE_H

#include "network.h"
#include <QList>
#include <memory>

class NetworkCascade : public Network
{
    Q_OBJECT
public:
    explicit NetworkCascade(QObject *parent = nullptr);
    ~NetworkCascade();

    void addNetwork(Network* network);
    void insertNetwork(int index, Network* network);
    void moveNetwork(int from, int to);
    void removeNetwork(int index);
    void clearNetworks();
    const QList<Network*>& getNetworks() const;

    QString name() const override;
    Eigen::MatrixXcd sparameters(const Eigen::VectorXd& freq) const override;
    QPair<QVector<double>, QVector<double>> getPlotData(int s_param_idx, PlotType type) override;
    Network* clone(QObject* parent = nullptr) const override;

    QVector<double> frequencies() const override;
    int portCount() const override;

    void setNetworkPortSelection(int index, int toPort, int fromPort);
    int toPort(int index) const;
    int fromPort(int index) const;
    QPair<int, int> networkPortSelection(int index) const;

    void setFrequencyRange(double fmin, double fmax, bool manualOverride = true);
    void clearManualFrequencyRange();
    bool hasManualFrequencyRange() const;
    void setPointCount(int pointCount);
    int pointCount() const;


private:
    void updateFrequencyRange();

    QList<Network*> m_networks;
    QList<int> m_toPorts;
    QList<int> m_fromPorts;
    int m_pointCount;
    bool m_manualFrequencyRange;
};

#endif // NETWORKCASCADE_H
