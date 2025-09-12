#ifndef PLOTMANAGER_H
#define PLOTMANAGER_H

#include <QObject>
#include <QVector>
#include <QColor>
#include <QMouseEvent>

class QCustomPlot;
class Network;
class NetworkCascade;
class QCPItemTracer;
class QCPItemText;

class PlotManager : public QObject
{
    Q_OBJECT
public:
    explicit PlotManager(QCustomPlot* plot, QObject *parent = nullptr);

    void setNetworks(const QList<Network*>& networks);
    void setCascade(NetworkCascade* cascade);
    void updatePlots(const QStringList& sparams, bool isPhase);
    void autoscale();

public slots:
    void mouseDoubleClick(QMouseEvent *event);
    void mousePress(QMouseEvent *event);
    void mouseMove(QMouseEvent *event);
    void mouseRelease(QMouseEvent *event);
    void setCursorAVisible(bool visible);
    void setCursorBVisible(bool visible);
    void createMathPlot();

private:
    enum class DragMode { None, Vertical, Horizontal };
    void plot(const QVector<double> &x, const QVector<double> &y, const QColor &color,
              const QString &name, Network* network,
              Qt::PenStyle style = Qt::SolidLine);
    void updateTracerText(QCPItemTracer *tracer, QCPItemText *text);
    void updateTracers();
    void checkForTracerDrag(QMouseEvent *event, QCPItemTracer *tracer);


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
};

#endif // PLOTMANAGER_H
