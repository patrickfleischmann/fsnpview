#include "network.h"
#include <complex>
#include <cmath>

Eigen::Matrix2cd Network::s2abcd(const std::complex<double>& s11, const std::complex<double>& s12, const std::complex<double>& s21, const std::complex<double>& s22, double z0)
{
    std::complex<double> A = ((1.0 + s11) * (1.0 - s22) + s12 * s21) / (2.0 * s21);
    std::complex<double> B = z0 * ((1.0 + s11) * (1.0 + s22) - s12 * s21) / (2.0 * s21);
    std::complex<double> C = (1.0 / z0) * ((1.0 - s11) * (1.0 - s22) - s12 * s21) / (2.0 * s21);
    std::complex<double> D = ((1.0 - s11) * (1.0 + s22) + s12 * s21) / (2.0 * s21);
    Eigen::Matrix2cd abcd;
    abcd << A, B, C, D;
    return abcd;
}

Eigen::Vector4cd Network::abcd2s(const Eigen::Matrix2cd& abcd, double z0)
{
    std::complex<double> A = abcd(0, 0);
    std::complex<double> B = abcd(0, 1);
    std::complex<double> C = abcd(1, 0);
    std::complex<double> D = abcd(1, 1);

    std::complex<double> common_divisor = A + B / z0 + C * z0 + D;
    std::complex<double> s11 = (A + B / z0 - C * z0 - D) / common_divisor;
    std::complex<double> s12 = (2.0 * (A * D - B * C)) / common_divisor;
    std::complex<double> s21 = 2.0 / common_divisor;
    std::complex<double> s22 = (-A + B / z0 - C * z0 + D) / common_divisor;

    Eigen::Vector4cd s;
    s << s11, s12, s21, s22;
    return s;
}

QString Network::formatEngineering(double value)
{
    if (!std::isfinite(value))
        return QString::number(value);

    if (value == 0.0)
        return QStringLiteral("000.00e+00");

    double absValue = std::fabs(value);
    int exponent = static_cast<int>(std::floor(std::log10(absValue)));
    int exponentOffset = ((exponent % 3) + 3) % 3;
    int engineeringExponent = exponent - exponentOffset;
    double mantissa = absValue / std::pow(10.0, engineeringExponent);
    double roundedMantissa = std::round(mantissa * 100.0) / 100.0;

    if (roundedMantissa >= 1000.0)
    {
        roundedMantissa /= 1000.0;
        engineeringExponent += 3;
    }

    QString mantissaStr = QString::number(roundedMantissa, 'f', 2);
    int dotIndex = mantissaStr.indexOf('.');
    if (dotIndex < 0)
    {
        mantissaStr.append(".00");
        dotIndex = mantissaStr.indexOf('.');
    }

    int integerDigits = dotIndex;
    if (integerDigits < 3)
        mantissaStr.prepend(QString(3 - integerDigits, '0'));

    QString signStr = value < 0 ? QStringLiteral("-") : QString();
    QString exponentStr = QString::number(std::abs(engineeringExponent)).rightJustified(2, '0');

    return QStringLiteral("%1%2e%3%4")
        .arg(signStr, mantissaStr,
             engineeringExponent >= 0 ? QStringLiteral("+") : QStringLiteral("-"),
             exponentStr);
}

Network::Network(QObject *parent)
    : QObject(parent),
      m_fmin(0),
      m_fmax(0),
      m_color(Qt::black),
      m_is_visible(true),
      m_unwrap_phase(true),
      m_is_active(true)
{
}

double Network::fmin() const
{
    return m_fmin;
}

void Network::setFmin(double fmin)
{
    m_fmin = fmin;
}

double Network::fmax() const
{
    return m_fmax;
}

void Network::setFmax(double fmax)
{
    m_fmax = fmax;
}

QColor Network::color() const
{
    return m_color;
}

void Network::setColor(const QColor &color)
{
    m_color = color;
}

bool Network::isVisible() const
{
    return m_is_visible;
}

void Network::setVisible(bool visible)
{
    m_is_visible = visible;
}

bool Network::unwrapPhase() const
{
    return m_unwrap_phase;
}

void Network::setUnwrapPhase(bool unwrap)
{
    m_unwrap_phase = unwrap;
}

bool Network::isActive() const
{
    return m_is_active;
}

void Network::setActive(bool active)
{
    m_is_active = active;
}

Eigen::ArrayXd Network::unwrap(const Eigen::ArrayXd& phase)
{
    Eigen::ArrayXd unwrapped_phase = phase;
    for (int i = 1; i < phase.size(); ++i) {
        double diff = unwrapped_phase(i) - unwrapped_phase(i - 1);
        if (diff > M_PI) {
            for (int j = i; j < phase.size(); ++j) {
                unwrapped_phase(j) -= 2 * M_PI;
            }
        } else if (diff < -M_PI) {
            for (int j = i; j < phase.size(); ++j) {
                unwrapped_phase(j) += 2 * M_PI;
            }
        }
    }
    return unwrapped_phase;
}
