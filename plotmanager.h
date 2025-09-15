#ifndef PLOTMANAGER_H
#define PLOTMANAGER_H

#include <QObject>
#include <QVector>
#include <QList>
#include <QColor>
#include <QMouseEvent>
#include <QMap>

#include "network.h"

class QCustomPlot;
class Network;
class NetworkCascade;
class QCPItemTracer;
class QCPItemText;
class QCPAbstractItem;
class QCPGraph;
class QCPCurve;
class QCPAbstractPlottable;

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

public slots:
    void mouseDoubleClick(QMouseEvent *event);
    void mousePress(QMouseEvent *event);
    void mouseMove(QMouseEvent *event);
    void mouseRelease(QMouseEvent *event);
    void setCursorAVisible(bool visible);
    void setCursorBVisible(bool visible);
    void createMathPlot();
    void selectionChanged();
    void keepAspectRatio();

private:
    enum class DragMode { None, Vertical, Horizontal, Curve };
    QCPAbstractPlottable* plot(const QVector<double> &x, const QVector<double> &y, const QColor &color,
              const QString &name, Network* network, PlotType type,
              Qt::PenStyle style = Qt::SolidLine);
    void updateTracerText(QCPItemTracer *tracer, QCPItemText *text);
    void updateTracers();
    void checkForTracerDrag(QMouseEvent *event, QCPItemTracer *tracer);
    void setupSmithGrid();
    void clearSmithGrid();
    void clearSmithMarkers();
    void addSmithMarkers(const QVector<double>& x, const QVector<double>& y, const QColor& color);


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
};

#endif // PLOTMANAGER_H
