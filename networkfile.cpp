#include "networkfile.h"
#include "tdrcalculator.h"
#include <QFileInfo>
#include <iostream>
#include <numeric>
#include <algorithm>
#include <optional>

NetworkFile::NetworkFile(const QString &filePath, QObject *parent)
    : Network(parent), m_file_path(filePath)
{
    try {
        m_data = std::make_unique<ts::TouchstoneData>(ts::parse_touchstone(filePath.toStdString()));
        if (m_data->freq.size() > 0) {
            m_fmin = m_data->freq.minCoeff();
            m_fmax = m_data->freq.maxCoeff();
        }
    } catch (const std::exception& e) {
        std::cerr << "Error processing file " << filePath.toStdString() << ": " << e.what() << std::endl;
        m_data = nullptr;
    }
}

QString NetworkFile::name() const
{
    return QFileInfo(m_file_path).fileName();
}

QString NetworkFile::filePath() const
{
    return m_file_path;
}

Network* NetworkFile::clone(QObject* parent) const
{
    NetworkFile* copy = new NetworkFile(m_file_path, parent);
    copy->setColor(m_color);
    copy->setVisible(m_is_visible);
    copy->setUnwrapPhase(m_unwrap_phase);
    copy->setActive(m_is_active);
    copy->setFmin(m_fmin);
    copy->setFmax(m_fmax);
    return copy;
}

QPair<QVector<double>, QVector<double>> NetworkFile::getPlotData(int s_param_idx, PlotType type)
{
    if (!m_data || s_param_idx < 0 || s_param_idx >= m_data->sparams.cols()) {
        return {};
    }

    const int ports = m_data->ports;
    if (ports <= 0) {
        return {};
    }

    const int outputPortIndex = s_param_idx % ports;
    const int inputPortIndex = s_param_idx / ports;
    const bool isReflection = (outputPortIndex == inputPortIndex);

    Eigen::ArrayXd xValues;
    Eigen::ArrayXd yValues;

    Eigen::ArrayXcd s_param_col = m_data->sparams.col(s_param_idx);

    // Renormalize to 50 Ohm when needed
    if (type == PlotType::VSWR || type == PlotType::Smith || type == PlotType::TDR) {
        double R = m_data->R;
        Eigen::ArrayXcd z = R * (1.0 + s_param_col) / (1.0 - s_param_col);
        s_param_col = (z - 50.0) / (z + 50.0);
    }

    Network::TimeGateSettings gateSettings = Network::timeGateSettings();
    const bool isReflectionParam = isReflection;
    TDRCalculator calculator;
    TDRCalculator::Parameters tdrParams;
    tdrParams.effectivePermittivity = std::max(gateSettings.epsilonR, 1.0);

    std::optional<TDRCalculator::GateResult> gateResult;
    if (gateSettings.enabled && isReflectionParam) {
        auto gated = calculator.applyGate(m_data->freq, s_param_col,
                                         gateSettings.startDistance,
                                         gateSettings.stopDistance,
                                         gateSettings.epsilonR,
                                         tdrParams);
        if (gated) {
            s_param_col = gated->gatedReflection;
            gateResult = std::move(*gated);
        }
    }

    switch (type) {
    case PlotType::Magnitude:
        xValues = m_data->freq;
        yValues = 20 * s_param_col.abs().log10();
        break;
    case PlotType::Phase:
    {
        xValues = m_data->freq;
        Eigen::ArrayXd phase_rad = s_param_col.arg();
        Eigen::ArrayXd unwrapped_phase_rad = m_unwrap_phase ? unwrap(phase_rad) : phase_rad;
        yValues = unwrapped_phase_rad * (180.0 / M_PI);
        break;
    }
    case PlotType::VSWR:
        xValues = m_data->freq;
        yValues = (1 + s_param_col.abs()) / (1 - s_param_col.abs());
        break;
    case PlotType::Smith:
        xValues = s_param_col.real();
        yValues = s_param_col.imag();
        break;
    case PlotType::TDR:
        if (!isReflectionParam)
            return {};
        if (gateResult)
            return qMakePair(gateResult->distance, gateResult->impedance);
        {
            auto result = calculator.compute(m_data->freq, s_param_col, tdrParams);
            return qMakePair(result.distance, result.impedance);
        }
    }

    QVector<double> xValuesQVector = QVector<double>(xValues.data(), xValues.data() + xValues.size());
    QVector<double> yValuesQVector = QVector<double>(yValues.data(), yValues.data() + yValues.size());

    return qMakePair(xValuesQVector, yValuesQVector);
}

std::complex<double> NetworkFile::interpolate_s_param(double freq, int s_param_idx) const
{
    if (!m_data) {
        return {0, 0};
    }

    if (s_param_idx < 0 || s_param_idx >= m_data->sparams.cols()) {
        return {0, 0};
    }

    const auto& freqs = m_data->freq;
    if (freqs.size() == 0) {
        return {0, 0};
    }

    const auto& sparams = m_data->sparams.col(s_param_idx);

    const auto last_idx = freqs.size() - 1;
    if (freq <= freqs[0]) {
        return sparams[0];
    }
    if (freq >= freqs[last_idx]) {
        return sparams[last_idx];
    }

    auto it = std::lower_bound(freqs.data(), freqs.data() + freqs.size(), freq);
    if (it == freqs.data()) {
        it++;
    }
    int upper_idx = std::distance(freqs.data(), it);
    int lower_idx = upper_idx - 1;

    double f1 = freqs[lower_idx];
    double f2 = freqs[upper_idx];

    if (f2 == f1) {
        return sparams[lower_idx];
    }

    std::complex<double> s1 = sparams[lower_idx];
    std::complex<double> s2 = sparams[upper_idx];

    double mag1 = std::abs(s1);
    double mag2 = std::abs(s2);
    double phase1 = std::arg(s1);
    double phase2 = std::arg(s2);

    // unwrap phase
    if (phase2 - phase1 > M_PI) {
        phase2 -= 2 * M_PI;
    } else if (phase1 - phase2 > M_PI) {
        phase1 -= 2 * M_PI;
    }

    double t = (freq - f1) / (f2 - f1);
    double interpolated_mag = mag1 + t * (mag2 - mag1);
    double interpolated_phase = phase1 + t * (phase2 - phase1);

    return std::polar(interpolated_mag, interpolated_phase);
}

QVector<double> NetworkFile::frequencies() const
{
    if (!m_data)
        return {};
    return QVector<double>(m_data->freq.data(), m_data->freq.data() + m_data->freq.size());
}

int NetworkFile::portCount() const
{
    if (!m_data)
        return 0;
    return m_data->ports;
}

Eigen::MatrixXcd NetworkFile::sparameters(const Eigen::VectorXd& freq) const
{
    if (!m_data || freq.size() == 0) {
        return {};
    }

    const int ports = m_data->ports;
    if (ports != 2) {
        return {};
    }

    Eigen::MatrixXcd s_matrix(freq.size(), 4);
    for (int i = 0; i < freq.size(); ++i) {
        std::complex<double> s11 = interpolate_s_param(freq(i), 0);
        std::complex<double> s12 = interpolate_s_param(freq(i), 1);
        std::complex<double> s21 = interpolate_s_param(freq(i), 2);
        std::complex<double> s22 = interpolate_s_param(freq(i), 3);
        s_matrix.row(i) << s11, s12, s21, s22;
    }

    return s_matrix;
}
