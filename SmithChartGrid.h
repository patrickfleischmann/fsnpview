// SmithChartGrid.h
#pragma once
#include <QVector>
#include <QString>
#include <QtMath>
#include "qcustomplot.h"

namespace SmithChart
{

// ---- Version-compat helper -----------------------------------------------
// QCustomPlot < 2.0: items auto-register via constructor, no addItem() API.
// QCustomPlot ≥ 2.0: must call plot->addItem(item).
#ifndef QCUSTOMPLOT_VERSION
#define QCUSTOMPLOT_VERSION 0
#endif

inline void addItemCompat(QCustomPlot* plot, QCPAbstractItem* item)
{
#if QCUSTOMPLOT_VERSION >= 0x020000
    plot->addItem(item);
#else
    Q_UNUSED(plot);
    Q_UNUSED(item);
#endif
}

// Preset grid densities
enum class Preset { Coarse, Medium, Dense };

// Core geometry generator (impedance solid, admittance dashed, all clipped)
inline void generateSmithChartGridExtended(
    QVector<QVector<double>>& impX, QVector<QVector<double>>& impY,
    QVector<QVector<double>>& admX, QVector<QVector<double>>& admY,
    QVector<double>& unitX, QVector<double>& unitY,
    QVector<double>& realX, QVector<double>& realY,
    QVector<double>& labelX, QVector<double>& labelY, QVector<QString>& labelText,
    const QVector<double>& rVals,
    const QVector<double>& xVals,
    int pointsPerCircle = 720)
{
    impX.clear(); impY.clear();
    admX.clear(); admY.clear();
    unitX.clear(); unitY.clear();
    realX.clear(); realY.clear();
    labelX.clear(); labelY.clear(); labelText.clear();

    auto appendClippedCircle = [&](double cx, double cy, double R,
                                   double th0, double th1,
                                   QVector<QVector<double>>& outX,
                                   QVector<QVector<double>>& outY)
    {
        QVector<double> x, y;
        x.reserve(pointsPerCircle + 1);
        y.reserve(pointsPerCircle + 1);

        for (int i = 0; i <= pointsPerCircle; ++i) {
            double t = th0 + (th1 - th0) * double(i) / double(pointsPerCircle);
            double px = cx + R * qCos(t);
            double py = cy + R * qSin(t);
            if (px*px + py*py <= 1.0 + 1e-6) {
                x.push_back(px);
                y.push_back(py);
            } else {
                if (!x.isEmpty()) { outX.push_back(x); outY.push_back(y); x.clear(); y.clear(); }
            }
        }
        if (!x.isEmpty()) { outX.push_back(x); outY.push_back(y); }
    };

    // Impedance: r-circles (solid) + ±jx arcs (solid)
    for (double r : rVals) {
        double cx = r / (r + 1.0), cy = 0.0, R = 1.0 / (r + 1.0);
        appendClippedCircle(cx, cy, R, 0.0, 2*M_PI, impX, impY);

        // label at left intersection with real axis (inside unit disk)
        double xLabel = cx - R;
        labelX.push_back(xLabel);
        labelY.push_back(0.0);
        labelText.push_back(QString::number(r));
    }
    for (double xval : xVals) {
        double R = 1.0 / xval;
        appendClippedCircle(1.0,  1.0/xval, R, 0.0, 2*M_PI, impX, impY); // +j
        appendClippedCircle(1.0, -1.0/xval, R, 0.0, 2*M_PI, impX, impY); // -j
    }

    // Admittance: mirrored about origin (dashed)
    for (double r : rVals) {
        double cx = -r / (r + 1.0), cy = 0.0, R = 1.0 / (r + 1.0);
        appendClippedCircle(cx, cy, R, 0.0, 2*M_PI, admX, admY);
    }
    for (double xval : xVals) {
        double R = 1.0 / xval;
        appendClippedCircle(-1.0,  1.0/xval, R, 0.0, 2*M_PI, admX, admY); // +jb
        appendClippedCircle(-1.0, -1.0/xval, R, 0.0, 2*M_PI, admX, admY); // -jb
    }

    // Unit circle
    unitX.reserve(pointsPerCircle + 1);
    unitY.reserve(pointsPerCircle + 1);
    for (int i = 0; i <= pointsPerCircle; ++i) {
        double t = 2*M_PI * double(i) / double(pointsPerCircle);
        unitX.push_back(qCos(t));
        unitY.push_back(qSin(t));
    }

    // Real axis (same styling as impedance)
    realX = {-1.0, 1.0};
    realY = { 0.0, 0.0};
}

// High-level: draw onto a QCustomPlot
inline void addSmithChartGrid(QCustomPlot* plot, Preset preset = Preset::Medium)
{
    QVector<double> rVals, xVals;
    switch (preset) {
    case Preset::Coarse: rVals = {0.5, 1.0, 2.0}; xVals = {0.5, 1.0, 2.0}; break;
    case Preset::Medium: rVals = {0.2, 0.5, 1.0, 2.0, 5.0}; xVals = {0.2, 0.5, 1.0, 2.0, 5.0}; break;
    case Preset::Dense:  rVals = {0.1,0.2,0.5,1.0,2.0,5.0,10.0}; xVals = {0.1,0.2,0.5,1.0,2.0,5.0,10.0}; break;
    }

    QVector<QVector<double>> impX, impY, admX, admY;
    QVector<double> unitX, unitY, realX, realY;
    QVector<double> labelX, labelY; QVector<QString> labelText;

    generateSmithChartGridExtended(
        impX, impY, admX, admY, unitX, unitY, realX, realY,
        labelX, labelY, labelText, rVals, xVals, 720);

    // Pens
    QPen impPen(QColor(120,120,120)); impPen.setWidthF(1.0);
    QPen admPen = impPen; admPen.setStyle(Qt::DashLine);

    // Impedance (solid)
    for (int i = 0; i < impX.size(); ++i) {
        plot->addGraph();
        plot->graph()->setPen(impPen);
        plot->graph()->setData(impX[i], impY[i]);
    }
    // Admittance (dashed)
    for (int i = 0; i < admX.size(); ++i) {
        plot->addGraph();
        plot->graph()->setPen(admPen);
        plot->graph()->setData(admX[i], admY[i]);
    }
    // Unit circle
    plot->addGraph();
    plot->graph()->setPen(impPen);
    plot->graph()->setData(unitX, unitY);
    // Real axis
    plot->addGraph();
    plot->graph()->setPen(impPen);
    plot->graph()->setData(realX, realY);

    // Labels (below real axis), version-safe add
    for (int i = 0; i < labelX.size(); ++i) {
        auto *txt = new QCPItemText(plot);         // auto-register on <2.0
        addItemCompat(plot, txt);                  // explicit add on ≥2.0
        txt->position->setType(QCPItemPosition::ptPlotCoords);
        txt->position->setCoords(labelX[i], -0.03);
        txt->setText(labelText[i]);
        txt->setFont(QFont(plot->font().family(), 8));
        txt->setColor(QColor(120,120,120));
        txt->setPositionAlignment(Qt::AlignHCenter|Qt::AlignTop);
        txt->setPadding(QMargins(1,1,1,1));
        txt->setPen(Qt::NoPen);
        txt->setBrush(Qt::NoBrush);
    }

    // View / axes
    plot->xAxis->setRange(-1.05, 1.05);
    plot->yAxis->setRange(-1.05, 1.05);
    plot->xAxis->setTicks(false);
    plot->yAxis->setTicks(false);
    plot->xAxis->setTickLabels(false);
    plot->yAxis->setTickLabels(false);
    plot->setBackground(Qt::white);
    plot->replot();
}

} // namespace SmithChart
