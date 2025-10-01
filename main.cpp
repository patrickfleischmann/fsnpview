#include "mainwindow.h"
#include "commandlineparser.h"
#include "networkcascade.h"
#include "networkfile.h"
#include "networklumped.h"
#include "cascadeio.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDataStream>
#include <QFileInfo>
#include <QLocalSocket>
#include <QSet>

#include <Eigen/Dense>

#include <algorithm>
#include <exception>
#include <iostream>
#include <memory>
#include <vector>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

QString resolvePath(const QString& path)
{
    QFileInfo info(path);
    if (info.isAbsolute())
        return info.filePath();
    return info.absoluteFilePath();
}

std::unique_ptr<Network> createNetworkForCascade(const CommandLineParser::CascadeEntry& entry, QString* error)
{
    if (entry.type == CommandLineParser::CascadeEntry::Type::File) {
        const QString resolvedPath = resolvePath(entry.identifier);
        auto network = std::make_unique<NetworkFile>(resolvedPath);
        if (network->portCount() <= 0) {
            if (error) {
                *error = QStringLiteral("Failed to load network file '%1'").arg(resolvedPath);
            }
            return nullptr;
        }
        network->setVisible(true);
        network->setActive(true);
        return network;
    }

    auto network = std::make_unique<NetworkLumped>(entry.lumpedType);
    for (const auto& overrideValue : entry.parameterOverrides) {
        if (overrideValue.index < 0 || overrideValue.index >= network->parameterCount()) {
            if (error) {
                *error = QStringLiteral("Invalid parameter index %1 for lumped network").arg(overrideValue.index);
            }
            return nullptr;
        }
        network->setParameterValue(overrideValue.index, overrideValue.value);
    }
    return network;
}

Eigen::VectorXd buildFrequencyVector(const CommandLineParser::Options& options, const NetworkCascade& cascade)
{
    if (options.freqSpecified) {
        return Eigen::VectorXd::LinSpaced(options.freqPoints, options.fmin, options.fmax);
    }
    const double fmin = cascade.fmin();
    const double fmax = cascade.fmax();
    const int points = std::max(cascade.pointCount(), 2);
    if (fmax <= fmin) {
        return Eigen::VectorXd::LinSpaced(points, 1e6, 10e9);
    }
    return Eigen::VectorXd::LinSpaced(points, fmin, fmax);
}

int runNoGui(const CommandLineParser::Options& options)
{
    for (const QString& file : options.files) {
        const QString resolved = resolvePath(file);
        NetworkFile network(resolved);
        if (network.portCount() <= 0) {
            std::cerr << "Failed to load file '" << resolved.toStdString() << "'." << std::endl;
            return 1;
        }
        std::cout << "Loaded file \"" << resolved.toStdString() << "\"." << std::endl;
    }

    if (!options.cascadeRequested) {
        if (options.saveRequested) {
            std::cerr << "No cascade specified; nothing to save." << std::endl;
            return 1;
        }
        return 0;
    }

    NetworkCascade cascade;
    if (options.freqSpecified) {
        cascade.setFrequencyRange(options.fmin, options.fmax);
        cascade.setPointCount(options.freqPoints);
    }

    std::vector<std::unique_ptr<Network>> cascadeNetworks;
    cascadeNetworks.reserve(options.cascade.size());
    for (const auto& entry : options.cascade) {
        QString error;
        auto network = createNetworkForCascade(entry, &error);
        if (!network) {
            if (!error.isEmpty()) {
                std::cerr << error.toStdString() << std::endl;
            }
            return 1;
        }
        cascadeNetworks.push_back(std::move(network));
        cascade.addNetwork(cascadeNetworks.back().get());
    }

    if (cascade.getNetworks().isEmpty()) {
        std::cerr << "Cascade is empty; nothing to process." << std::endl;
        return 1;
    }

    Eigen::VectorXd freq = buildFrequencyVector(options, cascade);

    int exitCode = 0;
    if (options.saveRequested) {
        QString savedPath;
        QString errorMessage;
        if (saveCascadeToFile(cascade, freq, options.savePath, &savedPath, &errorMessage)) {
            std::cout << "Cascade saved to \"" << savedPath.toStdString() << "\"" << std::endl;
        } else {
            if (!errorMessage.isEmpty())
                std::cerr << errorMessage.toStdString() << std::endl;
            exitCode = 1;
        }
    } else {
        std::cout << "Cascade configured with " << cascade.getNetworks().size()
                  << " network(s)." << std::endl;
    }

    cascade.clearNetworks();
    return exitCode;
}

QStringList collectFilesToOpen(const CommandLineParser::Options& options)
{
    QStringList files = options.files;
    QSet<QString> seen;
    for (const QString& file : files) {
        seen.insert(resolvePath(file));
    }
    for (const auto& entry : options.cascade) {
        if (entry.type != CommandLineParser::CascadeEntry::Type::File)
            continue;
        const QString resolved = resolvePath(entry.identifier);
        if (!seen.contains(resolved)) {
            seen.insert(resolved);
            files.append(entry.identifier);
        }
    }
    return files;
}

bool configureCascadeForWindow(MainWindow& window, const CommandLineParser::Options& options)
{
    if (!options.cascadeRequested) {
        window.clearCascade();
        return true;
    }

    window.clearCascade();
    for (const auto& entry : options.cascade) {
        QString error;
        auto network = createNetworkForCascade(entry, &error);
        if (!network) {
            if (!error.isEmpty()) {
                std::cerr << error.toStdString() << std::endl;
            }
            return false;
        }
        Network* raw = network.release();
        window.addNetworkToCascade(raw);
    }

    if (options.freqSpecified) {
        window.setCascadeFrequencyRange(options.fmin, options.fmax);
        window.setCascadePointCount(options.freqPoints);
    }

    return true;
}

} // namespace

int main(int argc, char *argv[])
{
    std::cout << "fsnpview start" << std::endl;

    CommandLineParser parser;
    CommandLineParser::ParseResult parseResult = parser.parse(argc, argv);
    if (!parseResult.ok) {
        std::cerr << parseResult.errorMessage.toStdString() << std::endl;
        std::cout << parser.helpText().toStdString();
        return 1;
    }

    const CommandLineParser::Options& options = parseResult.options;
    if (options.helpRequested) {
        std::cout << parser.helpText().toStdString();
        return 0;
    }

    if (options.noGui) {
        QCoreApplication app(argc, argv);
        return runNoGui(options);
    }

#ifdef Q_OS_WIN
    HANDLE hMutex = CreateMutexA(NULL, TRUE, "MutexFsnpview");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        const QString serverName = QStringLiteral("fsnpview-server");
        QLocalSocket socket;
        socket.connectToServer(serverName);

        if (socket.waitForConnected(500)) {
            std::cout << "Sending arguments to first instance" << std::endl;
            QDataStream stream(&socket);
            QStringList filesToOpen = collectFilesToOpen(options);
            stream << filesToOpen;
            if (socket.waitForBytesWritten()) {
                socket.flush();
                socket.waitForDisconnected();
            }
        }
        return 0;
    }
#else
    const QString serverName = QStringLiteral("fsnpview-server");
    QLocalSocket socket;
    socket.connectToServer(serverName);

    if (socket.waitForConnected(500)) {
        std::cout << "Sending arguments to first instance" << std::endl;
        QDataStream stream(&socket);
        QStringList filesToOpen = collectFilesToOpen(options);
        stream << filesToOpen;
        if (socket.waitForBytesWritten()) {
            socket.flush();
            socket.waitForDisconnected();
        }
        return 0;
    }
#endif

    QApplication app(argc, argv);
    MainWindow window;
    window.show();

    QStringList filesToOpen = collectFilesToOpen(options);
    if (!filesToOpen.isEmpty()) {
        window.processFiles(filesToOpen, true);
    }

    window.initializeFrequencyControls(options.freqSpecified,
                                       options.fmin,
                                       options.fmax,
                                       options.freqPoints,
                                       !filesToOpen.isEmpty());

    if (!configureCascadeForWindow(window, options)) {
#ifdef Q_OS_WIN
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
#endif
        return 1;
    }

    bool saveSuccess = true;
    if (options.saveRequested) {
        if (!window.cascade()->getNetworks().isEmpty()) {
            Eigen::VectorXd freq = buildFrequencyVector(options, *window.cascade());
            QString savedPath;
            QString errorMessage;
            saveSuccess = saveCascadeToFile(*window.cascade(), freq, options.savePath, &savedPath, &errorMessage);
            if (saveSuccess) {
                std::cout << "Cascade saved to \"" << savedPath.toStdString() << "\"" << std::endl;
            } else if (!errorMessage.isEmpty()) {
                std::cerr << errorMessage.toStdString() << std::endl;
            }
        } else {
            std::cerr << "Cannot save cascade: no networks configured." << std::endl;
            saveSuccess = false;
        }
    }

    int result = app.exec();

#ifdef Q_OS_WIN
    ReleaseMutex(hMutex);
    CloseHandle(hMutex);
#endif

    if (!saveSuccess)
        return 1;
    return result;
}

