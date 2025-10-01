#include "cascadeio.h"

#include "networkcascade.h"
#include "parser_touchstone.h"

#include <QFile>
#include <QFileInfo>
#include <QByteArray>

#include <exception>

bool saveCascadeToFile(const NetworkCascade& cascade,
                       const Eigen::VectorXd& freq,
                       QString path,
                       QString* savedAbsolutePath,
                       QString* errorMessage)
{
    if (freq.size() == 0) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Cannot save cascade: no frequency points available.");
        return false;
    }

    const int ports = cascade.portCount();
    if (ports <= 0) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Cannot save cascade: invalid port count.");
        return false;
    }

    ts::TouchstoneData data;
    data.ports = ports;
    data.parameter = "S";
    data.format = "RI";
    data.freq_unit = "HZ";
    data.R = 50.0;
    data.freq = freq;

    const Eigen::Index rows = freq.size();
    const Eigen::Index cols = static_cast<Eigen::Index>(ports * ports);
    data.sparams.resize(rows, cols);
    data.sparams.setZero();

    Eigen::MatrixXcd s_matrix = cascade.sparameters(freq);
    for (Eigen::Index row = 0; row < rows; ++row) {
        for (Eigen::Index col = 0; col < cols && col < s_matrix.cols(); ++col) {
            data.sparams(row, col) = s_matrix(row, col);
        }
    }

    if (!path.endsWith(QStringLiteral(".s2p"), Qt::CaseInsensitive)) {
        path += QStringLiteral(".s2p");
    }

    QFileInfo info(path);
    const QString absolutePath = info.absoluteFilePath();

    try {
        const QByteArray encoded = QFile::encodeName(absolutePath);
        ts::write_touchstone(data, std::string(encoded.constData()));
    } catch (const std::exception& ex) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Failed to save cascade: %1").arg(QString::fromLocal8Bit(ex.what()));
        return false;
    }

    if (savedAbsolutePath)
        *savedAbsolutePath = absolutePath;

    if (errorMessage)
        errorMessage->clear();

    return true;
}
