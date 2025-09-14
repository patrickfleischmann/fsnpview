#include "networkfile.h"
#include <QFileInfo>
#include <iostream>
#include <numeric>

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

QPair<QVector<double>, QVector<double>> NetworkFile::getPlotData(int s_param_idx, PlotType type)
{
    if (!m_data || s_param_idx < 0 || s_param_idx >= m_data->sparams.cols()) {
        return {};
    }

    Eigen::ArrayXd xValues;
    Eigen::ArrayXd yValues;

    Eigen::ArrayXcd s_param_col = m_data->sparams.col(s_param_idx);

    // Renormalize to 50 Ohm when needed
    if (type == PlotType::VSWR || type == PlotType::Smith) {
        double R = m_data->R;
        Eigen::ArrayXcd z = R * (1.0 + s_param_col) / (1.0 - s_param_col);
        s_param_col = (z - 50.0) / (z + 50.0);
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

    const auto& freqs = m_data->freq;
    const auto& sparams = m_data->sparams.col(s_param_idx);

    if (freq < freqs.minCoeff() || freq > freqs.maxCoeff()) {
        return {0, 0}; // or some other handling for out of range
    }

    auto it = std::lower_bound(freqs.data(), freqs.data() + freqs.size(), freq);
    if (it == freqs.data()) {
        it++;
    }
    int upper_idx = std::distance(freqs.data(), it);
    int lower_idx = upper_idx - 1;

    double f1 = freqs[lower_idx];
    double f2 = freqs[upper_idx];

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

Eigen::MatrixXcd NetworkFile::abcd(const Eigen::VectorXd& freq) const
{
    if (!m_data) {
        return {};
    }

    Eigen::MatrixXcd abcd_matrix(freq.size(), 4);
    for (int i = 0; i < freq.size(); ++i) {
        std::complex<double> s11 = interpolate_s_param(freq(i), 0);
        std::complex<double> s12 = interpolate_s_param(freq(i), 1);
        std::complex<double> s21 = interpolate_s_param(freq(i), 2);
        std::complex<double> s22 = interpolate_s_param(freq(i), 3);
        Eigen::Matrix2cd abcd_point = s2abcd(s11, s12, s21, s22);
        abcd_matrix.row(i) << abcd_point(0, 0), abcd_point(0, 1), abcd_point(1, 0), abcd_point(1, 1);
    }

    return abcd_matrix;
}
