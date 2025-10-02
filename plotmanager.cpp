#include "plotmanager.h"
#include "qcustomplot.h"
#include "network.h"
#include "networkcascade.h"
#include "plotsettingsdialog.h"
#include "SmithChartGrid.h"
#include <QDebug>
#include <QVariant>
#include <QLineF>
#include <set>
#include <algorithm>
#include <limits>
#include <cmath>
#include <QSharedPointer>
#include <QLocale>
#include <QApplication>
#include <QStringList>
#include <QColor>
#include <QPen>
#include <QVector>

namespace
{
class EngineeringAxisTicker : public QCPAxisTicker
{
protected:
    QString getTickLabel(double tick, const QLocale &locale, QChar formatChar, int precision) override
    {
        Q_UNUSED(locale)
        Q_UNUSED(formatChar)
        Q_UNUSED(precision)
        return Network::formatEngineering(tick, false);
    }
};

class EngineeringAxisTickerFixed : public QCPAxisTickerFixed
{
protected:
    QString getTickLabel(double tick, const QLocale &locale, QChar formatChar, int precision) override
    {
        Q_UNUSED(locale)
        Q_UNUSED(formatChar)
        Q_UNUSED(precision)
        return Network::formatEngineering(tick, false);
    }
};

class EngineeringAxisTickerLog : public QCPAxisTickerLog
{
protected:
    QString getTickLabel(double tick, const QLocale &locale, QChar formatChar, int precision) override
    {
        Q_UNUSED(locale)
        Q_UNUSED(formatChar)
        Q_UNUSED(precision)
        return Network::formatEngineering(tick, false);
    }
};
}

using namespace std;

PlotManager::PlotManager(QCustomPlot* plot, QObject *parent)
    : QObject(parent)
    , m_plot(plot)
    , m_cascade(nullptr)
    , m_color_index(0)
    , m_keepAspectConnected(false)
    , m_currentPlotType(PlotType::Magnitude)
    , m_crosshairEnabled(true)
    , m_showPlotSettingsOnRightRelease(false)
    , m_rightClickPressPos()
    , m_gridPenStyle(Qt::DotLine)
    , m_gridColor(QColor(200, 200, 200))
    , m_subGridPenStyle(Qt::NoPen)
    , m_subGridColor(QColor(220, 220, 220))
    , m_xTickAuto(true)
    , m_xTickSpacing(0.0)
    , m_yTickAuto(true)
    , m_yTickSpacing(0.0)
    , m_requestedXAxisScaleType(plot ? plot->xAxis->scaleType() : QCPAxis::stLinear)
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
    mTracerA->setVisible(false);
    mTracerA->setInterpolating(true);

    mTracerTextA = new QCPItemText(m_plot);
    mTracerTextA->setVisible(false);

    mTracerB = new QCPItemTracer(m_plot);
    mTracerB->setVisible(false);
    mTracerB->setInterpolating(true);

    mTracerTextB = new QCPItemText(m_plot);
    mTracerTextB->setVisible(false);

    m_plot->addLayer("tracers", m_plot->layer("main"), QCustomPlot::limAbove);
    mTracerA->setLayer("tracers");
    mTracerTextA->setLayer("tracers");
    mTracerB->setLayer("tracers");
    mTracerTextB->setLayer("tracers");

    mDraggedTracer = nullptr;
    mDragMode = DragMode::None;

    configureCursorStyles(m_currentPlotType);


    m_colors.append(QColor(  0,   0, 255));   // Blue
    m_colors.append(QColor(  0, 128,   0));   // Medium Green
 //   m_colors.append(QColor(255,   0,   0));   // Red reserved for math difference plot
    m_colors.append(QColor(  0, 191, 191));   // Cyan / Teal
    m_colors.append(QColor(191,   0, 191));   // Magenta / Purple
    m_colors.append(QColor(191, 191,   0));   // Olive / Yellow-Green
    m_colors.append(QColor( 64,  64,  64));   // Dark Gray
    m_colors.append(QColor(  0,  60, 110));   // Dark Blue
    m_colors.append(QColor(140,  50,  15));   // Dark Orange
    m_colors.append(QColor(145, 110,  15));   // Dark Mustard Yellow
    m_colors.append(QColor( 90,  30, 105));   // Dark Purple

    m_colors.append(QColor(0, 114, 189));   // Blue
    m_colors.append(QColor(217, 83, 25));    // Orange
    m_colors.append(QColor(237, 177, 32));   // Yellow
    m_colors.append(QColor(126, 47, 142));   // Purple
    m_colors.append(QColor(119, 172, 48));   // Green
    m_colors.append(QColor(77, 190, 238));   // Light Blue
    m_colors.append(QColor(162, 20, 47));    // Red
    m_colors.append(QColor( 70, 110,  30));   // Dark Green
    m_colors.append(QColor( 40, 120, 150));   // Dark Teal Blue
    m_colors.append(QColor(110,  15,  35));   // Dark Red
    m_colors.append(QColor(  0,   0, 160));   // Dark Royal Blue
    m_colors.append(QColor(  0, 100,   0));   // Dark Forest Green
    m_colors.append(QColor(170,   0,   0));   // Dark Crimson Red
    m_colors.append(QColor(  0, 120, 120));   // Dark Cyan / Teal
    m_colors.append(QColor(120,   0, 120));   // Dark Magenta / Violet
    m_colors.append(QColor(120, 120,   0));   // Dark Olive Green
    m_colors.append(QColor( 40,  40,  40));   // Deep Gray

    updateAxisTickers();

    if (m_plot)
    {
        if (QCPGrid *grid = m_plot->xAxis->grid())
        {
            if (!grid->visible())
                m_gridPenStyle = Qt::NoPen;
            else
                m_gridPenStyle = grid->pen().style();
            QColor penColor = grid->pen().color();
            if (penColor.isValid())
                m_gridColor = penColor;

            if (!grid->subGridVisible())
                m_subGridPenStyle = Qt::NoPen;
            else
                m_subGridPenStyle = grid->subGridPen().style();
            QColor subColor = grid->subGridPen().color();
            if (subColor.isValid())
                m_subGridColor = subColor;
        }

        applyStoredGridSettings();
    }
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

void PlotManager::setXAxisScaleType(QCPAxis::ScaleType type)
{
    m_requestedXAxisScaleType = type;
    if (m_currentPlotType == PlotType::Smith)
        return;

    if (!m_plot)
        return;

    m_plot->xAxis->setScaleType(type);
    m_plot->xAxis2->setScaleType(type);
    updateAxisTickers();
}

void PlotManager::updateAxisTickers()
{
    if (!m_plot)
        return;

    m_plot->xAxis2->setScaleType(m_plot->xAxis->scaleType());
    m_plot->yAxis2->setScaleType(m_plot->yAxis->scaleType());

    auto configureAxis = [](QCPAxis *primary, QCPAxis *secondary,
                            bool autoSpacing, double spacing)
    {
        if (!primary || !secondary)
            return;

        if (primary->scaleType() == QCPAxis::stLogarithmic)
        {
            QSharedPointer<EngineeringAxisTickerLog> logTicker(new EngineeringAxisTickerLog);
            logTicker->setLogBase(10);
            logTicker->setSubTickCount(8);
            primary->setTicker(logTicker);
            secondary->setTicker(logTicker);
            return;
        }

        if (!autoSpacing && std::isfinite(spacing) && spacing > 0.0)
        {
            QSharedPointer<EngineeringAxisTickerFixed> fixedTicker(new EngineeringAxisTickerFixed);
            fixedTicker->setTickStep(spacing);
            fixedTicker->setScaleStrategy(QCPAxisTickerFixed::ssNone);
            primary->setTicker(fixedTicker);
            secondary->setTicker(fixedTicker);
            return;
        }

        QSharedPointer<EngineeringAxisTicker> ticker(new EngineeringAxisTicker);
        primary->setTicker(ticker);
        secondary->setTicker(ticker);
    };

    bool useAutoX = m_xTickAuto || m_plot->xAxis->scaleType() != QCPAxis::stLinear;
    bool useAutoY = m_yTickAuto || m_plot->yAxis->scaleType() != QCPAxis::stLinear;

    configureAxis(m_plot->xAxis, m_plot->xAxis2, useAutoX, m_xTickSpacing);
    configureAxis(m_plot->yAxis, m_plot->yAxis2, useAutoY, m_yTickSpacing);
}

QCPAbstractPlottable* PlotManager::plot(const QVector<double> &x, const QVector<double> &y, const QPen &pen,
                       const QString &name, Network* network, PlotType type, const QString &parameterKey)
{
    if (type == PlotType::Smith)
    {
        QCPCurve *curve = new QCPCurve(m_plot->xAxis, m_plot->yAxis);
        curve->setData(x, y);
        curve->setPen(pen);
        curve->setName(name);
        curve->setProperty("network_ptr", QVariant::fromValue(reinterpret_cast<quintptr>(network)));
        curve->setProperty("sparam_key", parameterKey);
        curve->setSelectable(QCP::stWhole);
        return curve;
    }
    QCPGraph *graph = m_plot->addGraph(m_plot->xAxis, m_plot->yAxis);
    graph->setData(x, y);
    graph->setPen(pen);
    graph->setName(name);
    graph->setProperty("network_ptr", QVariant::fromValue(reinterpret_cast<quintptr>(network)));
    graph->setProperty("sparam_key", parameterKey);
    graph->setSelectable(QCP::stWhole);

    return graph;
}

void PlotManager::configureCursorStyles(PlotType type)
{
    auto configureSmithMarkers = [this]() {
        mTracerA->setStyle(QCPItemTracer::tsTriangle);
        mTracerA->setPen(QPen(Qt::red, 0));
        mTracerA->setBrush(Qt::red);
        mTracerTextA->setColor(Qt::red);

        mTracerB->setStyle(QCPItemTracer::tsSquare);
        mTracerB->setPen(QPen(Qt::blue, 0));
        mTracerB->setBrush(Qt::blue);
        mTracerTextB->setColor(Qt::blue);
    };

    if (type == PlotType::Smith)
    {
        configureSmithMarkers();
    }
    else if (m_crosshairEnabled)
    {
        mTracerA->setStyle(QCPItemTracer::tsCrosshair);
        mTracerA->setPen(QPen(Qt::black, 0));
        mTracerA->setBrush(Qt::NoBrush);
        mTracerTextA->setColor(Qt::red);

        mTracerB->setStyle(QCPItemTracer::tsCrosshair);
        mTracerB->setPen(QPen(Qt::darkGray, 0));
        mTracerB->setBrush(Qt::NoBrush);
        mTracerTextB->setColor(Qt::blue);
    }
    else
    {
        configureSmithMarkers();
    }
}

void PlotManager::setCrosshairEnabled(bool enabled)
{
    if (m_crosshairEnabled == enabled)
        return;

    m_crosshairEnabled = enabled;
    configureCursorStyles(m_currentPlotType);
    m_plot->replot();
}

QCPGraph *PlotManager::firstGraph() const
{
    for (int i = 0; i < m_plot->plottableCount(); ++i)
    {
        if (QCPGraph *graph = qobject_cast<QCPGraph*>(m_plot->plottable(i)))
            return graph;
    }
    return nullptr;
}

QCPCurve *PlotManager::firstSmithCurve() const
{
    for (int i = 0; i < m_plot->plottableCount(); ++i)
    {
        if (QCPCurve *curve = qobject_cast<QCPCurve*>(m_plot->plottable(i)))
        {
            if (m_curveFreqs.contains(curve))
                return curve;
        }
    }
    return nullptr;
}

QCPCurve *PlotManager::smithCurveAt(const QPoint &pos) const
{
    QCPAbstractPlottable *pl = m_plot->plottableAt(pos, true);
    if (QCPCurve *curve = qobject_cast<QCPCurve*>(pl))
    {
        if (m_curveFreqs.contains(curve))
            return curve;
    }
    return nullptr;
}

QCPGraph *PlotManager::graphByName(const QString &name) const
{
    if (!m_plot)
        return nullptr;

    for (int i = 0; i < m_plot->plottableCount(); ++i)
    {
        QCPAbstractPlottable *pl = m_plot->plottable(i);
        if (!pl)
            continue;
        if (pl->name() == name)
        {
            if (QCPGraph *graph = qobject_cast<QCPGraph*>(pl))
                return graph;
            return nullptr;
        }
    }
    return nullptr;
}

bool PlotManager::computeMathPlotData(QCPGraph *graph1, QCPGraph *graph2,
                                      QVector<double> &x, QVector<double> &y) const
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

    x = diffX;
    y = diffY;
    return !x.isEmpty();
}

void PlotManager::updateMathPlots()
{
    if (!m_plot)
        return;

    QList<QCPAbstractPlottable*> toRemove;

    auto metaForGraph = [](QCPGraph *graph) -> QVariantMap
    {
        QVariantMap meta;
        if (!graph)
            return meta;
        meta.insert(QStringLiteral("network_ptr"), graph->property("network_ptr"));
        meta.insert(QStringLiteral("sparam_key"), graph->property("sparam_key"));
        return meta;
    };

    auto resolveGraphFromMeta = [&](const QVariant &metaVariant) -> QCPGraph*
    {
        QVariantMap meta = metaVariant.toMap();
        if (meta.isEmpty())
            return nullptr;

        QVariant networkVariant = meta.value(QStringLiteral("network_ptr"));
        QString sparamKey = meta.value(QStringLiteral("sparam_key")).toString();
        if (!networkVariant.isValid() || sparamKey.isEmpty())
            return nullptr;

        for (int j = 0; j < m_plot->plottableCount(); ++j)
        {
            if (QCPGraph *graph = qobject_cast<QCPGraph*>(m_plot->plottable(j)))
            {
                if (graph->property("math_plot").toBool())
                    continue;
                if (graph->property("network_ptr") == networkVariant &&
                    graph->property("sparam_key").toString() == sparamKey)
                {
                    return graph;
                }
            }
        }

        return nullptr;
    };

    for (int i = 0; i < m_plot->plottableCount(); ++i)
    {
        QCPGraph *mathGraph = qobject_cast<QCPGraph*>(m_plot->plottable(i));
        if (!mathGraph)
            continue;
        if (!mathGraph->property("math_plot").toBool())
            continue;

        QVector<QCPGraph*> resolvedGraphs;
        QStringList resolvedNames;
        QVariantList resolvedMeta;

        QVariantList storedMeta = mathGraph->property("math_plot_source_meta").toList();
        if (storedMeta.size() == 2)
        {
            for (const QVariant &metaVariant : storedMeta)
            {
                if (QCPGraph *sourceGraph = resolveGraphFromMeta(metaVariant))
                {
                    resolvedGraphs.append(sourceGraph);
                    resolvedNames.append(sourceGraph->name());
                    resolvedMeta.append(metaForGraph(sourceGraph));
                }
            }
        }

        if (resolvedGraphs.size() != 2)
        {
            resolvedGraphs.clear();
            resolvedNames.clear();
            resolvedMeta.clear();

            QStringList sources = mathGraph->property("math_plot_sources").toStringList();
            if (sources.size() == 2)
            {
                for (const QString &name : sources)
                {
                    if (QCPGraph *sourceGraph = graphByName(name))
                    {
                        resolvedGraphs.append(sourceGraph);
                        resolvedNames.append(sourceGraph->name());
                        resolvedMeta.append(metaForGraph(sourceGraph));
                    }
                }
            }
        }

        if (resolvedGraphs.size() != 2)
        {
            toRemove.append(mathGraph);
            continue;
        }

        QVector<double> x;
        QVector<double> y;
        if (computeMathPlotData(resolvedGraphs.at(0), resolvedGraphs.at(1), x, y) && !x.isEmpty())
        {
            mathGraph->setData(x, y);
            mathGraph->setName(QStringLiteral("%1 - %2").arg(resolvedGraphs.at(0)->name())
                                                   .arg(resolvedGraphs.at(1)->name()));
            mathGraph->setProperty("math_plot_sources", resolvedNames);
            mathGraph->setProperty("math_plot_source_meta", resolvedMeta);
        }
        else
        {
            toRemove.append(mathGraph);
        }
    }

    for (QCPAbstractPlottable *pl : toRemove)
    {
        if (pl)
            m_plot->removePlottable(pl);
    }
}

void PlotManager::updatePlots(const QStringList& sparams, PlotType type)
{
#ifdef FSNPVIEW_ENABLE_PLOT_DEBUG
    qDebug() << "  updatePlots()";
#endif
    PlotType previousPlotType = m_currentPlotType;

    auto suffixForType = [](PlotType plotType) -> QString
    {
        switch (plotType)
        {
        case PlotType::Magnitude:
            return QString();
        case PlotType::Phase:
            return QStringLiteral("_phase");
        case PlotType::GroupDelay:
            return QStringLiteral("_gdelay");
        case PlotType::VSWR:
            return QStringLiteral("_vswr");
        case PlotType::Smith:
            return QStringLiteral("_smith");
        case PlotType::TDR:
            return QStringLiteral("_tdr");
        }
        return QString();
    };

    auto baseNameFor = [&](const QString &name, PlotType plotType) -> QString
    {
        if (name.isEmpty())
            return QString();
        QString suffix = suffixForType(plotType);
        if (suffix.isEmpty())
            return name;
        if (name.endsWith(suffix))
            return name.left(name.size() - suffix.size());
        return QString();
    };

    struct TracerState
    {
        bool visible = false;
        QString baseName;
        double frequency = std::numeric_limits<double>::quiet_NaN();
    };

    auto captureTracerState = [&](QCPItemTracer *tracer) -> TracerState
    {
        TracerState state;
        if (!tracer || !tracer->visible())
            return state;

        state.visible = true;

        if (previousPlotType == PlotType::Smith)
        {
            QCPCurve *curve = m_tracerCurves.value(tracer, nullptr);
            int idx = m_tracerIndices.value(tracer, -1);
            if (curve)
            {
                state.baseName = baseNameFor(curve->name(), previousPlotType);
                if (idx >= 0)
                {
                    const QVector<double> freqs = m_curveFreqs.value(curve);
                    if (idx < freqs.size())
                        state.frequency = freqs.at(idx);
                }
            }
        }
        else
        {
            if (QCPGraph *graph = tracer->graph())
            {
                state.baseName = baseNameFor(graph->name(), previousPlotType);
                state.frequency = tracer->graphKey();
            }
        }

        return state;
    };

    TracerState tracerAState = captureTracerState(mTracerA);
    TracerState tracerBState = captureTracerState(mTracerB);

    QStringList required_graphs;
    QString suffix;

    switch (type) {
    case PlotType::Magnitude:
        m_plot->yAxis->setLabel("Magnitude (dB)");
        m_plot->xAxis->setLabel("Frequency (Hz)");
        break;
    case PlotType::Phase:
        m_plot->yAxis->setLabel("Phase (deg)");
        m_plot->xAxis->setLabel("Frequency (Hz)");
        break;
    case PlotType::GroupDelay:
        m_plot->yAxis->setLabel("Group Delay (s)");
        m_plot->xAxis->setLabel("Frequency (Hz)");
        break;
    case PlotType::VSWR:
        m_plot->yAxis->setLabel("VSWR");
        m_plot->xAxis->setLabel("Frequency (Hz)");
        break;
    case PlotType::Smith:
        m_plot->yAxis->setLabel("Imag");
        m_plot->xAxis->setLabel("Real");
        break;
    case PlotType::TDR:
        m_plot->yAxis->setLabel(QString::fromUtf8("Impedance (Ω)"));
        m_plot->xAxis->setLabel("Distance (m)");
        m_plot->xAxis->setScaleType(QCPAxis::stLinear);
        m_plot->xAxis2->setScaleType(QCPAxis::stLinear);
        updateAxisTickers();
        break;
    }

    suffix = suffixForType(type);

    m_currentPlotType = type;
    configureCursorStyles(type);

    if (type == PlotType::Smith) {
        if (m_plot) {
            m_plot->xAxis->setScaleType(QCPAxis::stLinear);
            m_plot->xAxis2->setScaleType(QCPAxis::stLinear);
            m_plot->yAxis->setScaleType(QCPAxis::stLinear);
            m_plot->yAxis2->setScaleType(QCPAxis::stLinear);
            updateAxisTickers();
        }
        setupSmithGrid();
    } else {
        clearSmithGrid();
        clearSmithMarkers();
        m_curveFreqs.clear();
        m_tracerCurves.clear();
        m_tracerIndices.clear();
        if (m_plot) {
            m_plot->xAxis->setScaleType(m_requestedXAxisScaleType);
            m_plot->xAxis2->setScaleType(m_requestedXAxisScaleType);
            updateAxisTickers();
        }
        m_plot->xAxis->setTicks(true);
        m_plot->yAxis->setTicks(true);
        m_plot->xAxis->setTickLabels(true);
        m_plot->yAxis->setTickLabels(true);
        m_plot->xAxis->grid()->setVisible(true);
        m_plot->yAxis->grid()->setVisible(true);
    }

    bool cascadeHasActive = false;
    if (m_cascade) {
        const QList<Network*> cascadeNetworks = m_cascade->getNetworks();
        for (Network *network : cascadeNetworks) {
            if (network && network->isActive()) {
                cascadeHasActive = true;
                break;
            }
        }
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
    if (cascadeHasActive) {
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
        if (pl->property("math_plot").toBool())
            continue;

        if (!required_graphs.contains(pl->name())) {
            if (QCPCurve *curve = qobject_cast<QCPCurve*>(pl)) {
                m_curveFreqs.remove(curve);
                if (m_tracerCurves.value(mTracerA) == curve)
                    m_tracerCurves.remove(mTracerA), m_tracerIndices.remove(mTracerA);
                if (m_tracerCurves.value(mTracerB) == curve)
                    m_tracerCurves.remove(mTracerB), m_tracerIndices.remove(mTracerB);
            }
            m_plot->removePlottable(pl);
        }
    }

    if (type == PlotType::Smith)
        clearSmithMarkers();

    auto parseSparamPorts = [](const QString &sparam, int &outputPort, int &inputPort) -> bool
    {
        if (sparam.size() < 3)
            return false;
        if (sparam.at(0).toLower() != QLatin1Char('s'))
            return false;

        QString portsPart = sparam.mid(1);
        if (portsPart.size() % 2 != 0)
            return false;

        int half = portsPart.size() / 2;
        if (half <= 0)
            return false;

        bool okOutput = false;
        bool okInput = false;
        outputPort = portsPart.left(half).toInt(&okOutput);
        inputPort = portsPart.mid(half).toInt(&okInput);
        return okOutput && okInput;
    };

    auto indexForNetwork = [&](Network *network, const QString &sparam) -> int
    {
        if (!network)
            return -1;

        int outputPort = 0;
        int inputPort = 0;
        if (!parseSparamPorts(sparam, outputPort, inputPort))
            return -1;

        int ports = network->portCount();
        if (ports <= 0)
            return -1;

        if (outputPort < 1 || inputPort < 1 || outputPort > ports || inputPort > ports)
            return -1;

        return (inputPort - 1) * ports + (outputPort - 1);
    };

    for (const auto& sparam : sparams) {

        // Individual networks
        for (auto network : qAsConst(m_networks)) {
            if (!network->isVisible())
                continue;

            QString graph_name = network->name() + "_" + sparam + suffix;
            QPen pen = network->parameterPen(sparam);
            QCPAbstractPlottable *pl = nullptr;
            for (int i = 0; i < m_plot->plottableCount(); ++i) {
                if (m_plot->plottable(i)->name() == graph_name) {
                    pl = m_plot->plottable(i);
                    break;
                }
            }

            QPair<QVector<double>, QVector<double>> plotData;
            int sparam_idx_to_plot = indexForNetwork(network, sparam);
            if (sparam_idx_to_plot >= 0)
                plotData = network->getPlotData(sparam_idx_to_plot, type);
            if (plotData.first.isEmpty() || plotData.second.isEmpty()) {
                if (pl) {
                    if (type == PlotType::Smith) {
                        if (QCPCurve *curve = qobject_cast<QCPCurve*>(pl)) {
                            m_curveFreqs.remove(curve);
                            if (m_tracerCurves.value(mTracerA) == curve)
                                m_tracerCurves.remove(mTracerA), m_tracerIndices.remove(mTracerA);
                            if (m_tracerCurves.value(mTracerB) == curve)
                                m_tracerCurves.remove(mTracerB), m_tracerIndices.remove(mTracerB);
                        }
                    }
                    m_plot->removePlottable(pl);
                }
                continue;
            }
            QVector<double> freqs = network->frequencies();
            if (pl) {
                pl->setProperty("network_ptr", QVariant::fromValue(reinterpret_cast<quintptr>(network)));
                pl->setProperty("sparam_key", sparam);
                if (type == PlotType::Smith) {
                    if (QCPCurve *curve = qobject_cast<QCPCurve*>(pl)) {
                        curve->setData(plotData.first, plotData.second);
                        curve->setPen(pen);
                        m_curveFreqs[curve] = freqs;
                    }
                } else {
                    if (QCPGraph *graph = qobject_cast<QCPGraph*>(pl)) {
                        graph->setData(plotData.first, plotData.second);
                        graph->setPen(pen);
                    }
                }
            } else {
                pl = plot(plotData.first, plotData.second,
                              pen, graph_name, network, type, sparam);
                if (pl) {
                    pl->setProperty("sparam_key", sparam);
                }
                if (type == PlotType::Smith)
                    if (QCPCurve *curve = qobject_cast<QCPCurve*>(pl))
                        m_curveFreqs[curve] = freqs;
            }

            if (type == PlotType::Smith)
                addSmithMarkers(plotData.first, plotData.second, pen.color());
        }

        // Cascade
        if (cascadeHasActive) {
            QPen pen = m_cascade->parameterPen(sparam);
            QColor cascadeColor = pen.color();
            if (!cascadeColor.isValid()) {
                cascadeColor = Qt::magenta;
                pen.setColor(cascadeColor);
            }
            QString graph_name = m_cascade->name() + "_" + sparam + suffix;
            QCPAbstractPlottable *pl = nullptr;
            for (int i = 0; i < m_plot->plottableCount(); ++i) {
                if (m_plot->plottable(i)->name() == graph_name) {
                    pl = m_plot->plottable(i);
                    break;
                }
            }

            QPair<QVector<double>, QVector<double>> plotData;
            int sparam_idx_to_plot = indexForNetwork(m_cascade, sparam);
            if (sparam_idx_to_plot >= 0)
                plotData = m_cascade->getPlotData(sparam_idx_to_plot, type);
            if (plotData.first.isEmpty() || plotData.second.isEmpty()) {
                if (pl) {
                    if (type == PlotType::Smith) {
                        if (QCPCurve *curve = qobject_cast<QCPCurve*>(pl)) {
                            m_curveFreqs.remove(curve);
                            if (m_tracerCurves.value(mTracerA) == curve)
                                m_tracerCurves.remove(mTracerA), m_tracerIndices.remove(mTracerA);
                            if (m_tracerCurves.value(mTracerB) == curve)
                                m_tracerCurves.remove(mTracerB), m_tracerIndices.remove(mTracerB);
                        }
                    }
                    m_plot->removePlottable(pl);
                }
                continue;
            }
            QVector<double> freqs = m_cascade->frequencies();
            if (pl) {
                pl->setProperty("network_ptr", QVariant::fromValue(reinterpret_cast<quintptr>(m_cascade)));
                pl->setProperty("sparam_key", sparam);
                if (type == PlotType::Smith) {
                    if (QCPCurve *curve = qobject_cast<QCPCurve*>(pl)) {
                        curve->setData(plotData.first, plotData.second);
                        curve->setPen(pen);
                        m_curveFreqs[curve] = freqs;
                    }
                } else {
                    if (QCPGraph *graph = qobject_cast<QCPGraph*>(pl)) {
                        graph->setData(plotData.first, plotData.second);
                        graph->setPen(pen);
                    }
                }
            } else {
                pl = plot(plotData.first, plotData.second, pen,
                              graph_name, m_cascade, type, sparam);
                if (pl) {
                    pl->setProperty("sparam_key", sparam);
                }
                if (type == PlotType::Smith)
                    if (QCPCurve *curve = qobject_cast<QCPCurve*>(pl))
                        m_curveFreqs[curve] = freqs;
            }
            if (type == PlotType::Smith)
                addSmithMarkers(plotData.first, plotData.second, cascadeColor);
        }
    }

    auto nameForCurrentType = [&](const TracerState &state) -> QString
    {
        if (state.baseName.isEmpty())
            return QString();
        return state.baseName + suffix;
    };

    auto findPlottableByName = [&](const QString &name) -> QCPAbstractPlottable*
    {
        if (name.isEmpty())
            return nullptr;
        for (int i = 0; i < m_plot->plottableCount(); ++i)
        {
            if (m_plot->plottable(i)->name() == name)
                return m_plot->plottable(i);
        }
        return nullptr;
    };

    auto restoreCartesianTracer = [&](QCPItemTracer *tracer, const TracerState &state)
    {
        m_tracerCurves.remove(tracer);
        m_tracerIndices.remove(tracer);

        if (!tracer)
            return;

        if (!state.visible)
        {
            tracer->setGraph(nullptr);
            return;
        }

        QCPGraph *targetGraph = nullptr;
        if (QCPAbstractPlottable *pl = findPlottableByName(nameForCurrentType(state)))
            targetGraph = qobject_cast<QCPGraph*>(pl);

        if (!targetGraph)
            targetGraph = firstGraph();

        if (targetGraph)
        {
            tracer->setGraph(targetGraph);
            double key = state.frequency;
            if (!std::isfinite(key))
            {
                key = m_plot->xAxis->range().center();
            }
            else
            {
                auto data = targetGraph->data();
                if (!data->isEmpty())
                {
                    double minKey = data->constBegin()->key;
                    auto itEnd = data->constEnd();
                    --itEnd;
                    double maxKey = itEnd->key;
                    if (key < minKey)
                        key = minKey;
                    else if (key > maxKey)
                        key = maxKey;
                }
            }
            tracer->setGraphKey(key);
        }
        else
        {
            tracer->setGraph(nullptr);
            tracer->position->setType(QCPItemPosition::ptPlotCoords);
            tracer->position->setCoords(m_plot->xAxis->range().center(),
                                        m_plot->yAxis->range().center());
        }
    };

    auto restoreSmithTracer = [&](QCPItemTracer *tracer, const TracerState &state)
    {
        m_tracerCurves.remove(tracer);
        m_tracerIndices.remove(tracer);

        if (!tracer)
            return;

        tracer->setGraph(nullptr);
        tracer->position->setType(QCPItemPosition::ptPlotCoords);

        if (!state.visible)
            return;

        QCPCurve *targetCurve = nullptr;
        if (QCPAbstractPlottable *pl = findPlottableByName(nameForCurrentType(state)))
            targetCurve = qobject_cast<QCPCurve*>(pl);

        if (!targetCurve)
            targetCurve = firstSmithCurve();

        if (targetCurve)
        {
            auto data = targetCurve->data();
            if (!data->isEmpty())
            {
                QVector<double> freqs = m_curveFreqs.value(targetCurve);
                int index = 0;
                if (!freqs.isEmpty() && std::isfinite(state.frequency))
                {
                    auto lower = std::lower_bound(freqs.constBegin(), freqs.constEnd(), state.frequency);
                    if (lower == freqs.constEnd())
                        index = freqs.size() - 1;
                    else if (lower == freqs.constBegin())
                        index = 0;
                    else
                    {
                        index = lower - freqs.constBegin();
                        double lowerDiff = qAbs(*lower - state.frequency);
                        auto prev = lower;
                        --prev;
                        double prevDiff = qAbs(state.frequency - *prev);
                        if (prevDiff <= lowerDiff)
                            index = prev - freqs.constBegin();
                    }
                }

                int dataSize = data->size();
                if (index < 0)
                    index = 0;
                else if (index >= dataSize)
                    index = dataSize - 1;

                auto it = data->constBegin();
                std::advance(it, index);
                tracer->position->setCoords(it->key, it->value);
                m_tracerCurves[tracer] = targetCurve;
                m_tracerIndices[tracer] = index;
                return;
            }
        }

        tracer->position->setCoords(m_plot->xAxis->range().center(),
                                    m_plot->yAxis->range().center());
    };

    if (type != PlotType::Smith)
    {
        restoreCartesianTracer(mTracerA, tracerAState);
        restoreCartesianTracer(mTracerB, tracerBState);
    }
    else
    {
        restoreSmithTracer(mTracerA, tracerAState);
        restoreSmithTracer(mTracerB, tracerBState);
    }

    updateMathPlots();

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

    if (!visible)
    {
        m_tracerCurves.remove(mTracerA);
        m_tracerIndices.remove(mTracerA);
    }
    else if (m_currentPlotType == PlotType::Smith)
    {
        mTracerA->setGraph(nullptr);
        mTracerA->position->setType(QCPItemPosition::ptPlotCoords);

        if (QCPCurve *curve = firstSmithCurve())
        {
            auto data = curve->data();
            if (!data->isEmpty())
            {
                auto it = data->constBegin();
                mTracerA->position->setCoords(it->key, it->value);
                m_tracerCurves[mTracerA] = curve;
                m_tracerIndices[mTracerA] = 0;
            }
            else
            {
                m_tracerCurves.remove(mTracerA);
                m_tracerIndices.remove(mTracerA);
            }
        }
        else
        {
            m_tracerCurves.remove(mTracerA);
            m_tracerIndices.remove(mTracerA);
            mTracerA->position->setCoords(m_plot->xAxis->range().center(),
                                          m_plot->yAxis->range().center());
        }
    }
    else
    {
        m_tracerCurves.remove(mTracerA);
        m_tracerIndices.remove(mTracerA);

        if (QCPGraph *graph = firstGraph())
        {
            mTracerA->setGraph(graph);
            mTracerA->setGraphKey(m_plot->xAxis->range().center());
        }
        else
        {
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

    if (!visible)
    {
        m_tracerCurves.remove(mTracerB);
        m_tracerIndices.remove(mTracerB);
    }
    else if (m_currentPlotType == PlotType::Smith)
    {
        mTracerB->setGraph(nullptr);
        mTracerB->position->setType(QCPItemPosition::ptPlotCoords);

        if (QCPCurve *curve = firstSmithCurve())
        {
            auto data = curve->data();
            if (!data->isEmpty())
            {
                auto it = data->constBegin();
                mTracerB->position->setCoords(it->key, it->value);
                m_tracerCurves[mTracerB] = curve;
                m_tracerIndices[mTracerB] = 0;
            }
            else
            {
                m_tracerCurves.remove(mTracerB);
                m_tracerIndices.remove(mTracerB);
            }
        }
        else
        {
            m_tracerCurves.remove(mTracerB);
            m_tracerIndices.remove(mTracerB);
            mTracerB->position->setCoords(m_plot->xAxis->range().center(),
                                          m_plot->yAxis->range().center());
        }
    }
    else
    {
        m_tracerCurves.remove(mTracerB);
        m_tracerIndices.remove(mTracerB);

        if (QCPGraph *graph = firstGraph())
        {
            mTracerB->setGraph(graph);
            mTracerB->setGraphKey(m_plot->xAxis->range().center());
        }
        else
        {
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
    if (event->button() == Qt::RightButton)
    {
        m_rightClickPressPos = event->pos();
        m_showPlotSettingsOnRightRelease = true;
        return;
    }

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

QString PlotManager::markerLabelText(const QString &markerName) const
{
    QString unit;
    switch (m_currentPlotType)
    {
    case PlotType::TDR:
        unit = tr("Distance (m)");
        break;
    case PlotType::Smith:
        unit = tr("Frequency (Hz)");
        break;
    default:
        unit = tr("Frequency (Hz)");
        break;
    }

    if (unit.isEmpty())
        return markerName;
    return QStringLiteral("%1 (%2)").arg(markerName, unit);
}

void PlotManager::showPlotSettingsDialog()
{
    if (!m_plot)
        return;

    PlotSettingsDialog dialog(m_plot);
    dialog.setAxisLabels(m_plot->xAxis->label(), m_plot->yAxis->label());
    dialog.setXAxisRange(m_plot->xAxis->range().lower, m_plot->xAxis->range().upper);
    dialog.setYAxisRange(m_plot->yAxis->range().lower, m_plot->yAxis->range().upper);
    dialog.setMarkerLabelTexts(markerLabelText(tr("Marker A position")),
                               markerLabelText(tr("Marker B position")));
    dialog.setMarkerValues(markerValue(mTracerA), mTracerA && mTracerA->visible(),
                           markerValue(mTracerB), mTracerB && mTracerB->visible());

    QPen gridPen(m_gridColor.isValid() ? m_gridColor : QColor(200, 200, 200));
    gridPen.setWidthF(0.0);
    gridPen.setStyle(m_gridPenStyle == Qt::NoPen ? Qt::DotLine : m_gridPenStyle);
    QPen subGridPen(m_subGridColor.isValid() ? m_subGridColor : QColor(220, 220, 220));
    subGridPen.setWidthF(0.0);
    subGridPen.setStyle(m_subGridPenStyle == Qt::NoPen ? Qt::DotLine : m_subGridPenStyle);
    dialog.setGridSettings(gridPen, m_gridPenStyle != Qt::NoPen,
                           subGridPen, m_subGridPenStyle != Qt::NoPen);

    bool xManualEnabled = m_plot->xAxis->scaleType() == QCPAxis::stLinear;
    bool yManualEnabled = m_plot->yAxis->scaleType() == QCPAxis::stLinear;
    double xAutoSpacing = currentTickStep(m_plot->xAxis);
    double yAutoSpacing = currentTickStep(m_plot->yAxis);
    dialog.setXAxisTickSpacing(m_xTickSpacing, m_xTickAuto || !xManualEnabled,
                               xAutoSpacing, xManualEnabled);
    dialog.setYAxisTickSpacing(m_yTickSpacing, m_yTickAuto || !yManualEnabled,
                               yAutoSpacing, yManualEnabled);

    if (dialog.exec() == QDialog::Accepted)
        applySettingsFromDialog(dialog);
}

void PlotManager::applySettingsFromDialog(const PlotSettingsDialog &dialog)
{
    applyAxisRanges(dialog);
    applyGridSettings(dialog);
    applyTickSettings(dialog);
    applyMarkerPositions(dialog);
    updateTracers();
    if (m_plot)
        m_plot->replot();
}

void PlotManager::applyAxisRanges(const PlotSettingsDialog &dialog)
{
    double xMin = dialog.xMinimum();
    double xMax = dialog.xMaximum();
    double yMin = dialog.yMinimum();
    double yMax = dialog.yMaximum();

    if (std::isfinite(xMin) && std::isfinite(xMax) && xMax > xMin)
        m_plot->xAxis->setRange(xMin, xMax);
    if (std::isfinite(yMin) && std::isfinite(yMax) && yMax > yMin)
        m_plot->yAxis->setRange(yMin, yMax);
}

void PlotManager::applyMarkerPositions(const PlotSettingsDialog &dialog)
{
    if (dialog.markerAIsEnabled())
        setMarkerValue(mTracerA, dialog.markerAValue());
    if (dialog.markerBIsEnabled())
        setMarkerValue(mTracerB, dialog.markerBValue());
}

void PlotManager::applyGridSettings(const PlotSettingsDialog &dialog)
{
    if (!m_plot)
        return;

    Qt::PenStyle gridStyle = dialog.gridPenStyle();
    Qt::PenStyle subGridStyle = dialog.subGridPenStyle();

    QColor gridColor = dialog.gridColor();
    if (!gridColor.isValid())
        gridColor = QColor(200, 200, 200);
    QColor subGridColor = dialog.subGridColor();
    if (!subGridColor.isValid())
        subGridColor = QColor(220, 220, 220);

    m_gridPenStyle = gridStyle;
    m_gridColor = gridColor;
    m_subGridPenStyle = subGridStyle;
    m_subGridColor = subGridColor;

    applyStoredGridSettings();
}

void PlotManager::applyTickSettings(const PlotSettingsDialog &dialog)
{
    if (m_plot && m_plot->xAxis->scaleType() == QCPAxis::stLinear)
    {
        if (dialog.xTickSpacingIsAutomatic())
        {
            m_xTickAuto = true;
        }
        else
        {
            double spacing = dialog.xTickSpacing();
            if (std::isfinite(spacing) && spacing > 0.0)
            {
                m_xTickAuto = false;
                m_xTickSpacing = spacing;
            }
            else
            {
                m_xTickAuto = true;
            }
        }
    }

    if (m_plot && m_plot->yAxis->scaleType() == QCPAxis::stLinear)
    {
        if (dialog.yTickSpacingIsAutomatic())
        {
            m_yTickAuto = true;
        }
        else
        {
            double spacing = dialog.yTickSpacing();
            if (std::isfinite(spacing) && spacing > 0.0)
            {
                m_yTickAuto = false;
                m_yTickSpacing = spacing;
            }
            else
            {
                m_yTickAuto = true;
            }
        }
    }

    updateAxisTickers();
}

void PlotManager::applyStoredGridSettings()
{
    if (!m_plot)
        return;

    auto applyToAxis = [&](QCPAxis *axis)
    {
        if (!axis)
            return;
        if (QCPGrid *grid = axis->grid())
        {
            bool gridVisible = m_gridPenStyle != Qt::NoPen;
            grid->setVisible(gridVisible);
            QPen pen = gridVisible ? grid->pen() : QPen(Qt::NoPen);
            if (gridVisible)
            {
                pen.setColor(m_gridColor);
                pen.setStyle(m_gridPenStyle);
                pen.setWidthF(0.0);
            }
            grid->setPen(pen);

            bool subVisible = m_subGridPenStyle != Qt::NoPen;
            grid->setSubGridVisible(subVisible);
            QPen subPen = subVisible ? grid->subGridPen() : QPen(Qt::NoPen);
            if (subVisible)
            {
                subPen.setColor(m_subGridColor);
                subPen.setStyle(m_subGridPenStyle);
                subPen.setWidthF(0.0);
            }
            grid->setSubGridPen(subPen);
        }
    };

    applyToAxis(m_plot->xAxis);
    applyToAxis(m_plot->yAxis);
    applyToAxis(m_plot->xAxis2);
    applyToAxis(m_plot->yAxis2);
}

double PlotManager::currentTickStep(const QCPAxis *axis) const
{
    if (!axis || !axis->ticker())
        return std::numeric_limits<double>::quiet_NaN();

    QVector<double> ticks;
    axis->ticker()->generate(axis->range(), axis->parentPlot()->locale(),
                             axis->numberFormat().isEmpty() ? QLatin1Char('g') : axis->numberFormat().at(0),
                             axis->numberPrecision(), ticks, nullptr, nullptr);

    if (ticks.size() < 2)
        return std::numeric_limits<double>::quiet_NaN();

    double minDiff = std::numeric_limits<double>::infinity();
    for (int i = 1; i < ticks.size(); ++i)
    {
        double diff = ticks.at(i) - ticks.at(i - 1);
        diff = std::abs(diff);
        if (diff > 0.0 && diff < minDiff)
            minDiff = diff;
    }

    if (!std::isfinite(minDiff) || minDiff <= 0.0)
        return std::numeric_limits<double>::quiet_NaN();

    return minDiff;
}

double PlotManager::markerValue(const QCPItemTracer *tracer) const
{
    if (!tracer || !tracer->visible())
        return std::numeric_limits<double>::quiet_NaN();

    if (m_currentPlotType == PlotType::Smith)
    {
        QCPCurve *curve = m_tracerCurves.value(const_cast<QCPItemTracer *>(tracer), nullptr);
        int index = m_tracerIndices.value(const_cast<QCPItemTracer *>(tracer), -1);
        if (curve && index >= 0)
        {
            const QVector<double> freqs = m_curveFreqs.value(curve);
            if (index < freqs.size())
                return freqs.at(index);
        }
        return std::numeric_limits<double>::quiet_NaN();
    }

    if (const QCPGraph *graph = tracer->graph())
        return tracer->graphKey();

    return tracer->position->coords().x();
}

void PlotManager::setMarkerValue(QCPItemTracer *tracer, double value)
{
    if (!tracer || !tracer->visible() || !std::isfinite(value))
        return;

    if (m_currentPlotType == PlotType::Smith)
        setSmithMarkerFrequency(tracer, value);
    else
        setCartesianMarkerValue(tracer, value);
}

void PlotManager::setCartesianMarkerValue(QCPItemTracer *tracer, double value)
{
    if (!tracer)
        return;

    m_tracerCurves.remove(tracer);
    m_tracerIndices.remove(tracer);

    QCPGraph *targetGraph = tracer->graph();
    if (!targetGraph)
        targetGraph = firstGraph();

    if (targetGraph)
    {
        auto data = targetGraph->data();
        if (!data->isEmpty())
        {
            double minKey = data->constBegin()->key;
            auto itEnd = data->constEnd();
            --itEnd;
            double maxKey = itEnd->key;
            if (value < minKey)
                value = minKey;
            else if (value > maxKey)
                value = maxKey;
        }
        tracer->setGraph(targetGraph);
        tracer->setGraphKey(value);
    }
    else
    {
        tracer->setGraph(nullptr);
        tracer->position->setType(QCPItemPosition::ptPlotCoords);
        tracer->position->setCoords(value, m_plot->yAxis->range().center());
    }
}

void PlotManager::setSmithMarkerFrequency(QCPItemTracer *tracer, double frequency)
{
    if (!tracer)
        return;

    tracer->setGraph(nullptr);
    tracer->position->setType(QCPItemPosition::ptPlotCoords);

    QCPCurve *targetCurve = m_tracerCurves.value(tracer, nullptr);
    if (!targetCurve)
        targetCurve = firstSmithCurve();

    if (!targetCurve)
        return;

    auto data = targetCurve->data();
    if (data->isEmpty())
        return;

    QVector<double> freqs = m_curveFreqs.value(targetCurve);
    int index = 0;
    if (!freqs.isEmpty())
    {
        auto lower = std::lower_bound(freqs.constBegin(), freqs.constEnd(), frequency);
        if (lower == freqs.constEnd())
            index = freqs.size() - 1;
        else if (lower == freqs.constBegin())
            index = 0;
        else
        {
            index = lower - freqs.constBegin();
            double lowerDiff = qAbs(*lower - frequency);
            auto prev = lower;
            --prev;
            double prevDiff = qAbs(frequency - *prev);
            if (prevDiff <= lowerDiff)
                index = prev - freqs.constBegin();
        }
    }

    if (index < 0)
        index = 0;
    else if (index >= data->size())
        index = data->size() - 1;

    auto it = data->constBegin();
    std::advance(it, index);
    tracer->position->setCoords(it->key, it->value);
    m_tracerCurves[tracer] = targetCurve;
    m_tracerIndices[tracer] = index;
}

void PlotManager::checkForTracerDrag(QMouseEvent *event, QCPItemTracer *tracer)
{
    if (!tracer->visible())
        return;

    const QPointF tracerPos = tracer->position->pixelPosition();
    if (!m_smithGridCurves.isEmpty()) {
        if (QLineF(event->pos(), tracerPos).length() < 8) {
            mDraggedTracer = tracer;
            mDragMode = DragMode::Curve;
            if (QCPCurve *curve = smithCurveAt(event->pos()))
            {
                m_tracerCurves[tracer] = curve;
            }
            else if (!m_tracerCurves.contains(tracer))
            {
                if (QCPCurve *existing = firstSmithCurve())
                    m_tracerCurves[tracer] = existing;
            }
        }
    } else {
        if (!m_crosshairEnabled) {
            if (QLineF(event->pos(), tracerPos).length() < 8) {
                mDraggedTracer = tracer;
                mDragMode = DragMode::Horizontal;
            }
        } else if (qAbs(event->pos().x() - tracerPos.x()) < 5) {
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
    if (m_showPlotSettingsOnRightRelease && (event->buttons() & Qt::RightButton))
    {
        if ((event->pos() - m_rightClickPressPos).manhattanLength() > QApplication::startDragDistance())
            m_showPlotSettingsOnRightRelease = false;
    }

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
        else if (mDragMode == DragMode::Curve)
        {
            QCPCurve *curve = m_tracerCurves.value(mDraggedTracer, nullptr);
            if (QCPCurve *nearCurve = smithCurveAt(event->pos()))
            {
                if (nearCurve != curve)
                {
                    curve = nearCurve;
                    m_tracerCurves[mDraggedTracer] = curve;
                }
            }
            if (curve)
            {
                double x = m_plot->xAxis->pixelToCoord(event->pos().x());
                double y = m_plot->yAxis->pixelToCoord(event->pos().y());
                auto data = curve->data();
                if (!data->isEmpty())
                {
                    double minDist = std::numeric_limits<double>::max();
                    int closestIndex = m_tracerIndices.value(mDraggedTracer, 0);
                    int i = 0;
                    for (auto it = data->constBegin(); it != data->constEnd(); ++it, ++i)
                    {
                        double dx = it->key - x;
                        double dy = it->value - y;
                        double dist = dx*dx + dy*dy;
                        if (dist < minDist)
                        {
                            minDist = dist;
                            closestIndex = i;
                        }
                    }
                    auto it = data->constBegin();
                    std::advance(it, closestIndex);
                    mDraggedTracer->position->setCoords(it->key, it->value);
                    m_tracerIndices[mDraggedTracer] = closestIndex;
                }
            }
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
    else if (event->button() == Qt::RightButton)
    {
        if (m_showPlotSettingsOnRightRelease)
            showPlotSettingsDialog();
        m_showPlotSettingsOnRightRelease = false;
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
        double mag = std::sqrt(x*x + y*y);
        double angle = std::atan2(y, x) * 180.0 / M_PI;
        double freq = 0;
        QCPCurve *curve = m_tracerCurves.value(tracer, nullptr);
        int idx = m_tracerIndices.value(tracer, -1);
        if (curve && m_curveFreqs.contains(curve) && idx >= 0 && idx < m_curveFreqs[curve].size())
            freq = m_curveFreqs[curve].at(idx);
        labelText = QString("f: %1Hz\n|\u0393|: %2\n\u2220: %3\u00B0")
                        .arg(Network::formatEngineering(freq))
                        .arg(Network::formatEngineering(mag))
                        .arg(Network::formatEngineering(angle));
        if (tracer == mTracerB && mTracerA->visible()) {
            mTracerA->updatePosition();
            double xA = mTracerA->position->coords().x();
            double yA = mTracerA->position->coords().y();
            double magA = std::sqrt(xA*xA + yA*yA);
            double angleA = std::atan2(yA, xA) * 180.0 / M_PI;
            double freqA = 0;
            QCPCurve *curveA = m_tracerCurves.value(mTracerA, nullptr);
            int idxA = m_tracerIndices.value(mTracerA, -1);
            if (curveA && m_curveFreqs.contains(curveA) && idxA >= 0 && idxA < m_curveFreqs[curveA].size())
                freqA = m_curveFreqs[curveA].at(idxA);
            labelText += QString("\n\u0394f: %1Hz \u0394|\u0393|: %2 \u0394\u2220: %3\u00B0")
                             .arg(Network::formatEngineering(freq - freqA))
                             .arg(Network::formatEngineering(mag - magA))
                             .arg(Network::formatEngineering(angle - angleA));
        }
    } else {
        if (!tracer->graph())
            return;
        labelText = Network::formatEngineering(x) + "Hz " + Network::formatEngineering(y);
        if (tracer == mTracerB && mTracerA->visible())
        {
            mTracerA->updatePosition();
            double xA = mTracerA->position->coords().x();
            double yA = mTracerA->position->coords().y();
            double dx = x - xA;
            double dy = y - yA;
            labelText += QString("\nΔx: %1Hz Δy: %2")
                             .arg(Network::formatEngineering(dx))
                             .arg(Network::formatEngineering(dy));
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
    if (!m_plot)
        return;

    QVector<QCPGraph*> selectedGraphs;
    const auto selected = m_plot->selectedPlottables();
    for (QCPAbstractPlottable *pl : selected)
    {
        if (!pl)
            continue;
        if (pl->property("math_plot").toBool())
            continue;
        if (QCPGraph *graph = qobject_cast<QCPGraph*>(pl))
            selectedGraphs.append(graph);
    }

    if (selectedGraphs.size() != 2)
        return;

    QCPGraph *graph1 = selectedGraphs.at(0);
    QCPGraph *graph2 = selectedGraphs.at(1);

    QVector<double> x;
    QVector<double> y;
    if (!computeMathPlotData(graph1, graph2, x, y) || x.isEmpty())
        return;

    if (QCPAbstractPlottable *pl = plot(x, y, QPen(Qt::red),
                                        QStringLiteral("%1 - %2").arg(graph1->name()).arg(graph2->name()),
                                        nullptr, PlotType::Magnitude))
    {
        if (QCPGraph *mathGraph = qobject_cast<QCPGraph*>(pl))
        {
            mathGraph->setProperty("math_plot", true);
            QStringList sources{graph1->name(), graph2->name()};
            mathGraph->setProperty("math_plot_sources", sources);
            QVariantList sourceMeta;
            auto appendMeta = [&](QCPGraph *sourceGraph)
            {
                if (!sourceGraph)
                    return;
                QVariantMap meta;
                meta.insert(QStringLiteral("network_ptr"), sourceGraph->property("network_ptr"));
                meta.insert(QStringLiteral("sparam_key"), sourceGraph->property("sparam_key"));
                sourceMeta.append(meta);
            };
            appendMeta(graph1);
            appendMeta(graph2);
            if (sourceMeta.size() == 2)
                mathGraph->setProperty("math_plot_source_meta", sourceMeta);
        }
    }

    m_plot->replot();
}

bool PlotManager::removeSelectedMathPlots()
{
    if (!m_plot)
        return false;

    const auto selected = m_plot->selectedPlottables();
    bool removed = false;

    for (QCPAbstractPlottable *pl : selected)
    {
        if (!pl)
            continue;
        if (pl->property("math_plot").toBool())
        {
            m_plot->removePlottable(pl);
            removed = true;
        }
    }

    if (removed)
        m_plot->replot();

    return removed;
}

void PlotManager::selectionChanged()
{
    if (!m_plot)
        return;

    constexpr double selectionWidthIncrease = 2.0;
    const QColor defaultSelectionColor(80, 80, 255);

    for (int i = 0; i < m_plot->plottableCount(); ++i) {
        QCPAbstractPlottable *pl = m_plot->plottable(i);
        if (!pl)
            continue;

        QPen pen = pl->pen();
        QPen basePen = pen;
        double baseWidth = basePen.widthF();

        Network *network = nullptr;
        const QVariant networkVariant = pl->property("network_ptr");
        if (networkVariant.isValid())
            network = reinterpret_cast<Network*>(networkVariant.value<quintptr>());

        QString parameterKey = pl->property("sparam_key").toString();

        if (network && !parameterKey.isEmpty()) {
            basePen = network->parameterPen(parameterKey);
            baseWidth = basePen.widthF();
        }

        const double selectedWidth = baseWidth + selectionWidthIncrease;

        if (QCPSelectionDecorator *decorator = pl->selectionDecorator()) {
            QPen decoratorPen = decorator->pen();
            decoratorPen.setWidthF(selectedWidth);
            decorator->setPen(decoratorPen);
        }

        if (pl->selected()) {
            QPen selectionPen = basePen;
            QColor selectionColor = defaultSelectionColor;
            if (QCPSelectionDecorator *decorator = pl->selectionDecorator())
                selectionColor = decorator->pen().color();
            selectionPen.setColor(selectionColor);
            selectionPen.setWidthF(selectedWidth);
            pen = selectionPen;
        } else {
            basePen.setWidthF(baseWidth);
            pen = basePen;
        }

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

