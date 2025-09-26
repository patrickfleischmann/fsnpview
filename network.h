#ifndef NETWORK_H
#define NETWORK_H

#include <QObject>
#include <QString>
#include <QColor>
#include <QVector>
#include <QPair>
#include <Eigen/Dense>
#include <complex>

enum class PlotType { Magnitude, Phase, VSWR, Smith, TDR };

class Network : public QObject
{
    Q_OBJECT
public:
    explicit Network(QObject *parent = nullptr);
    virtual ~Network() = default;

    static Eigen::Matrix2cd s2abcd(const std::complex<double>& s11, const std::complex<double>& s12, const std::complex<double>& s21, const std::complex<double>& s22, double z0 = 50.0);
    static Eigen::Vector4cd abcd2s(const Eigen::Matrix2cd& abcd, double z0 = 50.0);
    static QString formatEngineering(double value, bool padMantissa = true);

    virtual QString name() const = 0;
    virtual QString displayName() const;
    virtual Eigen::MatrixXcd sparameters(const Eigen::VectorXd& freq) const = 0;
    virtual QPair<QVector<double>, QVector<double>> getPlotData(int s_param_idx, PlotType type) = 0;
    virtual Network* clone(QObject* parent = nullptr) const = 0;
    virtual QVector<double> frequencies() const = 0;
    virtual int portCount() const = 0;

    struct TimeGateSettings
    {
        bool enabled = false;
        double startDistance = 0.0;
        double stopDistance = 0.0;
        double epsilonR = 1.0;
    };

    static void setTimeGateSettings(const TimeGateSettings& settings);
    static TimeGateSettings timeGateSettings();

    double fmin() const;
    void setFmin(double fmin);

    double fmax() const;
    void setFmax(double fmax);

    QColor color() const;
    void setColor(const QColor &color);

    bool isVisible() const;
    void setVisible(bool visible);

    bool unwrapPhase() const;
    void setUnwrapPhase(bool unwrap);

    bool isActive() const;
    void setActive(bool active);

protected:
    Eigen::ArrayXd unwrap(const Eigen::ArrayXd& phase);

    double m_fmin;
    double m_fmax;
    QColor m_color;
    bool m_is_visible;
    bool m_unwrap_phase;
    bool m_is_active; // for cascade calculation
};

#endif // NETWORK_H
