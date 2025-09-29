#ifndef NETWORKLUMPED_H
#define NETWORKLUMPED_H

#include "network.h"
#include <initializer_list>
#include <QVector>

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
        L_shunt,
        TransmissionLine,
        TransmissionLineLossy,
        LRC_series_shunt,
        LRC_parallel_series
    };

    explicit NetworkLumped(NetworkType type, QObject *parent = nullptr);
    NetworkLumped(NetworkType type, const QVector<double>& values, QObject *parent = nullptr);
    NetworkLumped(NetworkType type, std::initializer_list<double> values, QObject *parent = nullptr);

    NetworkType type() const;

    QString name() const override;
    QString displayName() const override;
    Eigen::MatrixXcd sparameters(const Eigen::VectorXd& freq) const override;
    QPair<QVector<double>, QVector<double>> getPlotData(int s_param_idx, PlotType type) override;
    Network* clone(QObject* parent = nullptr) const override;
    QVector<double> frequencies() const override;
    int portCount() const override;

    void setPointCount(int pointCount);
    int pointCount() const;

    int parameterCount() const;
    QString parameterDescription(int index) const;
    double parameterValue(int index) const;
    void setParameterValue(int index, double value);

    double value() const;
    void setValue(double value);


private:
    NetworkType m_type;
    struct Parameter {
        QString description;
        double value;
        double scale;
    };
    QVector<Parameter> m_parameters;

    int m_pointCount;

    void initializeParameters(const QVector<double>& values);
    double parameterValueSI(int index) const;
    QString typeName() const;
};

#endif // NETWORKLUMPED_H
