#ifndef PLOTMANAGER_H
#define PLOTMANAGER_H

#include <QObject>
#include <QVector>
#include <QList>
#include <QColor>
#include <QMouseEvent>
#include <QMap>
#include <QPoint>

#include "network.h"
#include "qcustomplot.h"

class QCustomPlot;
class Network;
class NetworkCascade;
class QCPItemTracer;
class QCPItemText;
class QCPAbstractItem;
class QCPGraph;
class QCPCurve;
class QCPAbstractPlottable;
class PlotSettingsDialog;

class PlotManager : public QObject
{
    Q_OBJECT
public:
    explicit PlotManager(QCustomPlot* plot, QObject *parent = nullptr);

    void setNetworks(const QList<Network*>& networks);
    void setCascade(NetworkCascade* cascade);
    void updatePlots(const QStringList& sparams, PlotType type);
    void autoscale();
    QColor nextColor();
    void setXAxisScaleType(QCPAxis::ScaleType type);
    void setCrosshairEnabled(bool enabled);

public slots:
    void mouseDoubleClick(QMouseEvent *event);
    void mousePress(QMouseEvent *event);
    void mouseMove(QMouseEvent *event);
    void mouseRelease(QMouseEvent *event);
    void setCursorAVisible(bool visible);
    void setCursorBVisible(bool visible);
    void createMathPlot();
    bool removeSelectedMathPlots();
    void selectionChanged();
    void keepAspectRatio();

private:
    enum class DragMode { None, Vertical, Horizontal, Curve };
    QCPAbstractPlottable* plot(const QVector<double> &x, const QVector<double> &y, const QPen &pen,
              const QString &name, Network* network, PlotType type, const QString &parameterKey = QString());
    void updateTracerText(QCPItemTracer *tracer, QCPItemText *text);
    void updateTracers();
    void checkForTracerDrag(QMouseEvent *event, QCPItemTracer *tracer);
    void configureCursorStyles(PlotType type);
    QCPGraph *firstGraph() const;
    QCPCurve *firstSmithCurve() const;
    QCPCurve *smithCurveAt(const QPoint &pos) const;
    QCPGraph *graphByName(const QString &name) const;
    bool computeMathPlotData(QCPGraph *graph1, QCPGraph *graph2,
                             QVector<double> &x, QVector<double> &y) const;
    void updateMathPlots();
    void setupSmithGrid();
    void clearSmithGrid();
    void clearSmithMarkers();
    void addSmithMarkers(const QVector<double>& x, const QVector<double>& y, const QColor& color);
    void updateAxisTickers();
    void showPlotSettingsDialog();
    void applyAxisRanges(const PlotSettingsDialog &dialog);
    void applyMarkerPositions(const PlotSettingsDialog &dialog);
    double markerValue(const QCPItemTracer *tracer) const;
    void setMarkerValue(QCPItemTracer *tracer, double value);
    void setCartesianMarkerValue(QCPItemTracer *tracer, double value);
    void setSmithMarkerFrequency(QCPItemTracer *tracer, double frequency);
    QString markerLabelText(const QString &markerName) const;


    QCustomPlot* m_plot;
    QList<Network*> m_networks;
    NetworkCascade* m_cascade;
    QVector<QColor> m_colors;
    int m_color_index;

    QCPItemTracer *mTracerA;
    QCPItemText *mTracerTextA;
    QCPItemTracer *mTracerB;
    QCPItemText *mTracerTextB;
    QCPItemTracer *mDraggedTracer;
    DragMode mDragMode;
    QList<QCPCurve*> m_smithGridCurves;
    QList<QCPAbstractItem*> m_smithGridItems;
    QList<QCPAbstractItem*> m_smithMarkers;
    QMap<QCPCurve*, QVector<double>> m_curveFreqs;
    QMap<QCPItemTracer*, QCPCurve*> m_tracerCurves;
    QMap<QCPItemTracer*, int> m_tracerIndices;
    bool m_keepAspectConnected;
    PlotType m_currentPlotType;
    bool m_crosshairEnabled;
    bool m_showPlotSettingsOnRightRelease;
    QPoint m_rightClickPressPos;
};

#endif // PLOTMANAGER_H
