#ifndef NETWORKLUMPED_H
#define NETWORKLUMPED_H

#include "network.h"

class NetworkLumped : public Network
{
    Q_OBJECT
public:
    enum class NetworkType {
        R_series,
        R_shunt,
        C_series,
        C_shunt,
        L_series,
        L_shunt
    };

    explicit NetworkLumped(NetworkType type, double value, QObject *parent = nullptr);

    QString name() const override;
    Eigen::MatrixXcd abcd(const Eigen::VectorXd& freq) const override;
    QPair<QVector<double>, QVector<double>> getPlotData(int s_param_idx, PlotType type) override;
    Network* clone(QObject* parent = nullptr) const override;
    QVector<double> frequencies() const override;

private:
    NetworkType m_type;
    double m_value;
};

#endif // NETWORKLUMPED_H
