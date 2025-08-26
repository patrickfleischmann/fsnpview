#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <iostream>
#include <Eigen/Dense>
#include <math.h>
#include <QVector>
#include <QFileInfo>
#include "parser_touchstone.h"

using namespace Eigen;
using ts::TouchstoneData;


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , localServer(nullptr)
{
    ui->setupUi(this);

    ui->widgetGraph->legend->setVisible(false);
    ui->widgetGraph->legend->setSelectableParts(QCPLegend::spItems);

    const QString serverName = "fsnpview-server";
    localServer = new QLocalServer(this);
    if (!localServer->listen(serverName)) {
        if (localServer->serverError() == QAbstractSocket::AddressInUseError) {
            QLocalServer::removeServer(serverName);
            localServer->listen(serverName);
        }
    }
    connect(localServer, &QLocalServer::newConnection, this, &MainWindow::newConnection);

    connect(ui->widgetGraph, &QCustomPlot::mouseDoubleClick, this, &MainWindow::mouseDoubleClick);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::plot(const QVector<double> &x, const QVector<double> &y, const QColor &color, const QString &name)
{
    QCustomPlot *customPlot = ui->widgetGraph;
    customPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);
    customPlot->setSelectionRectMode(QCP::srmZoom);
    customPlot->setRangeDragButton(Qt::RightButton);
    customPlot->setSelectionRectButton(Qt::LeftButton);

    int graphCount = customPlot->graphCount();
    customPlot->addGraph();
    customPlot->graph(graphCount)->setData(x, y);
    customPlot->graph(graphCount)->setAntialiased(true);

   // QPen pen;
   // pen.setColor(color);
    customPlot->graph(graphCount)->setPen(QPen(color,2));
    customPlot->graph(graphCount)->setName(name);
    customPlot->graph(graphCount)->addToLegend();

    customPlot->xAxis->setLabel("Frequency");
    customPlot->yAxis->setLabel("S21 (dB)");

    customPlot->xAxis->grid()->setPen(QPen(Qt::lightGray, 0)); //0 -> defaults to cosmetic pen -> always drawn with exactly 1 pixel
    customPlot->yAxis->grid()->setPen(QPen(Qt::lightGray, 0));

    customPlot->xAxis->grid()->setSubGridVisible(true);
    customPlot->yAxis->grid()->setSubGridVisible(true);

    customPlot->rescaleAxes();
    customPlot->replot();
}

void MainWindow::processFiles(const QStringList &files)
{
    QList<QColor> colors;
    colors.append(QColor::fromRgbF(0, 0.4470, 0.7410));
    colors.append(QColor::fromRgbF(0.8500, 0.3250, 0.0980));
    colors.append(QColor::fromRgbF(0.9290, 0.6940, 0.1250));
    colors.append(QColor::fromRgbF(0.4940, 0.1840, 0.5560));
    colors.append(QColor::fromRgbF(0.4660, 0.6740, 0.1880));
    colors.append(QColor::fromRgbF(0.3010, 0.7450, 0.9330));
    colors.append(QColor::fromRgbF(0.6350, 0.0780, 0.1840));
    colors.append(QColor::fromRgbF(0, 0, 1.0000));
    colors.append(QColor::fromRgbF(0, 0.5000, 0));
    colors.append(QColor::fromRgbF(1.0000, 0, 0));
    colors.append(QColor::fromRgbF(0, 0.7500, 0.7500));
    colors.append(QColor::fromRgbF(0.7500, 0, 0.7500));
    colors.append(QColor::fromRgbF(0.7500, 0.7500, 0));
    colors.append(QColor::fromRgbF(0.2500, 0.2500, 0.2500));

    for (int i = 0; i < files.size(); ++i) {
        try {
            QString s_path = files.at(i);
            QFileInfo fileInfo(s_path);
            QString filename = fileInfo.fileName();

            std::string path = s_path.toStdString();
            auto data = std::make_unique<ts::TouchstoneData>(ts::parse_touchstone(path));

            Eigen::ArrayXd xValues = data->freq;
            Eigen::ArrayXd yValues = data->sparams.col(1).abs().log10() * 20; // s21 dB

            std::vector<double> xValuesStdVector(xValues.data(), xValues.data() + xValues.rows() * xValues.cols());
            std::vector<double> yValuesStdVector(yValues.data(), yValues.data() + yValues.rows() * yValues.cols());

            QVector<double> xValuesQVector = QVector<double>(xValuesStdVector.begin(), xValuesStdVector.end());
            QVector<double> yValuesQVector = QVector<double>(yValuesStdVector.begin(), yValuesStdVector.end());

            QColor color = colors.at(parsed_data.size() % colors.size());

            plot(xValuesQVector, yValuesQVector, color, filename);

            parsed_data[path] = std::move(data);
        } catch (const std::exception& e) {
            std::cerr << "Error processing file " << files.at(i).toStdString() << ": " << e.what() << std::endl;
        }
    }
}

void MainWindow::on_pushButtonAutoscale_clicked()
{
    ui->widgetGraph->rescaleAxes();
    ui->widgetGraph->replot();
}

void MainWindow::newConnection()
{
    QLocalSocket *socket = localServer->nextPendingConnection();
    if (socket) {
        connect(socket, &QLocalSocket::readyRead, this, &MainWindow::readyRead);
        connect(socket, &QLocalSocket::disconnected, socket, &QLocalSocket::deleteLater);
        std::cout << "New connection received." << std::endl;
    }
}

void MainWindow::readyRead()
{
    QLocalSocket *socket = qobject_cast<QLocalSocket*>(sender());
    if (socket) {
        QDataStream stream(socket);
        stream.startTransaction();
        QStringList files;
        stream >> files;
        if (stream.commitTransaction()) {
            std::cout << "Received files from new instance: " << files.join(", ").toStdString() << std::endl;
            processFiles(files);
            socket->disconnectFromServer();
        }
    }
}

void MainWindow::on_checkBoxCursorA_checkStateChanged(const Qt::CheckState &arg1)
{

}


void MainWindow::on_checkBoxCursorB_checkStateChanged(const Qt::CheckState &arg1)
{

}


void MainWindow::on_checkBoxLegend_checkStateChanged(const Qt::CheckState &arg1)
{
    ui->widgetGraph->legend->setVisible(arg1 == Qt::Checked);
    ui->widgetGraph->replot();
}

void MainWindow::mouseDoubleClick(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        ui->widgetGraph->rescaleAxes();
        ui->widgetGraph->replot();
    }
}
