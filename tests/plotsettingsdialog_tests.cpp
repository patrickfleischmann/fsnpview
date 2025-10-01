#include <QApplication>
#include <QSharedPointer>
#include <QPen>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

#include "plotmanager.h"
#include "plotsettingsdialog.h"
#include "qcustomplot.h"

static void test_manual_grid_and_ticks()
{
    QCustomPlot plot;
    PlotManager manager(&plot);

    plot.xAxis->setRange(0.0, 100.0);
    plot.yAxis->setRange(-10.0, 10.0);

    PlotSettingsDialog dialog(&plot);
    QPen gridPen(Qt::blue);
    gridPen.setStyle(Qt::DashLine);
    gridPen.setWidthF(0.0);
    QPen subGridPen(Qt::green);
    subGridPen.setStyle(Qt::DotLine);
    subGridPen.setWidthF(0.0);
    dialog.setGridSettings(gridPen, true, subGridPen, true);
    dialog.setXAxisTickSpacing(5.0, false, 0.0, true);
    dialog.setYAxisTickSpacing(2.5, false, 0.0, true);

    manager.applySettingsFromDialog(dialog);

    QCPGrid *xGrid = plot.xAxis->grid();
    QCPGrid *yGrid = plot.yAxis->grid();
    assert(xGrid && yGrid);
    assert(xGrid->visible());
    assert(yGrid->visible());
    assert(xGrid->pen().style() == Qt::DashLine);
    assert(yGrid->pen().style() == Qt::DashLine);
    assert(xGrid->pen().color() == Qt::blue);
    assert(yGrid->pen().color() == Qt::blue);
    assert(xGrid->subGridVisible());
    assert(yGrid->subGridVisible());
    assert(xGrid->subGridPen().style() == Qt::DotLine);
    assert(yGrid->subGridPen().style() == Qt::DotLine);
    assert(xGrid->subGridPen().color() == Qt::green);
    assert(yGrid->subGridPen().color() == Qt::green);

    auto xTicker = plot.xAxis->ticker();
    auto yTicker = plot.yAxis->ticker();
    auto fixedXTicker = qSharedPointerDynamicCast<QCPAxisTickerFixed>(xTicker);
    auto fixedYTicker = qSharedPointerDynamicCast<QCPAxisTickerFixed>(yTicker);
    assert(fixedXTicker);
    assert(fixedYTicker);
    assert(std::abs(fixedXTicker->tickStep() - 5.0) < 1e-9);
    assert(std::abs(fixedYTicker->tickStep() - 2.5) < 1e-9);

    PlotSettingsDialog disableDialog(&plot);
    disableDialog.setGridSettings(gridPen, false, subGridPen, false);
    disableDialog.setXAxisTickSpacing(5.0, true, 1.0, true);
    disableDialog.setYAxisTickSpacing(2.5, true, 1.0, true);

    manager.applySettingsFromDialog(disableDialog);

    assert(!plot.xAxis->grid()->visible());
    assert(!plot.yAxis->grid()->visible());
    assert(!plot.xAxis->grid()->subGridVisible());
    assert(!plot.yAxis->grid()->subGridVisible());

    auto autoXTicker = qSharedPointerDynamicCast<QCPAxisTickerFixed>(plot.xAxis->ticker());
    auto autoYTicker = qSharedPointerDynamicCast<QCPAxisTickerFixed>(plot.yAxis->ticker());
    assert(!autoXTicker);
    assert(!autoYTicker);
}

static void test_invalid_manual_spacing_falls_back_to_auto()
{
    QCustomPlot plot;
    PlotManager manager(&plot);
    plot.xAxis->setRange(0.0, 1.0);
    plot.yAxis->setRange(0.0, 1.0);

    PlotSettingsDialog dialog(&plot);
    dialog.setGridSettings(QPen(Qt::gray), true, QPen(Qt::gray), false);
    dialog.setXAxisTickSpacing(-1.0, false, 0.0, true);
    dialog.setYAxisTickSpacing(std::numeric_limits<double>::quiet_NaN(), false, 0.0, true);

    manager.applySettingsFromDialog(dialog);

    auto xTicker = plot.xAxis->ticker();
    auto yTicker = plot.yAxis->ticker();
    auto fixedXTicker = qSharedPointerDynamicCast<QCPAxisTickerFixed>(xTicker);
    auto fixedYTicker = qSharedPointerDynamicCast<QCPAxisTickerFixed>(yTicker);
    assert(!fixedXTicker);
    assert(!fixedYTicker);
}

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
    QApplication app(argc, argv);

    test_manual_grid_and_ticks();
    test_invalid_manual_spacing_falls_back_to_auto();

    std::cout << "Plot settings dialog tests passed" << std::endl;
    return 0;
}
