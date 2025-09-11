#ifndef NETWORKLUMPED_H
#define NETWORKLUMPED_H

#include "network.h"
#include <QString>

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

    explicit NetworkLumped(NetworkType type, double value, const QString& unit, QObject *parent = nullptr);

    QString name() const override;
    Eigen::MatrixXcd abcd(const Eigen::VectorXd& freq) const override;
    QPair<QVector<double>, QVector<double>> getPlotData(int s_param_idx, bool isPhase) override;

    double value() const;
    void setValue(double value);

    QString unit() const;
    void setUnit(const QString &unit);

signals:
    void valueChanged();

private:
    NetworkType m_type;
    double m_value;
    QString m_unit;
};

#endif // NETWORKLUMPED_H
