#include "plotmanager.h"
#include "qcustomplot.h"
#include "network.h"
#include "networkcascade.h"
#include "SmithChartGrid.h"
#include <QDebug>
#include <QVariant>
#include <QLineF>
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

QCPAbstractPlottable* PlotManager::plot(const QVector<double> &x, const QVector<double> &y, const QColor &color,
                       const QString &name, Network* network, PlotType type,
                       Qt::PenStyle style)
{
    if (type == PlotType::Smith)
    {
        QCPCurve *curve = new QCPCurve(m_plot->xAxis, m_plot->yAxis);
        curve->setData(x, y);
        curve->setPen(QPen(color, 1, style));
        curve->setName(name);
        curve->setProperty("network_ptr", QVariant::fromValue(reinterpret_cast<quintptr>(network)));
        curve->setSelectable(QCP::stWhole);
        return curve;
    }
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
        m_plot->xAxis->grid()->setVisible(true);
        m_plot->yAxis->grid()->setVisible(true);
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

    // Remove plottables not needed, but keep Smith grid curves
    for (int i = m_plot->plottableCount() - 1; i >= 0; --i) {
        QCPAbstractPlottable *pl = m_plot->plottable(i);
        if (type == PlotType::Smith && m_smithGridCurves.contains(qobject_cast<QCPCurve*>(pl)))
            continue;
        if (!required_graphs.contains(pl->name()))
            m_plot->removePlottable(pl);
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
            QCPAbstractPlottable *pl = nullptr;
            for (int i = 0; i < m_plot->plottableCount(); ++i) {
                if (m_plot->plottable(i)->name() == graph_name) {
                    pl = m_plot->plottable(i);
                    break;
                }
            }

            auto plotData = network->getPlotData(sparam_idx_to_plot, type);
            if (pl) {
                if (type == PlotType::Smith) {
                    if (QCPCurve *curve = qobject_cast<QCPCurve*>(pl)) {
                        curve->setData(plotData.first, plotData.second);
                        curve->setPen(QPen(network->color(), 1, Qt::SolidLine));
                    }
                } else {
                    if (QCPGraph *graph = qobject_cast<QCPGraph*>(pl)) {
                        graph->setData(plotData.first, plotData.second);
                        graph->setPen(QPen(network->color(), 1, Qt::SolidLine));
                    }
                }
            } else {
                pl = plot(plotData.first, plotData.second,
                              network->color(), graph_name, network, type);
            }

            if (type == PlotType::Smith)
                addSmithMarkers(plotData.first, plotData.second, network->color());
        }

        // Cascade
        if (m_cascade && m_cascade->getNetworks().size() > 0) {
            QString graph_name = m_cascade->name() + "_" + sparam + suffix;
            QCPAbstractPlottable *pl = nullptr;
            for (int i = 0; i < m_plot->plottableCount(); ++i) {
                if (m_plot->plottable(i)->name() == graph_name) {
                    pl = m_plot->plottable(i);
                    break;
                }
            }

            auto plotData = m_cascade->getPlotData(sparam_idx_to_plot, type);
            if (pl) {
                if (type == PlotType::Smith) {
                    if (QCPCurve *curve = qobject_cast<QCPCurve*>(pl))
                        curve->setData(plotData.first, plotData.second);
                } else {
                    if (QCPGraph *graph = qobject_cast<QCPGraph*>(pl))
                        graph->setData(plotData.first, plotData.second);
                }
            } else {
                pl = plot(plotData.first, plotData.second, Qt::black,
                              graph_name, nullptr, type, Qt::DashLine);
            }
            if (type == PlotType::Smith)
                addSmithMarkers(plotData.first, plotData.second, Qt::black);
        }
    }

    m_plot->replot();
    selectionChanged();
    updateTracers();

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
    if (!m_smithGridCurves.isEmpty()) {
        m_plot->xAxis->setRange(-1.05, 1.05);
        m_plot->yAxis->setRange(-1.05, 1.05);
    } else {
        m_plot->rescaleAxes();
    }
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
    if (m_plot->plottableCount() == 0)
    {
        // todo: maybe uncheck the box in the mainwindow?
        return;
    }

    mTracerA->setVisible(visible);
    mTracerTextA->setVisible(visible);

    if (visible)
    {
        if (QCPGraph *graph = qobject_cast<QCPGraph*>(m_plot->plottable(0))) {
            mTracerA->setGraph(graph);
            mTracerA->setGraphKey(m_plot->xAxis->range().center());
        } else {
            mTracerA->setGraph(nullptr);
            mTracerA->position->setType(QCPItemPosition::ptPlotCoords);
            mTracerA->position->setCoords(m_plot->xAxis->range().center(),
                                          m_plot->yAxis->range().center());
        }
    }
    updateTracers();
    m_plot->replot();
}

void PlotManager::setCursorBVisible(bool visible)
{
    if (m_plot->plottableCount() == 0)
    {
        // todo: maybe uncheck the box in the mainwindow?
        return;
    }

    mTracerB->setVisible(visible);
    mTracerTextB->setVisible(visible);

    if (visible)
    {
        if (QCPGraph *graph = qobject_cast<QCPGraph*>(m_plot->plottable(0))) {
            mTracerB->setGraph(graph);
            mTracerB->setGraphKey(m_plot->xAxis->range().center());
        } else {
            mTracerB->setGraph(nullptr);
            mTracerB->position->setType(QCPItemPosition::ptPlotCoords);
            mTracerB->position->setCoords(m_plot->xAxis->range().center(),
                                          m_plot->yAxis->range().center());
        }
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
    if (!tracer->visible())
        return;

    const QPointF tracerPos = tracer->position->pixelPosition();
    if (!m_smithGridCurves.isEmpty()) {
        if (QLineF(event->pos(), tracerPos).length() < 8) {
            mDraggedTracer = tracer;
            mDragMode = DragMode::Free;
        }
    } else {
        if (qAbs(event->pos().x() - tracerPos.x()) < 5) {
            mDraggedTracer = tracer;
            mDragMode = DragMode::Vertical;
        } else if (qAbs(event->pos().y() - tracerPos.y()) < 5) {
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
        else if (mDragMode == DragMode::Free)
        {
            double x = m_plot->xAxis->pixelToCoord(event->pos().x());
            double y = m_plot->yAxis->pixelToCoord(event->pos().y());
            mDraggedTracer->position->setCoords(x, y);
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
    if (!tracer->visible())
        return;

    tracer->updatePosition();
    double x = tracer->position->coords().x();
    double y = tracer->position->coords().y();

    QString labelText;
    if (!m_smithGridCurves.isEmpty()) {
        labelText = QString("Re: %1\nIm: %2").arg(QString::number(x, 'f', 2))
                                              .arg(QString::number(y, 'f', 2));
        if (tracer == mTracerB && mTracerA->visible()) {
            mTracerA->updatePosition();
            double xA = mTracerA->position->coords().x();
            double yA = mTracerA->position->coords().y();
            double dx = x - xA;
            double dy = y - yA;
            labelText += QString("\nΔRe: %1 ΔIm: %2")
                             .arg(QString::number(dx, 'f', 2))
                             .arg(QString::number(dy, 'f', 2));
        }
    } else {
        labelText = QString::number(x, 'g', 4) + "Hz " + QString::number(y, 'f', 2);
        if (tracer == mTracerB && mTracerA->visible())
        {
            mTracerA->updatePosition();
            double xA = mTracerA->position->coords().x();
            double yA = mTracerA->position->coords().y();
            double dx = x - xA;
            double dy = y - yA;
            labelText += QString("\nΔx: %1Hz Δy: %2")
                             .arg(QString::number(dx, 'g', 4))
                             .arg(QString::number(dy, 'f', 2));
        }
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
                 nullptr, PlotType::Magnitude);
            m_plot->replot();
        }
    }
}

void PlotManager::selectionChanged()
{
    for (int i = 0; i < m_plot->plottableCount(); ++i) {
        QCPAbstractPlottable *pl = m_plot->plottable(i);
        QPen pen = pl->pen();
        pen.setWidthF(pl->selected() ? 2.5 : 1.0);
        pl->setPen(pen);
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

    if (m_smithGridCurves.isEmpty() && m_smithGridItems.isEmpty()) {
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

        auto addCurves = [&](const QVector<QVector<double>>& xs,
                             const QVector<QVector<double>>& ys, const QPen& pen)
        {
            for (int i = 0; i < xs.size(); ++i) {
                QCPCurve *c = new QCPCurve(m_plot->xAxis, m_plot->yAxis);
                c->setData(xs[i], ys[i]);
                c->setPen(pen);
                c->setLayer("smithGrid");
                c->setName(QString());
                c->removeFromLegend();
                c->setSelectable(QCP::stNone);
                m_smithGridCurves.append(c);
            }
        };

        addCurves(impX, impY, impPen);
        addCurves(admX, admY, admPen);

        QCPCurve *gUnit = new QCPCurve(m_plot->xAxis, m_plot->yAxis);
        gUnit->setData(unitX, unitY); gUnit->setPen(impPen); gUnit->setLayer("smithGrid");
        gUnit->setName(QString());
        gUnit->removeFromLegend();
        gUnit->setSelectable(QCP::stNone);
        m_smithGridCurves.append(gUnit);
        QCPCurve *gReal = new QCPCurve(m_plot->xAxis, m_plot->yAxis);
        gReal->setData(realX, realY); gReal->setPen(impPen); gReal->setLayer("smithGrid");
        gReal->setName(QString());
        gReal->removeFromLegend();
        gReal->setSelectable(QCP::stNone);
        m_smithGridCurves.append(gReal);

        for (int i = 0; i < labelX.size(); ++i) {
            QCPItemText *txt = new QCPItemText(m_plot);
            txt->setPositionAlignment(Qt::AlignHCenter|Qt::AlignTop);
            txt->position->setType(QCPItemPosition::ptPlotCoords);
            txt->position->setCoords(labelX[i], -0.03);
            txt->setText(labelText[i]);
            txt->setFont(QFont(m_plot->font().family(), 8));
            txt->setColor(QColor(120,120,120));
            txt->setLayer("smithGrid");
            txt->setSelectable(false);
            m_smithGridItems.append(txt);
        }
    }

    m_plot->xAxis->setRange(-1.05, 1.05);
    m_plot->yAxis->setRange(-1.05, 1.05);
    m_plot->xAxis->setTicks(false);
    m_plot->yAxis->setTicks(false);
    m_plot->xAxis->setTickLabels(false);
    m_plot->yAxis->setTickLabels(false);
    m_plot->xAxis->grid()->setVisible(false);
    m_plot->yAxis->grid()->setVisible(false);
}

void PlotManager::clearSmithGrid()
{
    for (auto c : m_smithGridCurves)
        m_plot->removePlottable(c);
    m_smithGridCurves.clear();
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

