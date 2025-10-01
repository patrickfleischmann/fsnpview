#include "cascadeio.h"
#include "networkcascade.h"
#include "networkfile.h"
#include "parser_touchstone.h"

#include <QTemporaryDir>
#include <QFileInfo>

#include <cassert>
#include <cmath>
#include <iostream>

int main()
{
    NetworkFile net(QStringLiteral("test/a (1).s2p"));
    if (net.portCount() <= 0) {
        std::cerr << "Failed to load network fixture" << std::endl;
        return 1;
    }

    net.setVisible(true);
    net.setActive(true);

    NetworkCascade cascade;
    cascade.addNetwork(&net);

    Eigen::VectorXd freq = Eigen::VectorXd::LinSpaced(5, net.fmin(), net.fmax());

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        std::cerr << "Failed to create temporary directory" << std::endl;
        return 1;
    }

    QString outputPath = tempDir.path() + QStringLiteral("/result");
    QString savedPath;
    QString errorMessage;
    if (!saveCascadeToFile(cascade, freq, outputPath, &savedPath, &errorMessage)) {
        std::cerr << errorMessage.toStdString() << std::endl;
        return 1;
    }

    if (!QFileInfo::exists(savedPath)) {
        std::cerr << "Expected saved file to exist" << std::endl;
        return 1;
    }

    ts::TouchstoneData data = ts::parse_touchstone(savedPath.toStdString());
    if (data.ports != cascade.portCount()) {
        std::cerr << "Port count mismatch" << std::endl;
        return 1;
    }

    if (data.freq.size() != freq.size()) {
        std::cerr << "Frequency vector size mismatch" << std::endl;
        return 1;
    }

    for (Eigen::Index i = 0; i < freq.size(); ++i) {
        if (std::abs(data.freq(i) - freq(i)) > 1e-6) {
            std::cerr << "Frequency mismatch at index " << i << std::endl;
            return 1;
        }
    }

    QString failureError;
    if (saveCascadeToFile(cascade, Eigen::VectorXd(), outputPath, nullptr, &failureError)) {
        std::cerr << "Expected failure when saving without frequency vector" << std::endl;
        return 1;
    }

    if (failureError.isEmpty()) {
        std::cerr << "Expected an error message for failed save" << std::endl;
        return 1;
    }

    cascade.clearNetworks();
    return 0;
}
