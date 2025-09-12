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
    NetworkLumped(const NetworkLumped& other);
    Network* clone() const override;

    QString name() const override;
    Eigen::MatrixXcd abcd(const Eigen::VectorXd& freq) const override;
    QPair<QVector<double>, QVector<double>> getPlotData(int s_param_idx, bool isPhase) override;

    double value() const;
    void setValue(double value);

    QString unit() const;
    void setUnit(const QString& unit);

    NetworkType type() const;


private:
    NetworkType m_type;
    double m_value;
    QString m_unit;
};

#endif // NETWORKLUMPED_H
