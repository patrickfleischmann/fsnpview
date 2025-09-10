#ifndef NETWORKFILE_H
#define NETWORKFILE_H

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

class NetworkFile : public QObject
{
    Q_OBJECT
public:
    explicit NetworkFile(QObject *parent = nullptr);

    void addFile(const QString &filePath);
    void removeFile(const QString &filePath);

    QPair<QVector<double>, QVector<double>> getPlotData(const QString &filePath, int s_param_idx, bool isPhase);
    QColor getFileColor(const QString &filePath);
    QString getFileName(const QString &filePath);
    QVector<double> getFrequencies(const QString &filePath);
    int getSparamIndex(const QString &sparam);
    QStringList getFilePaths() const;

private:
    Eigen::ArrayXd unwrap(const Eigen::ArrayXd& phase);
    std::map<std::string, std::unique_ptr<ts::TouchstoneData>> m_parsed_data;
    std::map<std::string, QColor> m_file_colors;
    QList<QColor> m_colors;
};

#endif // NETWORKFILE_H
