#include <QApplication>
#include <QStringList>
#include "plotmanager.h"
#include "qcustomplot.h"

#include <cmath>
#include <iostream>

class DummyPlotNetwork : public Network
{
public:
    DummyPlotNetwork()
        : Network(nullptr)
    {
        setVisible(true);
        setColor(Qt::green);
    }

    QString name() const override { return QStringLiteral("dummy_network"); }

    Eigen::MatrixXcd sparameters(const Eigen::VectorXd& freq) const override
    {
        (void)freq;
        return Eigen::MatrixXcd::Zero(m_ports, m_ports);
    }

    QPair<QVector<double>, QVector<double>> getPlotData(int s_param_idx, PlotType type) override
    {
        (void)type;
        if (s_param_idx != 1)
            return {};
        return {QVector<double>{1.0, 2.0}, QVector<double>{-1.0, -2.0}};
    }

    Network* clone(QObject* parent = nullptr) const override
    {
        auto* copy = new DummyPlotNetwork();
        copy->setParent(parent);
        copy->setColor(color());
        copy->setVisible(isVisible());
        copy->setUnwrapPhase(unwrapPhase());
        copy->setActive(isActive());
        copy->setFmin(fmin());
        copy->setFmax(fmax());
        copy->copyStyleSettingsFrom(this);
        return copy;
    }

    QVector<double> frequencies() const override
    {
        return QVector<double>{1.0, 2.0};
    }

    int portCount() const override
    {
        return m_ports;
    }

private:
    int m_ports = 2;
};

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
    QApplication app(argc, argv);

    QCustomPlot plot;
    PlotManager manager(&plot);
    manager.setCascade(nullptr);

    DummyPlotNetwork network;
    network.setParameterWidth("s21", 4);

    QList<Network*> networks{&network};
    manager.setNetworks(networks);

    manager.updatePlots(QStringList{QStringLiteral("s21")}, PlotType::Magnitude);

    if (plot.graphCount() != 1) {
        std::cerr << "Expected a single graph to be created" << std::endl;
        return 1;
    }

    QCPGraph* graph = plot.graph(0);
    if (!graph) {
        std::cerr << "Graph pointer was null" << std::endl;
        return 1;
    }

    const double initialWidth = graph->pen().widthF();
    if (std::abs(initialWidth - 4.0) > 1e-6) {
        std::cerr << "Unexpected initial pen width: " << initialWidth << std::endl;
        return 1;
    }

    manager.selectionChanged();

    const double postSelectionWidth = graph->pen().widthF();
    if (std::abs(postSelectionWidth - 4.0) > 1e-6) {
        std::cerr << "Selection handling changed the custom width to " << postSelectionWidth << std::endl;
        return 1;
    }

    std::cout << "PlotManager selection width test passed." << std::endl;
    return 0;
}
