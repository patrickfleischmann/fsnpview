#include <QApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include "qcustomplot.h"
#include "plotmanager.h"
#include "networkfile.h"
#include "networkcascade.h"
#include "networklumped.h"
#include <fstream>
#include <iostream>
#include <cstdlib>

// Helper function to dump graph data to stream
static void writeGraphData(QCPGraph *graph, std::ostream &out)
{
    const auto data = graph->data();
    for (auto it = data->constBegin(); it != data->constEnd(); ++it) {
        out << it->key << "," << it->value << "\n";
    }
}

static std::vector<std::pair<double, double>> readBaseline(const std::string &path)
{
    std::ifstream in(path);
    std::vector<std::pair<double, double>> result;
    if (!in.is_open()) return result;
    double x, y; char comma;
    while (in >> x >> comma >> y) {
        result.emplace_back(x, y);
    }
    return result;
}

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
    QApplication app(argc, argv);

    QCustomPlot plot;
    PlotManager pm(&plot);

    NetworkFile net(QStringLiteral("test/a (1).s2p"));
    net.setVisible(true);
    QList<Network*> nets{&net};
    pm.setNetworks(nets);
    pm.setCascade(nullptr);
    pm.updatePlots(QStringList{QStringLiteral("s21")}, PlotType::Magnitude);

    if (plot.graphCount() == 0) {
        std::cerr << "No graph produced" << std::endl;
        return 1;
    }

    const char *baselinePath = "tests/gui_plot_baseline.csv";
    auto baseline = readBaseline(baselinePath);

    if (std::getenv("UPDATE_BASELINE")) {
        std::ofstream out(baselinePath);
        writeGraphData(plot.graph(0), out);
        std::cout << "Baseline updated" << std::endl;
        return 0;
    }

    // Compare with baseline
    std::vector<std::pair<double, double>> current;
    const auto data = plot.graph(0)->data();
    for (auto it = data->constBegin(); it != data->constEnd(); ++it) {
        current.emplace_back(it->key, it->value);
    }

    if (baseline.size() != current.size()) {
        std::cerr << "Baseline size mismatch" << std::endl;
        return 1;
    }

    const double tol = 1e-6;
    for (size_t i = 0; i < current.size(); ++i) {
        double dx = std::abs(current[i].first - baseline[i].first);
        double dy = std::abs(current[i].second - baseline[i].second);
        if (dx > tol || dy > tol) {
            std::cerr << "Data mismatch at index " << i << std::endl;
            return 1;
        }
    }

    {
        QCustomPlot cascadePlot;
        PlotManager cascadeManager(&cascadePlot);

        NetworkCascade cascade;
        cascade.setColor(Qt::cyan);
        NetworkLumped section(NetworkLumped::NetworkType::R_series, {50.0});
        cascade.addNetwork(&section);

        cascadeManager.setNetworks(QList<Network*>());
        cascadeManager.setCascade(&cascade);
        cascadeManager.updatePlots(QStringList{QStringLiteral("s21")}, PlotType::Magnitude);

        if (cascadePlot.graphCount() == 0) {
            std::cerr << "Cascade plot missing" << std::endl;
            return 1;
        }

        bool cascadeGraphFound = false;
        for (int i = 0; i < cascadePlot.graphCount(); ++i) {
            QCPGraph *graph = cascadePlot.graph(i);
            if (!graph)
                continue;
            quintptr ptrVal = graph->property("network_ptr").value<quintptr>();
            if (reinterpret_cast<Network*>(ptrVal) == &cascade) {
                cascadeGraphFound = true;
                if (graph->pen().color() != cascade.color()) {
                    std::cerr << "Cascade graph color mismatch" << std::endl;
                    return 1;
                }
            }
        }

        if (!cascadeGraphFound) {
            std::cerr << "Cascade graph not found" << std::endl;
            return 1;
        }
    }

    {
        QTemporaryDir temp;
        QDir dir(temp.path());
        const QString firstPath = dir.filePath(QStringLiteral("first/sample.s1p"));
        const QString secondPath = dir.filePath(QStringLiteral("second/sample.s1p"));
        auto writeTouchstone = [](const QString &path, const QByteArray &data) {
            QFile file(path);
            return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
        };
        if (!temp.isValid() || !dir.mkdir(QStringLiteral("first")) ||
            !dir.mkdir(QStringLiteral("second")) ||
            !writeTouchstone(firstPath, "# GHz S RI R 50\n1 1 0\n") ||
            !writeTouchstone(secondPath, "# GHz S RI R 50\n1 0.1 0\n")) {
            std::cerr << "Could not create same-named Touchstone files" << std::endl;
            return 1;
        }

        NetworkFile first(firstPath);
        NetworkFile second(secondPath);
        first.setVisible(true);
        second.setVisible(true);
        QCustomPlot duplicatePlot;
        PlotManager duplicateManager(&duplicatePlot);
        duplicateManager.setNetworks({&first, &second});
        const QStringList parameters{QStringLiteral("s11")};
        duplicateManager.updatePlots(parameters, PlotType::Magnitude);

        auto checkTrace = [](QCPGraph *graph, const NetworkFile *source, double expectedDb) {
            return graph->property("network_ptr").value<quintptr>() ==
                       reinterpret_cast<quintptr>(source) &&
                   graph->data()->size() == 1 &&
                   std::abs(graph->data()->constBegin()->value - expectedDb) < 1e-6;
        };
        if (duplicatePlot.graphCount() != 2 ||
            !checkTrace(duplicatePlot.graph(0), &first, 0.0) ||
            !checkTrace(duplicatePlot.graph(1), &second, -20.0)) {
            std::cerr << "Same-named files should produce two distinct traces" << std::endl;
            return 1;
        }

        second.setVisible(false);
        duplicateManager.updatePlots(parameters, PlotType::Magnitude);
        if (duplicatePlot.graphCount() != 1 || !checkTrace(duplicatePlot.graph(0), &first, 0.0)) {
            std::cerr << "Hiding one file should leave the other trace intact" << std::endl;
            return 1;
        }
    }

    std::cout << "GUI plot test passed" << std::endl;
    return 0;
}

