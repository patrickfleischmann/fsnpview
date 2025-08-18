#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <iostream>
#include <Eigen/Dense>
#include <math.h>
#include <QVector>
#include "parser_touchstone.h"

using namespace Eigen;
using ts::TouchstoneData;


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::plot(const QVector<double> &x, const QVector<double> &y, const QColor &color)
{
    QCustomPlot *customPlot = ui->widgetGraph;
    customPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);

    int graphCount = customPlot->graphCount();
    customPlot->addGraph();
    customPlot->graph(graphCount)->setData(x, y);
    customPlot->graph(graphCount)->setAntialiased(true);

    QPen pen;
    pen.setColor(color);
    customPlot->graph(graphCount)->setPen(pen);

    customPlot->xAxis->setLabel("Frequency");
    customPlot->yAxis->setLabel("S21 (dB)");

    customPlot->rescaleAxes();
    customPlot->replot();
}

void MainWindow::on_pushButtonPlot_clicked()
{
    // This button is now disabled as plotting is handled via command line.
}

