#include "plotmanager.h"
#include "qcustomplot.h"
#include "network.h"
#include "networkcascade.h"
#include <QRubberBand>
#include <QMouseEvent>

PlotManager::PlotManager(QCustomPlot* plot, QObject *parent)
    : QObject(parent)
    , m_plot(plot)
    , m_rubber_band(new QRubberBand(QRubberBand::Rectangle, m_plot))
    , m_cursor_a(nullptr)
    , m_cursor_b(nullptr)
    , m_cascade(nullptr)
    , m_color_index(0)
{
    m_plot->setInteractions(QCP::iSelectPlottables | QCP::iMultiSelect);
    connect(m_plot, &QCustomPlot::mousePress, this, &PlotManager::onMousePress);
    connect(m_plot, &QCustomPlot::mouseMove, this, &PlotManager::onMouseMove);
    connect(m_plot, &QCustomPlot::mouseRelease, this, &PlotManager::onMouseRelease);
    connect(m_plot, &QCustomPlot::mouseWheel, this, &PlotManager::onMouseWheel);
    connect(m_plot, &QCustomPlot::mouseDoubleClick, this, &PlotManager::onMouseDoubleClick);
    connect(m_plot, &QCustomPlot::plottableClick, this, &PlotManager::onPlottableClick);


    m_colors.append(QColor(0, 114, 189));   // Blue
    m_colors.append(QColor(217, 83, 25));    // Orange
    m_colors.append(QColor(237, 177, 32));   // Yellow
    m_colors.append(QColor(126, 47, 142));   // Purple
    m_colors.append(QColor(119, 172, 48));   // Green
    m_colors.append(QColor(77, 190, 238));   // Light Blue
    m_colors.append(QColor(162, 20, 47));    // Red

    setupCursors();
}

PlotManager::~PlotManager()
{
    delete m_rubber_band;
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

void PlotManager::createDifference(QCPGraph* graph1, QCPGraph* graph2) {
    if (!graph1 || !graph2)
        return;

    QVector<double> x, y;
    for (int i = 0; i < graph1->dataCount(); ++i) {
        double key1 = graph1->data()->at(i)->key;
        double value1 = graph1->data()->at(i)->value;

        // Find corresponding point in graph2
        for (int j = 0; j < graph2->dataCount(); ++j) {
            if (qFuzzyCompare(graph2->data()->at(j)->key, key1)) {
                double value2 = graph2->data()->at(j)->value;
                x.append(key1);
                y.append(value1 - value2);
                break;
            }
        }
    }
    plot(x, y, Qt::red, "Difference", m_plot->yAxis->label(), Qt::DashDotLine);
    m_plot->replot();
}


void PlotManager::onMousePress(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        m_drag_start_pos = event->pos();
        m_rubber_band->setGeometry(QRect(m_drag_start_pos, QSize()));
        m_rubber_band->show();
    }
    else if (event->button() == Qt::RightButton)
    {
        m_drag_start_pos = event->pos();
    }
}

void PlotManager::onMouseMove(QMouseEvent* event)
{
    if (event->buttons() & Qt::LeftButton)
    {
        m_rubber_band->setGeometry(QRect(m_drag_start_pos, event->pos()).normalized());
    }
    else if (event->buttons() & Qt::RightButton)
    {
        QPoint delta = event->pos() - m_drag_start_pos;
        m_drag_start_pos = event->pos();

        m_plot->xAxis->moveRange(m_plot->xAxis->pixelToCoord(0) - m_plot->xAxis->pixelToCoord(delta.x()));
        m_plot->yAxis->moveRange(m_plot->yAxis->pixelToCoord(0) - m_plot->yAxis->pixelToCoord(delta.y()));
        m_plot->replot();
    }
}

void PlotManager::onMouseRelease(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        m_rubber_band->hide();
        QRect rect = m_rubber_band->geometry();
        if (rect.width() < 5 || rect.height() < 5) return;

        m_plot->xAxis->setRange(m_plot->xAxis->pixelToCoord(rect.left()), m_plot->xAxis->pixelToCoord(rect.right()));
        m_plot->yAxis->setRange(m_plot->yAxis->pixelToCoord(rect.bottom()), m_plot->yAxis->pixelToCoord(rect.top()));
        m_plot->replot();
    }
}

void PlotManager::onMouseWheel(QWheelEvent* event)
{
    double factor = 1.15;
    if (event->angleDelta().y() < 0)
        factor = 1.0 / factor;

    m_plot->xAxis->scaleRange(factor, m_plot->xAxis->pixelToCoord(event->position().x()));
    m_plot->yAxis->scaleRange(factor, m_plot->yAxis->pixelToCoord(event->position().y()));
    m_plot->replot();
}

void PlotManager::onMouseDoubleClick(QMouseEvent *event)
{
    Q_UNUSED(event);
    autoscale();
}

void PlotManager::setupCursors()
{
    m_cursor_a = new QCPItemTracer(m_plot);
    m_cursor_a->setPen(QPen(Qt::red, 1, Qt::DashLine));
    m_cursor_a->setBrush(Qt::NoBrush);
    m_cursor_a->setStyle(QCPItemTracer::tsCrosshair);
    m_cursor_a->setVisible(false);

    m_cursor_b = new QCPItemTracer(m_plot);
    m_cursor_b->setPen(QPen(Qt::blue, 1, Qt::DashLine));
    m_cursor_b->setBrush(Qt::NoBrush);
    m_cursor_b->setStyle(QCPItemTracer::tsCrosshair);
    m_cursor_b->setVisible(false);
}

void PlotManager::onPlottableClick(QCPAbstractPlottable *plottable, int dataIndex, QMouseEvent *event)
{
    QCPGraph *graph = qobject_cast<QCPGraph*>(plottable);
    if (!graph) return;

    QCPItemTracer* cursor = nullptr;
    if (event->button() == Qt::LeftButton)
    {
        cursor = m_cursor_a;
    }
    else if (event->button() == Qt::RightButton)
    {
        cursor = m_cursor_b;
    }

    if (cursor)
    {
        cursor->setGraph(graph);
        cursor->setGraphKey(graph->data()->at(dataIndex)->key);
        cursor->setVisible(true);
        m_plot->replot();

        QString cursor_a_text, cursor_b_text, delta_text;
        if (m_cursor_a->visible())
            cursor_a_text = QString("A: %1 Hz, %2 dB").arg(m_cursor_a->position->key()).arg(m_cursor_a->position->value());
        if (m_cursor_b->visible())
            cursor_b_text = QString("B: %1 Hz, %2 dB").arg(m_cursor_b->position->key()).arg(m_cursor_b->position->value());
        if (m_cursor_a->visible() && m_cursor_b->visible())
        {
            double delta_x = m_cursor_b->position->key() - m_cursor_a->position->key();
            double delta_y = m_cursor_b->position->value() - m_cursor_a->position->value();
            delta_text = QString("Δ: %1 Hz, %2 dB").arg(delta_x).arg(delta_y);
        }
        emit cursorUpdated(cursor_a_text, cursor_b_text, delta_text);
    }
}
