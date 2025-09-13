#include <QApplication>
#include "qcustomplot.h"
#include "plotmanager.h"
#include "networkfile.h"
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

    std::cout << "GUI plot test passed" << std::endl;
    return 0;
}

