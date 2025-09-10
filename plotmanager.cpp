#include "plotmanager.h"
#include "qcustomplot.h"
#include "network.h"
#include "networkcascade.h"

PlotManager::PlotManager(QCustomPlot* plot, QObject *parent)
    : QObject(parent)
    , m_plot(plot)
    , m_cascade(nullptr)
    , m_color_index(0)
{
    m_plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables | QCP::iMultiSelect);

    m_colors.append(QColor(0, 114, 189));   // Blue
    m_colors.append(QColor(217, 83, 25));    // Orange
    m_colors.append(QColor(237, 177, 32));   // Yellow
    m_colors.append(QColor(126, 47, 142));   // Purple
    m_colors.append(QColor(119, 172, 48));   // Green
    m_colors.append(QColor(77, 190, 238));   // Light Blue
    m_colors.append(QColor(162, 20, 47));    // Red
}

void PlotManager::setNetworks(const QList<Network*>& networks)
{
    m_networks = networks;
}

void PlotManager::setCascade(NetworkCascade* cascade)
{
    m_cascade = cascade;
}

void PlotManager::plot(const QVector<double> &x, const QVector<double> &y, const QColor &color, const QString &name, const QString &yAxisLabel, Qt::PenStyle style)
{
    QCPGraph *graph = m_plot->addGraph(m_plot->xAxis, m_plot->yAxis);
    graph->setData(x, y);
    graph->setPen(QPen(color, 2, style));
    graph->setName(name);
    m_plot->yAxis->setLabel(yAxisLabel);
}

void PlotManager::updatePlots(const QStringList& sparams, bool isPhase)
{
    QStringList required_graphs;
    QString yAxisLabel = isPhase ? "Phase (deg)" : "Magnitude (dB)";

    // Build list of required graphs from individual networks
    for (auto network : qAsConst(m_networks)) {
        if (network->isVisible()) {
            for (const auto& sparam : sparams) {
                required_graphs << network->name() + "_" + sparam;
            }
        }
    }

    // Build list of required graphs from cascade
    if (m_cascade && m_cascade->getNetworks().size() > 0) {
        for (const auto& sparam : sparams) {
            required_graphs << m_cascade->name() + "_" + sparam;
        }
    }

    // Remove graphs that are no longer needed
    for (int i = m_plot->graphCount() - 1; i >= 0; --i) {
        if (!required_graphs.contains(m_plot->graph(i)->name())) {
            m_plot->removeGraph(i);
        }
    }

    m_color_index = 0;
    // Add new graphs
    for (const auto& sparam : sparams) {
        int sparam_idx_to_plot = -1;
        if (sparam == "s11") sparam_idx_to_plot = 0;
        else if (sparam == "s21") sparam_idx_to_plot = 1;
        else if (sparam == "s12") sparam_idx_to_plot = 2;
        else if (sparam == "s22") sparam_idx_to_plot = 3;

        // Individual networks
        for (auto network : qAsConst(m_networks)) {
            if (network->isVisible()) {
                QString graph_name = network->name() + "_" + sparam;
                bool graph_exists = false;
                for(int i=0; i<m_plot->graphCount(); ++i) {
                    if(m_plot->graph(i)->name() == graph_name) {
                        graph_exists = true;
                        break;
                    }
                }
                if (!graph_exists) {
                    auto plotData = network->getPlotData(sparam_idx_to_plot, isPhase);
                    plot(plotData.first, plotData.second, m_colors.at(m_color_index % m_colors.size()), graph_name, yAxisLabel);
                    m_color_index++;
                }
            }
        }

        // Cascade
        if (m_cascade && m_cascade->getNetworks().size() > 0) {
            QString graph_name = m_cascade->name() + "_" + sparam;
             bool graph_exists = false;
            for(int i=0; i<m_plot->graphCount(); ++i) {
                if(m_plot->graph(i)->name() == graph_name) {
                    graph_exists = true;
                    break;
                }
            }
            if(!graph_exists) {
                auto plotData = m_cascade->getPlotData(sparam_idx_to_plot, isPhase);
                plot(plotData.first, plotData.second, Qt::black, graph_name, yAxisLabel, Qt::DashLine);
            }
        }
    }

    m_plot->replot();
}

void PlotManager::autoscale()
{
    m_plot->rescaleAxes();
    m_plot->replot();
}
