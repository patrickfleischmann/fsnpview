#include "networks.h"
#include <QFileInfo>
#include <iostream>
#include <math.h>

Networks::Networks(QObject *parent) : QObject(parent)
{
    m_colors.append(QColor::fromRgbF(0, 0.4470, 0.7410));
    m_colors.append(QColor::fromRgbF(0.8500, 0.3250, 0.0980));
    m_colors.append(QColor::fromRgbF(0.9290, 0.6940, 0.1250));
    m_colors.append(QColor::fromRgbF(0.4940, 0.1840, 0.5560));
    m_colors.append(QColor::fromRgbF(0.4660, 0.6740, 0.1880));
    m_colors.append(QColor::fromRgbF(0.3010, 0.7450, 0.9330));
    m_colors.append(QColor::fromRgbF(0.6350, 0.0780, 0.1840));
    m_colors.append(QColor::fromRgbF(0, 0, 1.0000));
    m_colors.append(QColor::fromRgbF(0, 0.5000, 0));
    m_colors.append(QColor::fromRgbF(1.0000, 0, 0));
    m_colors.append(QColor::fromRgbF(0, 0.7500, 0.7500));
    m_colors.append(QColor::fromRgbF(0.7500, 0, 0.7500));
    m_colors.append(QColor::fromRgbF(0.7500, 0.7500, 0));
    m_colors.append(QColor::fromRgbF(0.2500, 0.2500, 0.2500));
}

void Networks::addFile(const QString &filePath)
{
    try {
        std::string path = filePath.toStdString();
        auto data = std::make_unique<ts::TouchstoneData>(ts::parse_touchstone(path));
        QColor color = m_colors.at(m_parsed_data.size() % m_colors.size());
        m_file_colors[path] = color;
        m_parsed_data[path] = std::move(data);
    } catch (const std::exception& e) {
        std::cerr << "Error processing file " << filePath.toStdString() << ": " << e.what() << std::endl;
    }
}

void Networks::removeFile(const QString &filePath)
{
    m_parsed_data.erase(filePath.toStdString());
    m_file_colors.erase(filePath.toStdString());
}

QPair<QVector<double>, QVector<double>> Networks::getPlotData(const QString &filePath, int s_param_idx, bool isPhase)
{
    std::string path = filePath.toStdString();
    auto it = m_parsed_data.find(path);
    if (it == m_parsed_data.end()) {
        return QPair<QVector<double>, QVector<double>>();
    }

    const auto& data = it->second;
    Eigen::ArrayXd xValues = data->freq;
    Eigen::ArrayXd yValues;

    if (isPhase) {
        Eigen::ArrayXcd s_param_col = data->sparams.col(s_param_idx);
        Eigen::ArrayXd phase_rad = s_param_col.arg();
        Eigen::ArrayXd unwrapped_phase_rad = unwrap(phase_rad);
        yValues = unwrapped_phase_rad * (180.0 / M_PI);
    } else {
        yValues = data->sparams.col(s_param_idx).abs().log10() * 20;
    }

    QVector<double> xValuesQVector = QVector<double>(xValues.data(), xValues.data() + xValues.size());
    QVector<double> yValuesQVector = QVector<double>(yValues.data(), yValues.data() + yValues.size());

    return qMakePair(xValuesQVector, yValuesQVector);
}

QColor Networks::getFileColor(const QString &filePath)
{
    return m_file_colors.at(filePath.toStdString());
}

QString Networks::getFileName(const QString &filePath)
{
    return QFileInfo(filePath).fileName();
}

QVector<double> Networks::getFrequencies(const QString &filePath)
{
    std::string path = filePath.toStdString();
    auto it = m_parsed_data.find(path);
    if (it == m_parsed_data.end()) {
        return QVector<double>();
    }
    const auto& data = it->second;
    Eigen::ArrayXd xValues = data->freq;
    return QVector<double>(xValues.data(), xValues.data() + xValues.size());
}

int Networks::getSparamIndex(const QString &sparam)
{
    if (sparam == "s11") return 0;
    if (sparam == "s21") return 1;
    if (sparam == "s12") return 2;
    if (sparam == "s22") return 3;
    return -1;
}

QStringList Networks::getFilePaths() const
{
    QStringList paths;
    for (const auto& pair : m_parsed_data) {
        paths.append(QString::fromStdString(pair.first));
    }
    return paths;
}

Eigen::ArrayXd Networks::unwrap(const Eigen::ArrayXd& phase)
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
