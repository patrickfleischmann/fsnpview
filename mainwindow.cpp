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

void MainWindow::plotFile(const QString &filePath)
{
    auto data = ts::parse_touchstone(filePath.toStdString());

    ArrayXd xValues = data.freq;
    ArrayXd yValues = data.sparams.col(1).abs().log10() * 20; //s21 dB

    std::vector<double> xValuesStdVector(xValues.data(), xValues.data() + xValues.rows() * xValues.cols());
    std::vector<double> yValuesStdVector(yValues.data(), yValues.data() + yValues.rows() * yValues.cols());

    QVector<double> xValuesQVector(xValuesStdVector.begin(), xValuesStdVector.end());
    QVector<double> yValuesQVector(yValuesStdVector.begin(), yValuesStdVector.end());

    QCustomPlot *customPlot = ui->widgetGraph;
    customPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);

    int graphCount = customPlot->graphCount();
    customPlot->addGraph();
    customPlot->graph(graphCount)->setData(xValuesQVector, yValuesQVector);
    customPlot->graph(graphCount)->setAntialiased(true);

    // Assign a different color to the new graph
    QPen pen;
    pen.setColor(QColor::fromHsv((graphCount * 60) % 360, 255, 255));
    customPlot->graph(graphCount)->setPen(pen);

    customPlot->xAxis->setLabel("Frequency");
    customPlot->yAxis->setLabel("S21 (dB)");

    customPlot->rescaleAxes();
    customPlot->replot();
}

void MainWindow::on_pushButtonPlot_clicked()
{
    std::cout << "on_pushButtonPlot_clicked()" << std::endl;
    plotFile("test.s2p");
}

