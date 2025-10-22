#include <QApplication>
#include "plotmanager.h"
#include "plotsettingsdialog.h"
#include "qcustomplot.h"
#include "network.h"

#include <iostream>
#include <cmath>

class MarkerTestNetwork : public Network
{
public:
    MarkerTestNetwork()
        : Network(nullptr)
    {
        setVisible(true);
        setColor(Qt::yellow);
    }

    QString name() const override { return QStringLiteral("marker_net"); }

    Eigen::MatrixXcd sparameters(const Eigen::VectorXd& freq) const override
    {
        Q_UNUSED(freq);
        return Eigen::MatrixXcd::Zero(1, 1);
    }

    QPair<QVector<double>, QVector<double>> getPlotData(int s_param_idx, PlotType type) override
    {
        if (s_param_idx != 0)
            return {};

        switch (type)
        {
        case PlotType::Magnitude:
            return {QVector<double>{1.0, 2.0, 3.0}, QVector<double>{0.0, 1.0, 2.0}};
        case PlotType::TDR:
            return {QVector<double>{0.0, 0.5, 1.0}, QVector<double>{40.0, 45.0, 50.0}};
        default:
            return {};
        }
    }

    Network* clone(QObject* parent = nullptr) const override
    {
        auto* copy = new MarkerTestNetwork();
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
        return QVector<double>{1.0, 2.0, 3.0};
    }

    int portCount() const override
    {
        return 1;
    }
};

static QCPItemTracer* firstTracer(QCustomPlot &plot)
{
    for (int i = 0; i < plot.itemCount(); ++i)
    {
        if (auto *tracer = qobject_cast<QCPItemTracer*>(plot.item(i)))
            return tracer;
    }
    return nullptr;
}

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
    QApplication app(argc, argv);

    QCustomPlot plot;
    PlotManager manager(&plot);

    MarkerTestNetwork network;
    QList<Network*> networks{&network};
    manager.setNetworks(networks);
    manager.setCascade(nullptr);

    const QStringList sparams{QStringLiteral("s11")};

    manager.updatePlots(sparams, PlotType::Magnitude);
    manager.setCursorAVisible(true);

    QCPItemTracer *tracerA = firstTracer(plot);
    if (!tracerA)
    {
        std::cerr << "Failed to locate tracer A" << std::endl;
        return 1;
    }

    if (!tracerA->graph())
    {
        std::cerr << "Tracer A had no graph in magnitude plot" << std::endl;
        return 1;
    }

    tracerA->setGraphKey(2.0);

    manager.updatePlots(sparams, PlotType::TDR);

    if (!tracerA->graph())
    {
        std::cerr << "Tracer A had no graph in TDR plot" << std::endl;
        return 1;
    }

    tracerA->setGraphKey(0.5);

    manager.updatePlots(sparams, PlotType::Magnitude);

    if (!tracerA->graph())
    {
        std::cerr << "Tracer A lost graph when returning to magnitude" << std::endl;
        return 1;
    }

    double magnitudeKey = tracerA->graphKey();
    if (std::abs(magnitudeKey - 2.0) > 1e-6)
    {
        std::cerr << "Magnitude marker key expected 2.0 but got " << magnitudeKey << std::endl;
        return 1;
    }

    manager.updatePlots(sparams, PlotType::TDR);

    if (!tracerA->graph())
    {
        std::cerr << "Tracer A lost graph when returning to TDR" << std::endl;
        return 1;
    }

    double tdrKey = tracerA->graphKey();
    if (std::abs(tdrKey - 0.5) > 1e-6)
    {
        std::cerr << "TDR marker key expected 0.5 but got " << tdrKey << std::endl;
        return 1;
    }

    std::cout << "TDR marker persistence test passed." << std::endl;
    return 0;
}
