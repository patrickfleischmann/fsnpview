#ifndef PLOTMANAGER_H
#define PLOTMANAGER_H

#include "qcustomplot.h"
#include <QObject>
#include <QVector>
#include <QColor>
#include <QMouseEvent>
#include <QWheelEvent>

class Network;
class NetworkCascade;
class QRubberBand;

class PlotManager : public QObject
{
    Q_OBJECT
public:
    explicit PlotManager(QCustomPlot* plot, QObject *parent = nullptr);
    ~PlotManager();

    void setNetworks(const QList<Network*>& networks);
    void setCascade(NetworkCascade* cascade);
    void updatePlots(const QStringList& sparams, bool isPhase);
    void autoscale();
    void createDifference(QCPGraph* graph1, QCPGraph* graph2);

signals:
    void cursorUpdated(const QString& cursor_a_text, const QString& cursor_b_text, const QString& delta_text);

private slots:
    void onMousePress(QMouseEvent* event);
    void onMouseMove(QMouseEvent* event);
    void onMouseRelease(QMouseEvent* event);
    void onMouseWheel(QWheelEvent* event);
    void onMouseDoubleClick(QMouseEvent *event);
    void onPlottableClick(QCPAbstractPlottable *plottable, int dataIndex, QMouseEvent *event);


private:
    void plot(const QVector<double> &x, const QVector<double> &y, const QColor &color, const QString &name, const QString &yAxisLabel, Qt::PenStyle style = Qt::SolidLine);
    void setupCursors();

    QCustomPlot* m_plot;
    QRubberBand* m_rubber_band;
    QPoint m_drag_start_pos;
    QCPItemTracer* m_cursor_a;
    QCPItemTracer* m_cursor_b;
    QList<Network*> m_networks;
    NetworkCascade* m_cascade;
    QVector<QColor> m_colors;
    int m_color_index;
};

#endif // PLOTMANAGER_H
