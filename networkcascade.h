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
    Eigen::MatrixXcd abcd(const Eigen::VectorXd& freq) const override;
    QPair<QVector<double>, QVector<double>> getPlotData(int s_param_idx, PlotType type) override;
    Network* clone(QObject* parent = nullptr) const override;

    QVector<double> frequencies() const override;


private:
    void updateFrequencyRange();

    QList<Network*> m_networks;
};

#endif // NETWORKCASCADE_H
