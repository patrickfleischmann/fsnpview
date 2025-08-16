#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <Eigen/Dense>
#include <math.h>
#include <QVector>

using namespace Eigen;

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

void MainWindow::on_pushButtonPlot_clicked()
{
    VectorXd xValues, yValues;
    xValues.setLinSpaced(1000,-2,2);
    int size_xValues=xValues.size();
    yValues.setZero(size_xValues,1);

    for(int i=0;i<size_xValues;i++)
    {
        yValues(i)=pow(xValues(i),2);
    }



    // convert the Eigen objects into the std::vector form
    // .data() returns the pointer to the first memory location of the first entry of the stored object
    // https://eigen.tuxfamily.org/dox/group__TopicStorageOrders.html
    std::vector<double> xValuesStdVector(xValues.data(), xValues.data() + xValues.rows() * xValues.cols());
    std::vector<double> yValuesStdVector(yValues.data(), yValues.data() + yValues.rows() * yValues.cols());

    //convert the std::vector objects to the Qt QVector form
    //QVector<double> xValuesQVector = QVector<double>::fromStdVector(xValuesStdVector);
    //QVector<double> yValuesQVector = QVector<double>::fromStdVector(yValuesStdVector);

    QVector<double> xValuesQVector(xValuesStdVector.begin(), xValuesStdVector.end());
    QVector<double> yValuesQVector(yValuesStdVector.begin(), yValuesStdVector.end());



    // this is necessary for seting the axes limits
    double x_maxValue=xValues.maxCoeff();
    double x_minValue=xValues.minCoeff();

    // this is necessary for seting the axes limits
    double y_maxValue=xValues.maxCoeff();
    double y_minValue=yValues.minCoeff();


    QCustomPlot *customPlot=ui->widgetGraph;

    // create graph and assign data to it:
    customPlot->addGraph();
    customPlot->graph(0)->setData(xValuesQVector, yValuesQVector);
    // give the axes some labels:
    customPlot->xAxis->setLabel("x");
    customPlot->yAxis->setLabel("y");
    // set axes ranges, so we see all data:
    customPlot->xAxis->setRange(x_minValue-0.1*abs(x_minValue), x_maxValue+0.1*abs(x_maxValue));
    customPlot->yAxis->setRange(y_minValue-0.1*abs(y_minValue), y_maxValue+0.1*abs(y_maxValue));
    customPlot->replot();



}

