#include "plotmanager.h"
#include "qcustomplot.h"
#include "network.h"
#include "networkcascade.h"
#include "SmithChartGrid.h"
#include <QDebug>
#include <QVariant>
#include <set>

using namespace std;

PlotManager::PlotManager(QCustomPlot* plot, QObject *parent)
    : QObject(parent)
    , m_plot(plot)
    , m_cascade(nullptr)
    , m_color_index(0)
    , m_keepAspectConnected(false)
{
    m_plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables | QCP::iMultiSelect);
    connect(m_plot, &QCustomPlot::mouseDoubleClick, this, &PlotManager::mouseDoubleClick);
    connect(m_plot, &QCustomPlot::mousePress, this, &PlotManager::mousePress);
    connect(m_plot, &QCustomPlot::mouseMove, this, &PlotManager::mouseMove);
    connect(m_plot, &QCustomPlot::mouseRelease, this, &PlotManager::mouseRelease);
    connect(m_plot, &QCustomPlot::selectionChangedByUser, this, &PlotManager::selectionChanged);

    m_plot->setSelectionRectMode(QCP::srmZoom);
    m_plot->setRangeDragButton(Qt::RightButton);

    mTracerA = new QCPItemTracer(m_plot);
    mTracerA->setPen(QPen(Qt::black,0));
    mTracerA->setBrush(Qt::NoBrush);
    mTracerA->setStyle(QCPItemTracer::tsCrosshair);
    mTracerA->setVisible(false);
    mTracerA->setInterpolating(true);

    mTracerTextA = new QCPItemText(m_plot);
    mTracerTextA->setColor(Qt::red);
    mTracerTextA->setVisible(false);

    mTracerB = new QCPItemTracer(m_plot);
    mTracerB->setPen(QPen(Qt::darkGray,0));
    mTracerB->setBrush(Qt::NoBrush);
    mTracerB->setStyle(QCPItemTracer::tsCrosshair);
    mTracerB->setVisible(false);
    mTracerB->setInterpolating(true);

    mTracerTextB = new QCPItemText(m_plot);
    mTracerTextB->setColor(Qt::blue);
    mTracerTextB->setVisible(false);

    m_plot->addLayer("tracers", m_plot->layer("main"), QCustomPlot::limAbove);
    mTracerA->setLayer("tracers");
    mTracerTextA->setLayer("tracers");
    mTracerB->setLayer("tracers");
    mTracerTextB->setLayer("tracers");

    mDraggedTracer = nullptr;
    mDragMode = DragMode::None;

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

QColor PlotManager::nextColor()
{
    QColor color = m_colors.at(m_color_index % m_colors.size());
    m_color_index++;
    return color;
}

QCPGraph* PlotManager::plot(const QVector<double> &x, const QVector<double> &y, const QColor &color,
                       const QString &name, Network* network,
                       Qt::PenStyle style)
{
    QCPGraph *graph = m_plot->addGraph(m_plot->xAxis, m_plot->yAxis);
    graph->setData(x, y);
    graph->setPen(QPen(color, 1, style));
    graph->setName(name);
    graph->setProperty("network_ptr", QVariant::fromValue(reinterpret_cast<quintptr>(network)));
    graph->setSelectable(QCP::stWhole);

    return graph;
}

void PlotManager::updatePlots(const QStringList& sparams, PlotType type)
{
    qInfo("  updatePlots()");
    QStringList required_graphs;
    QString suffix;

    switch (type) {
    case PlotType::Magnitude:
        m_plot->yAxis->setLabel("Magnitude (dB)");
        m_plot->xAxis->setLabel("Frequency (Hz)");
        suffix = "";
        break;
    case PlotType::Phase:
        m_plot->yAxis->setLabel("Phase (deg)");
        m_plot->xAxis->setLabel("Frequency (Hz)");
        suffix = "_phase";
        break;
    case PlotType::VSWR:
        m_plot->yAxis->setLabel("VSWR");
        m_plot->xAxis->setLabel("Frequency (Hz)");
        suffix = "_vswr";
        break;
    case PlotType::Smith:
        m_plot->yAxis->setLabel("Imag");
        m_plot->xAxis->setLabel("Real");
        suffix = "_smith";
        break;
    }

    if (type == PlotType::Smith) {
        setupSmithGrid();
    } else {
        clearSmithGrid();
        clearSmithMarkers();
        m_plot->xAxis->setTicks(true);
        m_plot->yAxis->setTicks(true);
        m_plot->xAxis->setTickLabels(true);
        m_plot->yAxis->setTickLabels(true);
    }

    // Build list of required graphs from individual networks
    for (auto network : qAsConst(m_networks)) {
        if (network->isVisible()) {
            for (const auto& sparam : sparams) {
                QString graph_name = network->name() + "_" + sparam + suffix;
                required_graphs << graph_name;
            }
        }
    }

    // Build list from cascade
    if (m_cascade && m_cascade->getNetworks().size() > 0) {
        for (const auto& sparam : sparams) {
            QString graph_name = m_cascade->name() + "_" + sparam + suffix;
            required_graphs << graph_name;
        }
    }

    // Remove graphs not needed
    for (int i = m_plot->graphCount() - 1; i >= 0; --i) {
        if (!required_graphs.contains(m_plot->graph(i)->name())) {
            m_plot->removeGraph(i);
        }
    }

    if (type == PlotType::Smith)
        clearSmithMarkers();

    for (const auto& sparam : sparams) {
        int sparam_idx_to_plot = -1;
        if (sparam == "s11") sparam_idx_to_plot = 0;
        else if (sparam == "s21") sparam_idx_to_plot = 1;
        else if (sparam == "s12") sparam_idx_to_plot = 2;
        else if (sparam == "s22") sparam_idx_to_plot = 3;

        // Individual networks
        for (auto network : qAsConst(m_networks)) {
            if (!network->isVisible())
                continue;

            QString graph_name = network->name() + "_" + sparam + suffix;
            QCPGraph *graph = nullptr;
            for (int i = 0; i < m_plot->graphCount(); ++i) {
                if (m_plot->graph(i)->name() == graph_name) {
                    graph = m_plot->graph(i);
                    break;
                }
            }

            auto plotData = network->getPlotData(sparam_idx_to_plot, type);
            if (graph) {
                graph->setData(plotData.first, plotData.second);
                graph->setPen(QPen(network->color(), 1, Qt::SolidLine));
            } else {
                graph = plot(plotData.first, plotData.second,
                              network->color(), graph_name, network);
            }

            if (type == PlotType::Smith)
                addSmithMarkers(plotData.first, plotData.second, network->color());
        }

        // Cascade
        if (m_cascade && m_cascade->getNetworks().size() > 0) {
            QString graph_name = m_cascade->name() + "_" + sparam + suffix;
            QCPGraph *graph = nullptr;
            for (int i = 0; i < m_plot->graphCount(); ++i) {
                if (m_plot->graph(i)->name() == graph_name) {
                    graph = m_plot->graph(i);
                    break;
                }
            }

            auto plotData = m_cascade->getPlotData(sparam_idx_to_plot, type);
            if (graph) {
                graph->setData(plotData.first, plotData.second);
            } else {
                graph = plot(plotData.first, plotData.second, Qt::black,
                              graph_name, nullptr, Qt::DashLine);
            }
            if (type == PlotType::Smith)
                addSmithMarkers(plotData.first, plotData.second, Qt::black);
        }
    }

    m_plot->replot();
    selectionChanged();

    if (type == PlotType::Smith) {
        m_plot->xAxis->setScaleRatio(m_plot->yAxis, 1.0);
        if (!m_keepAspectConnected) {
            connect(m_plot->xAxis, SIGNAL(rangeChanged(QCPRange)), this, SLOT(keepAspectRatio()));
            connect(m_plot->yAxis, SIGNAL(rangeChanged(QCPRange)), this, SLOT(keepAspectRatio()));
            m_keepAspectConnected = true;
        }
    } else if (m_keepAspectConnected) {
        disconnect(m_plot->xAxis, SIGNAL(rangeChanged(QCPRange)), this, SLOT(keepAspectRatio()));
        disconnect(m_plot->yAxis, SIGNAL(rangeChanged(QCPRange)), this, SLOT(keepAspectRatio()));
        m_keepAspectConnected = false;
    }
}

void PlotManager::autoscale()
{
    m_plot->rescaleAxes();
    m_plot->replot();
}

void PlotManager::mouseDoubleClick(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        autoscale();
    }
}

void PlotManager::setCursorAVisible(bool visible)
{
    if (m_plot->graphCount() == 0)
    {
        // todo: maybe uncheck the box in the mainwindow?
        return;
    }

    mTracerA->setVisible(visible);
    mTracerTextA->setVisible(visible);

    if (visible)
    {
        QCPGraph *graph = m_plot->graph(0);
        mTracerA->setGraph(graph);
        mTracerA->setGraphKey(m_plot->xAxis->range().center());
    }
    updateTracers();
    m_plot->replot();
}

void PlotManager::setCursorBVisible(bool visible)
{
    if (m_plot->graphCount() == 0)
    {
        // todo: maybe uncheck the box in the mainwindow?
        return;
    }

    mTracerB->setVisible(visible);
    mTracerTextB->setVisible(visible);

    if (visible)
    {
        QCPGraph *graph = m_plot->graph(0);
        mTracerB->setGraph(graph);
        mTracerB->setGraphKey(m_plot->xAxis->range().center());
    }
    updateTracers();
    m_plot->replot();
}


void PlotManager::mousePress(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        mDraggedTracer = nullptr;
        mDragMode = DragMode::None;

        checkForTracerDrag(event, mTracerA);
        if (!mDraggedTracer)
            checkForTracerDrag(event, mTracerB);

        if (mDraggedTracer)
        {
            m_plot->setSelectionRectMode(QCP::srmNone);
        }
    }
}

void PlotManager::checkForTracerDrag(QMouseEvent *event, QCPItemTracer *tracer)
{
    if (tracer->visible())
    {
        const QPointF tracerPos = tracer->position->pixelPosition();
        if (qAbs(event->pos().x() - tracerPos.x()) < 5) // Vertical line drag
        {
            mDraggedTracer = tracer;
            mDragMode = DragMode::Vertical;
        }
        else if (qAbs(event->pos().y() - tracerPos.y()) < 5) // Horizontal line drag
        {
            mDraggedTracer = tracer;
            mDragMode = DragMode::Horizontal;
        }
    }
}

void PlotManager::mouseMove(QMouseEvent *event)
{
    if (mDraggedTracer)
    {
        if (mDragMode == DragMode::Vertical)
        {
            double key = m_plot->xAxis->pixelToCoord(event->pos().x());
            mDraggedTracer->setGraphKey(key);
        }
        else if (mDragMode == DragMode::Horizontal)
        {
            QCPGraph *graph = qobject_cast<QCPGraph*>(m_plot->plottableAt(event->pos(), true));
            if (graph && graph != mDraggedTracer->graph())
            {
                mDraggedTracer->setGraph(graph);
            }
            double key = m_plot->xAxis->pixelToCoord(event->pos().x());
            mDraggedTracer->setGraphKey(key);
        }

        updateTracers();
        m_plot->replot();
    }
}

void PlotManager::mouseRelease(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        if (mDraggedTracer)
        {
            mDraggedTracer = nullptr;
            mDragMode = DragMode::None;
            m_plot->setSelectionRectMode(QCP::srmZoom);
        }
    }
}

void PlotManager::updateTracerText(QCPItemTracer *tracer, QCPItemText *text)
{
    if (!tracer->visible() || !tracer->graph())
        return;

    tracer->updatePosition();
    double x = tracer->position->coords().x();
    double y = tracer->position->coords().y();

    QString labelText = QString::number(x, 'g', 4) + "Hz " + QString::number(y, 'f', 2);

    if (tracer == mTracerB && mTracerA->visible())
    {
        mTracerA->updatePosition();
        double xA = mTracerA->position->coords().x();
        double yA = mTracerA->position->coords().y();
        double dx = x - xA;
        double dy = y - yA;
        labelText += QString("\nΔx: %1Hz Δy: %2").arg(QString::number(dx, 'g', 4)).arg(QString::number(dy, 'f', 2));
    }

    text->setText(labelText);
    text->position->setCoords(x, y);
    text->position->setType(QCPItemPosition::ptPlotCoords);

    if (tracer == mTracerA)
    {
        text->setPositionAlignment(Qt::AlignRight | Qt::AlignVCenter);
        text->setPadding(QMargins(0, 0, 5, 15));
    } else
    {
        text->setTextAlignment(Qt::AlignLeft);
        text->setPositionAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        text->setPadding(QMargins(5, 0, 0, 0));
    }
}

void PlotManager::updateTracers()
{
    if (mTracerA && mTracerA->visible())
        updateTracerText(mTracerA, mTracerTextA);
    if (mTracerB && mTracerB->visible())
        updateTracerText(mTracerB, mTracerTextB);
}

void PlotManager::createMathPlot()
{
    if (m_plot->selectedGraphs().size() == 2)
    {
        QCPGraph *graph1 = m_plot->selectedGraphs().at(0);
        QCPGraph *graph2 = m_plot->selectedGraphs().at(1);

        auto interpolate = [](QCPGraph *graph, double key, double &result) -> bool
        {
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
        for (auto it = graph1->data()->constBegin(); it != graph1->data()->constEnd(); ++it)
            keys.insert(it->key);
        for (auto it = graph2->data()->constBegin(); it != graph2->data()->constEnd(); ++it)
            keys.insert(it->key);

        QVector<double> x, y;
        for (double key : keys)
        {
            double y1, y2;
            if (interpolate(graph1, key, y1) && interpolate(graph2, key, y2))
            {
                x.append(key);
                y.append(y1 - y2);
            }
        }

        if (!x.isEmpty())
        {
            plot(x, y, Qt::red,
                 QString("%1 - %2").arg(graph1->name()).arg(graph2->name()),
                 nullptr);
            m_plot->replot();
        }
    }
}

void PlotManager::selectionChanged()
{
    for (int i = 0; i < m_plot->graphCount(); ++i) {
        QCPGraph *graph = m_plot->graph(i);
        QPen pen = graph->pen();
        pen.setWidthF(graph->selected() ? 2.5 : 1.0);
        graph->setPen(pen);
    }
    m_plot->replot();
}

void PlotManager::keepAspectRatio()
{
    m_plot->xAxis->setScaleRatio(m_plot->yAxis, 1.0);
}

void PlotManager::setupSmithGrid()
{
    if (!m_plot->layer("smithGrid"))
        m_plot->addLayer("smithGrid", m_plot->layer("background"), QCustomPlot::limAbove);

    if (m_smithGridGraphs.isEmpty() && m_smithGridItems.isEmpty()) {
        QVector<double> rVals {0.2,0.5,1.0,2.0,5.0};
        QVector<double> xVals {0.2,0.5,1.0,2.0,5.0};
        QVector<QVector<double>> impX, impY, admX, admY;
        QVector<double> unitX, unitY, realX, realY;
        QVector<double> labelX, labelY; QVector<QString> labelText;
        SmithChart::generateSmithChartGridExtended(impX, impY, admX, admY,
                                                  unitX, unitY, realX, realY,
                                                  labelX, labelY, labelText,
                                                  rVals, xVals, 720);

        QPen impPen(QColor(120,120,120)); impPen.setWidthF(1.0);
        QPen admPen = impPen; admPen.setStyle(Qt::DashLine);

        auto addGraphs = [&](const QVector<QVector<double>>& xs,
                             const QVector<QVector<double>>& ys, const QPen& pen)
        {
            for (int i = 0; i < xs.size(); ++i) {
                QCPGraph *g = m_plot->addGraph();
                g->setData(xs[i], ys[i]);
                g->setPen(pen);
                g->setLayer("smithGrid");
                m_smithGridGraphs.append(g);
            }
        };

        addGraphs(impX, impY, impPen);
        addGraphs(admX, admY, admPen);

        QCPGraph *gUnit = m_plot->addGraph();
        gUnit->setData(unitX, unitY); gUnit->setPen(impPen); gUnit->setLayer("smithGrid");
        m_smithGridGraphs.append(gUnit);
        QCPGraph *gReal = m_plot->addGraph();
        gReal->setData(realX, realY); gReal->setPen(impPen); gReal->setLayer("smithGrid");
        m_smithGridGraphs.append(gReal);

        for (int i = 0; i < labelX.size(); ++i) {
            QCPItemText *txt = new QCPItemText(m_plot);
            txt->setPositionAlignment(Qt::AlignHCenter|Qt::AlignTop);
            txt->position->setType(QCPItemPosition::ptPlotCoords);
            txt->position->setCoords(labelX[i], -0.03);
            txt->setText(labelText[i]);
            txt->setFont(QFont(m_plot->font().family(), 8));
            txt->setColor(QColor(120,120,120));
            txt->setLayer("smithGrid");
            m_smithGridItems.append(txt);
        }

        m_plot->xAxis->setRange(-1.05, 1.05);
        m_plot->yAxis->setRange(-1.05, 1.05);
        m_plot->xAxis->setTicks(false);
        m_plot->yAxis->setTicks(false);
        m_plot->xAxis->setTickLabels(false);
        m_plot->yAxis->setTickLabels(false);
    }
}

void PlotManager::clearSmithGrid()
{
    for (auto g : m_smithGridGraphs)
        m_plot->removeGraph(g);
    m_smithGridGraphs.clear();
    for (auto item : m_smithGridItems)
        m_plot->removeItem(item);
    m_smithGridItems.clear();
}

void PlotManager::clearSmithMarkers()
{
    for (auto item : m_smithMarkers)
        m_plot->removeItem(item);
    m_smithMarkers.clear();
}

void PlotManager::addSmithMarkers(const QVector<double>& x, const QVector<double>& y, const QColor& color)
{
    if (x.isEmpty())
        return;
    QCPItemTracer *start = new QCPItemTracer(m_plot);
    start->setStyle(QCPItemTracer::tsCircle);
    start->setPen(QPen(color));
    start->setBrush(color);
    start->setSize(6);
    start->position->setType(QCPItemPosition::ptPlotCoords);
    start->position->setCoords(x.first(), y.first());
    start->setLayer("tracers");
    m_smithMarkers.append(start);

    if (x.size() > 1) {
        QCPItemLine *arrow = new QCPItemLine(m_plot);
        arrow->start->setType(QCPItemPosition::ptPlotCoords);
        arrow->end->setType(QCPItemPosition::ptPlotCoords);
        arrow->start->setCoords(x[x.size()-2], y[y.size()-2]);
        arrow->end->setCoords(x.last(), y.last());
        arrow->setPen(QPen(color));
        arrow->setHead(QCPLineEnding(QCPLineEnding::esSpikeArrow));
        arrow->setLayer("tracers");
        m_smithMarkers.append(arrow);
    }
}

