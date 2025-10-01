#include <QApplication>
#include "plotmanager.h"
#include "qcustomplot.h"

#include <iostream>
#include <cmath>
#include <set>

class MathPlotTestNetwork : public Network
{
public:
    MathPlotTestNetwork(const QString &name,
                        const QVector<double> &frequencies,
                        const QVector<double> &values)
        : Network(nullptr)
        , m_name(name)
        , m_freq(frequencies)
        , m_values(values)
    {
        setVisible(true);
        setColor(Qt::green);
    }

    QString name() const override { return m_name; }

    void setName(const QString &name)
    {
        m_name = name;
    }

    QPair<QVector<double>, QVector<double>> getPlotData(int s_param_idx, PlotType type) override
    {
        Q_UNUSED(type);
        if (s_param_idx != 1)
            return {};
        return {m_freq, m_values};
    }

    Network* clone(QObject* parent = nullptr) const override
    {
        auto *copy = new MathPlotTestNetwork(m_name, m_freq, m_values);
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
        return m_freq;
    }

    int portCount() const override
    {
        return 2;
    }

    Eigen::MatrixXcd sparameters(const Eigen::VectorXd& freq) const override
    {
        Q_UNUSED(freq);
        return Eigen::MatrixXcd::Zero(portCount(), portCount());
    }

    void setValues(const QVector<double> &values)
    {
        m_values = values;
    }

    const QVector<double>& values() const
    {
        return m_values;
    }

private:
    QString m_name;
    QVector<double> m_freq;
    QVector<double> m_values;
};

static bool extractGraphData(QCPGraph *graph, QVector<double> &x, QVector<double> &y)
{
    if (!graph)
        return false;
    const auto data = graph->data();
    if (data->isEmpty())
        return false;
    x.clear();
    y.clear();
    x.reserve(data->size());
    y.reserve(data->size());
    for (auto it = data->constBegin(); it != data->constEnd(); ++it)
    {
        x.append(it->key);
        y.append(it->value);
    }
    return true;
}

static bool computeDifference(QCPGraph *graph1, QCPGraph *graph2,
                              QVector<double> &x, QVector<double> &y)
{
    if (!graph1 || !graph2)
        return false;

    auto interpolate = [](QCPGraph *graph, double key, double &result) -> bool
    {
        if (!graph)
            return false;
        auto data = graph->data();
        if (data->isEmpty())
            return false;
        auto it = data->findBegin(key);
        if (it == data->constBegin())
        {
            if (qFuzzyCompare(it->key, key))
            {
                result = it->value;
                return true;
            }
            return false;
        }
        if (it == data->constEnd())
            return false;
        if (qFuzzyCompare(it->key, key))
        {
            result = it->value;
            return true;
        }
        auto itPrev = it;
        --itPrev;
        double x1 = itPrev->key;
        double y1 = itPrev->value;
        double x2 = it->key;
        double y2 = it->value;
        if (qFuzzyCompare(x1, x2))
            return false;
        double t = (key - x1) / (x2 - x1);
        result = y1 + t * (y2 - y1);
        return true;
    };

    std::set<double> keys;
    auto data1 = graph1->data();
    for (auto it = data1->constBegin(); it != data1->constEnd(); ++it)
        keys.insert(it->key);
    auto data2 = graph2->data();
    for (auto it = data2->constBegin(); it != data2->constEnd(); ++it)
        keys.insert(it->key);

    QVector<double> diffX;
    QVector<double> diffY;
    for (double key : keys)
    {
        double y1 = 0;
        double y2 = 0;
        if (interpolate(graph1, key, y1) && interpolate(graph2, key, y2))
        {
            diffX.append(key);
            diffY.append(y1 - y2);
        }
    }

    if (diffX.isEmpty())
        return false;

    x = diffX;
    y = diffY;
    return true;
}

static QCPGraph *findGraphByName(QCustomPlot *plot, const QString &name)
{
    if (!plot)
        return nullptr;
    for (int i = 0; i < plot->graphCount(); ++i)
    {
        if (QCPGraph *graph = plot->graph(i))
        {
            if (graph->name() == name)
                return graph;
        }
    }
    return nullptr;
}

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
    QApplication app(argc, argv);

    QVector<double> freq{1.0, 2.0, 3.0};
    MathPlotTestNetwork netA(QStringLiteral("netA"), freq, QVector<double>{0.1, 0.2, 0.3});
    MathPlotTestNetwork netB(QStringLiteral("netB"), freq, QVector<double>{0.05, 0.15, 0.25});

    QCustomPlot plot;
    PlotManager manager(&plot);
    manager.setCascade(nullptr);
    manager.setNetworks(QList<Network*>{&netA, &netB});
    manager.updatePlots(QStringList{QStringLiteral("s21")}, PlotType::Magnitude);

    if (plot.graphCount() != 2)
    {
        std::cerr << "Expected two graphs for the base networks" << std::endl;
        return 1;
    }

    QCPGraph *graphA = findGraphByName(&plot, QStringLiteral("netA_s21"));
    QCPGraph *graphB = findGraphByName(&plot, QStringLiteral("netB_s21"));
    if (!graphA || !graphB)
    {
        std::cerr << "Failed to locate base graphs" << std::endl;
        return 1;
    }

    graphA->setSelection(QCPDataSelection(graphA->data()->dataRange()));
    graphB->setSelection(QCPDataSelection(graphB->data()->dataRange()));

    manager.createMathPlot();

    QCPGraph *mathGraph = nullptr;
    for (int i = 0; i < plot.graphCount(); ++i)
    {
        if (QCPGraph *graph = plot.graph(i))
        {
            if (graph->property("math_plot").toBool())
            {
                mathGraph = graph;
                break;
            }
        }
    }

    if (!mathGraph)
    {
        std::cerr << "Failed to create math plot" << std::endl;
        return 1;
    }

    auto verifyMathGraph = [&](const QStringList &sources) -> bool
    {
        if (sources.size() != 2)
        {
            std::cerr << "Math plot sources missing" << std::endl;
            return false;
        }

        QCPGraph *source1 = findGraphByName(&plot, sources.at(0));
        QCPGraph *source2 = findGraphByName(&plot, sources.at(1));
        if (!source1 || !source2)
        {
            std::cerr << "Unable to resolve math plot sources" << std::endl;
            return false;
        }

        QVector<double> expectedX;
        QVector<double> expectedY;
        if (!computeDifference(source1, source2, expectedX, expectedY))
        {
            std::cerr << "Failed to compute expected difference" << std::endl;
            return false;
        }

        QVector<double> actualX;
        QVector<double> actualY;
        if (!extractGraphData(mathGraph, actualX, actualY))
        {
            std::cerr << "Failed to extract math graph data" << std::endl;
            return false;
        }

        if (expectedX.size() != actualX.size() || expectedY.size() != actualY.size())
        {
            std::cerr << "Size mismatch between expected and actual data" << std::endl;
            return false;
        }

        const double tol = 1e-9;
        for (int i = 0; i < expectedX.size(); ++i)
        {
            if (std::abs(expectedX.at(i) - actualX.at(i)) > tol ||
                std::abs(expectedY.at(i) - actualY.at(i)) > tol)
            {
                std::cerr << "Value mismatch at index " << i << std::endl;
                return false;
            }
        }
        return true;
    };

    if (!verifyMathGraph(mathGraph->property("math_plot_sources").toStringList()))
        return 1;

    netA.setValues(QVector<double>{0.2, 0.3, 0.4});
    manager.updatePlots(QStringList{QStringLiteral("s21")}, PlotType::Magnitude);

    // After update, mathGraph should still be present.
    mathGraph = nullptr;
    for (int i = 0; i < plot.graphCount(); ++i)
    {
        if (QCPGraph *graph = plot.graph(i))
        {
            if (graph->property("math_plot").toBool())
            {
                mathGraph = graph;
                break;
            }
        }
    }

    if (!mathGraph)
    {
        std::cerr << "Math plot disappeared after update" << std::endl;
        return 1;
    }

    if (!verifyMathGraph(mathGraph->property("math_plot_sources").toStringList()))
        return 1;

    netA.setName(QStringLiteral("netA_mod"));
    manager.updatePlots(QStringList{QStringLiteral("s21")}, PlotType::Magnitude);

    mathGraph = nullptr;
    for (int i = 0; i < plot.graphCount(); ++i)
    {
        if (QCPGraph *graph = plot.graph(i))
        {
            if (graph->property("math_plot").toBool())
            {
                mathGraph = graph;
                break;
            }
        }
    }

    if (!mathGraph)
    {
        std::cerr << "Math plot disappeared after rename" << std::endl;
        return 1;
    }

    QStringList renamedSources = mathGraph->property("math_plot_sources").toStringList();
    if (renamedSources.size() != 2 ||
        renamedSources.at(0) != QStringLiteral("netA_mod_s21") ||
        renamedSources.at(1) != QStringLiteral("netB_s21"))
    {
        std::cerr << "Math plot sources not updated after rename" << std::endl;
        return 1;
    }

    if (!verifyMathGraph(renamedSources))
        return 1;

    std::cout << "Math plot update test passed." << std::endl;
    return 0;
}
