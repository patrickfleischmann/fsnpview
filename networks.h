#ifndef NETWORKS_H
#define NETWORKS_H

#include "parser_touchstone.h"
#include <QObject>
#include <QString>
#include <QVector>
#include <QColor>
#include <QPair>
#include <map>
#include <string>
#include <memory>
#include <Eigen/Dense>

class Networks : public QObject
{
    Q_OBJECT
public:
    explicit Networks(QObject *parent = nullptr);

    void addFile(const QString &filePath);
    void removeFile(const QString &filePath);
    void addMathNetwork(const QString &name, const QVector<double> &freq, const Eigen::ArrayXcd &data);

    QPair<QVector<double>, QVector<double>> getPlotData(const QString &name, int s_param_idx, bool isPhase);
    QPair<QVector<double>, Eigen::ArrayXcd> getComplexSparamData(const QString &name, int s_param_idx);
    QColor getFileColor(const QString &name);
    QString getFileName(const QString &name);
    QVector<double> getFrequencies(const QString &name);
    int getSparamIndex(const QString &sparam);
    QStringList getFilePaths() const;
    QStringList getMathNetworkNames() const;
    bool isMathNetwork(const QString &name) const;

private:
    struct MathNetwork {
        QVector<double> freq;
        Eigen::ArrayXcd data;
    };
    Eigen::ArrayXd unwrap(const Eigen::ArrayXd& phase);
    std::map<std::string, std::unique_ptr<ts::TouchstoneData>> m_parsed_data;
    std::map<std::string, MathNetwork> m_math_data;
    std::map<std::string, QColor> m_file_colors;
    QList<QColor> m_colors;
};

#endif // NETWORKS_H
