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
    connect(m_plot, &QCustomPlot::mouseDoubleClick, this, &PlotManager::mouseDoubleClick);
    connect(m_plot, &QCustomPlot::mousePress, this, &PlotManager::mousePress);
    connect(m_plot, &QCustomPlot::mouseMove, this, &PlotManager::mouseMove);
    connect(m_plot, &QCustomPlot::mouseRelease, this, &PlotManager::mouseRelease);

    m_plot->setSelectionRectMode(QCP::srmZoom);

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

void PlotManager::plot(const QVector<double> &x, const QVector<double> &y, const QColor &color, const QString &name, Qt::PenStyle style)
{
    QCPGraph *graph = m_plot->addGraph(m_plot->xAxis, m_plot->yAxis);
    graph->setData(x, y);
    graph->setPen(QPen(color, 2, style));
    graph->setName(name);
}

void PlotManager::updatePlots(const QStringList& sparams, bool isPhase)
{
    QStringList required_graphs;
    QString yAxisLabel = isPhase ? "Phase (deg)" : "Magnitude (dB)";
    m_plot->yAxis->setLabel(yAxisLabel);

    // Build list of required graphs from individual networks
    for (auto network : qAsConst(m_networks)) {
        if (network->isVisible()) {
            for (const auto& sparam : sparams) {
                QString graph_name = network->name() + "_" + sparam;
                if (isPhase) graph_name += "_phase";
                required_graphs << graph_name;
            }
        }
    }

    // Build list of required graphs from cascade
    if (m_cascade && m_cascade->getNetworks().size() > 0) {
        for (const auto& sparam : sparams) {
            QString graph_name = m_cascade->name() + "_" + sparam;
            if (isPhase) graph_name += "_phase";
            required_graphs << graph_name;
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
                if (isPhase) graph_name += "_phase";
                bool graph_exists = false;
                for(int i=0; i<m_plot->graphCount(); ++i) {
                    if(m_plot->graph(i)->name() == graph_name) {
                        graph_exists = true;
                        break;
                    }
                }
                if (!graph_exists) {
                    auto plotData = network->getPlotData(sparam_idx_to_plot, isPhase);
                    plot(plotData.first, plotData.second, m_colors.at(m_color_index % m_colors.size()), graph_name);
                    m_color_index++;
                }
            }
        }

        // Cascade
        if (m_cascade && m_cascade->getNetworks().size() > 0) {
            QString graph_name = m_cascade->name() + "_" + sparam;
            if (isPhase) graph_name += "_phase";
             bool graph_exists = false;
            for(int i=0; i<m_plot->graphCount(); ++i) {
                if(m_plot->graph(i)->name() == graph_name) {
                    graph_exists = true;
                    break;
                }
            }
            if(!graph_exists) {
                auto plotData = m_cascade->getPlotData(sparam_idx_to_plot, isPhase);
                plot(plotData.first, plotData.second, Qt::black, graph_name, Qt::DashLine);
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

        // Check for tracer A
        if (mTracerA->visible())
        {
            const QPointF tracerPos = mTracerA->position->pixelPosition();
            if (qAbs(event->pos().x() - tracerPos.x()) < 5) // Vertical line drag
            {
                mDraggedTracer = mTracerA;
                mDragMode = DragMode::Vertical;
            }
            else if (qAbs(event->pos().y() - tracerPos.y()) < 5) // Horizontal line drag
            {
                mDraggedTracer = mTracerA;
                mDragMode = DragMode::Horizontal;
            }
        }
        // Check for tracer B if A was not hit
        if (!mDraggedTracer && mTracerB->visible())
        {
            const QPointF tracerPos = mTracerB->position->pixelPosition();
            if (qAbs(event->pos().x() - tracerPos.x()) < 5) // Vertical line drag
            {
                mDraggedTracer = mTracerB;
                mDragMode = DragMode::Vertical;
            }
            else if (qAbs(event->pos().y() - tracerPos.y()) < 5) // Horizontal line drag
            {
                mDraggedTracer = mTracerB;
                mDragMode = DragMode::Horizontal;
            }
        }

        if (mDraggedTracer)
        {
            m_plot->setSelectionRectMode(QCP::srmNone);
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
        labelText += QString("\nΔx: %1Hz\nΔy: %2").arg(QString::number(dx, 'g', 4)).arg(QString::number(dy, 'f', 2));
    }

    text->setText(labelText);
    text->position->setCoords(x, y);
    text->position->setType(QCPItemPosition::ptPlotCoords);
    text->setPositionAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    text->setPadding(QMargins(5, 0, 0, 15));
}

void PlotManager::updateTracers()
{
    if (mTracerA && mTracerA->visible())
        updateTracerText(mTracerA, mTracerTextA);
    if (mTracerB && mTracerB->visible())
        updateTracerText(mTracerB, mTracerTextB);
}
