#include "network.h"
#include <complex>
#include <cmath>
#include <limits>
#include <algorithm>
#include <utility>

namespace {
Network::TimeGateSettings g_timeGateSettings;
}

namespace
{
    int clampWidth(int width)
    {
        return std::clamp(width, 0, 10);
    }
}

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

QString Network::formatEngineering(double value, bool padMantissa)
{
    auto applyPadding = [padMantissa](const QString& text) {
        if (!padMantissa)
            return text;
        constexpr int fixedWidth = 12;
        if (text.length() >= fixedWidth)
            return text;
        return text.rightJustified(fixedWidth, QLatin1Char(' '));
    };

    if (!std::isfinite(value))
        return applyPadding(QString::number(value));

    if (value == 0.0)
        return applyPadding(QStringLiteral("0"));

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
    if (mantissaStr.contains('.'))
    {
        while (mantissaStr.endsWith('0'))
            mantissaStr.chop(1);
        if (mantissaStr.endsWith('.'))
            mantissaStr.chop(1);
    }

    if (mantissaStr.isEmpty())
        mantissaStr = QStringLiteral("0");

    QString mantissaWithSign;
    if (value < 0 && mantissaStr != QStringLiteral("0"))
        mantissaWithSign = QStringLiteral("-") + mantissaStr;
    else
        mantissaWithSign = mantissaStr;

    if (mantissaWithSign == QStringLiteral("-0"))
        mantissaWithSign = QStringLiteral("0");

    if (engineeringExponent == 0 || mantissaStr == QStringLiteral("0"))
        return applyPadding(mantissaWithSign);

    return applyPadding(mantissaWithSign + QStringLiteral("e%1").arg(engineeringExponent));
}

QString Network::displayName() const
{
    return name();
}

void Network::setTimeGateSettings(const TimeGateSettings& settings)
{
    g_timeGateSettings = settings;
}

Network::TimeGateSettings Network::timeGateSettings()
{
    return g_timeGateSettings;
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

QStringList Network::parameterNames() const
{
    QStringList names;
    const int ports = portCount();
    if (ports <= 0)
        return names;

    for (int input = 1; input <= ports; ++input)
    {
        for (int output = 1; output <= ports; ++output)
            names.append(QStringLiteral("s%1%2").arg(output).arg(input));
    }

    return names;
}

QString Network::normalizedParameterKey(const QString& parameter)
{
    QString key = parameter;
    key = key.trimmed().toLower();
    return key;
}

void Network::updateOrRemovePenSettings(const QString& key, PenSettings&& settings)
{
    if (key.isEmpty())
        return;

    if (!settings.color.has_value() && !settings.width.has_value() && !settings.style.has_value())
    {
        m_parameterPenSettings.remove(key);
        return;
    }

    m_parameterPenSettings.insert(key, std::move(settings));
}

QColor Network::parameterColor(const QString& parameter) const
{
    const QString key = normalizedParameterKey(parameter);
    auto it = m_parameterPenSettings.constFind(key);
    if (it != m_parameterPenSettings.constEnd() && it->color.has_value())
        return *(it->color);
    return m_color;
}

void Network::setParameterColor(const QString& parameter, const QColor& color)
{
    const QString key = normalizedParameterKey(parameter);
    if (key.isEmpty())
        return;

    PenSettings settings = m_parameterPenSettings.value(key);
    if (!color.isValid() || color == m_color)
        settings.color.reset();
    else
        settings.color = color;

    updateOrRemovePenSettings(key, std::move(settings));
}

int Network::parameterWidth(const QString& parameter) const
{
    const QString key = normalizedParameterKey(parameter);
    auto it = m_parameterPenSettings.constFind(key);
    if (it != m_parameterPenSettings.constEnd() && it->width.has_value())
        return clampWidth(*(it->width));
    return 0;
}

void Network::setParameterWidth(const QString& parameter, int width)
{
    const QString key = normalizedParameterKey(parameter);
    if (key.isEmpty())
        return;

    PenSettings settings = m_parameterPenSettings.value(key);
    int clamped = clampWidth(width);
    if (clamped <= 0)
        settings.width.reset();
    else
        settings.width = clamped;

    updateOrRemovePenSettings(key, std::move(settings));
}

Qt::PenStyle Network::defaultPenStyleForParameter(const QString& parameter) const
{
    const QString key = normalizedParameterKey(parameter);
    if (key == QLatin1String("s11"))
        return Qt::DashLine;
    return Qt::SolidLine;
}

Qt::PenStyle Network::parameterStyle(const QString& parameter) const
{
    const QString key = normalizedParameterKey(parameter);
    auto it = m_parameterPenSettings.constFind(key);
    if (it != m_parameterPenSettings.constEnd() && it->style.has_value())
        return *(it->style);
    return defaultPenStyleForParameter(parameter);
}

void Network::setParameterStyle(const QString& parameter, Qt::PenStyle style)
{
    const QString key = normalizedParameterKey(parameter);
    if (key.isEmpty())
        return;

    PenSettings settings = m_parameterPenSettings.value(key);
    if (style == defaultPenStyleForParameter(parameter))
        settings.style.reset();
    else
        settings.style = style;

    updateOrRemovePenSettings(key, std::move(settings));
}

QPen Network::parameterPen(const QString& parameter) const
{
    const QColor color = parameterColor(parameter);
    const int width = parameterWidth(parameter);
    const Qt::PenStyle style = parameterStyle(parameter);
    QPen pen(color, width, style);
    return pen;
}

void Network::copyStyleSettingsFrom(const Network* other)
{
    if (!other)
        return;
    m_parameterPenSettings = other->m_parameterPenSettings;
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

Eigen::ArrayXd Network::computeGroupDelay(const Eigen::ArrayXd& phase_rad, const Eigen::ArrayXd& freq_hz)
{
    const int count = std::min<int>(phase_rad.size(), freq_hz.size());
    if (count <= 0)
        return {};

    Eigen::ArrayXd delay(count);
    delay.setZero();

    if (count == 1)
        return delay;

    for (int i = 0; i < count; ++i) {
        const int prevIndex = (i == 0) ? i : i - 1;
        const int nextIndex = (i == count - 1) ? i : i + 1;
        const double df = freq_hz(nextIndex) - freq_hz(prevIndex);
        if (nextIndex == prevIndex || std::abs(df) < std::numeric_limits<double>::epsilon()) {
            delay(i) = 0.0;
            continue;
        }

        const double dphi = phase_rad(nextIndex) - phase_rad(prevIndex);
        delay(i) = -(dphi / df) / (2.0 * M_PI);
    }

    return delay;
}

Eigen::ArrayXd Network::wrapToMinusPiPi(const Eigen::ArrayXd& phase_rad)
{
    Eigen::ArrayXd wrapped = phase_rad;
    const double pi = M_PI;
    const double twoPi = 2.0 * M_PI;
    constexpr double tolerance = 1e-12;
    for (int i = 0; i < wrapped.size(); ++i) {
        double value = wrapped(i);
        if (!std::isfinite(value))
            continue;
        while (value < -pi - tolerance)
            value += twoPi;
        while (value > pi + tolerance)
            value -= twoPi;
        if (value >= pi - tolerance)
            value -= twoPi;
        wrapped(i) = value;
    }
    return wrapped;
}
