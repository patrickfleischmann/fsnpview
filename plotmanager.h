#ifndef PLOTMANAGER_H
#define PLOTMANAGER_H

#include <QObject>
#include <QVector>
#include <QColor>

class QCustomPlot;
class Network;
class NetworkCascade;

class PlotManager : public QObject
{
    Q_OBJECT
public:
    explicit PlotManager(QCustomPlot* plot, QObject *parent = nullptr);

    void setNetworks(const QList<Network*>& networks);
    void setCascade(NetworkCascade* cascade);
    void updatePlots(const QStringList& sparams, bool isPhase);
    void autoscale();

private:
    void plot(const QVector<double> &x, const QVector<double> &y, const QColor &color, const QString &name, const QString &yAxisLabel, Qt::PenStyle style = Qt::SolidLine);

    QCustomPlot* m_plot;
    QList<Network*> m_networks;
    NetworkCascade* m_cascade;
    QVector<QColor> m_colors;
    int m_color_index;
};

#endif // PLOTMANAGER_H
