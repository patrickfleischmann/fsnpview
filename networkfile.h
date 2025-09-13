#ifndef NETWORKFILE_H
#define NETWORKFILE_H

#include "network.h"
#include "parser_touchstone.h"
#include <memory>

class NetworkFile : public Network
{
    Q_OBJECT
public:
    explicit NetworkFile(const QString &filePath, QObject *parent = nullptr);

    QString name() const override;
    Eigen::MatrixXcd abcd(const Eigen::VectorXd& freq) const override;
    QPair<QVector<double>, QVector<double>> getPlotData(int s_param_idx, PlotType type) override;

    QString filePath() const;

private:
    std::complex<double> interpolate_s_param(double freq, int s_param_idx) const;

    QString m_file_path;
    std::unique_ptr<ts::TouchstoneData> m_data;
};

#endif // NETWORKFILE_H
