#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <iostream>
#include <Eigen/Dense>
#include <unsupported/Eigen/MatrixFunctions>
#include <math.h>
#include <QVector>
#include <QFileInfo>
#include "parser_touchstone.h"
#include "qcustomplot.h"

using namespace Eigen;
using ts::TouchstoneData;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , localServer(nullptr)
{
    ui->setupUi(this);
    ui->checkBoxS21->setChecked(true);
#ifdef QCUSTOMPLOT_USE_OPENGL
    ui->widgetGraph->setOpenGl(true);
#endif

    if(ui->widgetGraph->openGl()){
        std::cout << "openGl on" << std::endl;
    } else {
        std::cout << "openGl off" << std::endl;
    }

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
    connect(ui->widgetGraph, &QCustomPlot::mousePress, this, &MainWindow::mousePress);
    connect(ui->widgetGraph, &QCustomPlot::mouseMove, this, &MainWindow::mouseMove);
    connect(ui->widgetGraph, &QCustomPlot::mouseRelease, this, &MainWindow::mouseRelease);

    mTracerA = new QCPItemTracer(ui->widgetGraph);
    mTracerA->setPen(QPen(Qt::black,0));
    mTracerA->setBrush(Qt::NoBrush);
    mTracerA->setStyle(QCPItemTracer::tsCrosshair);
    mTracerA->setVisible(false);
    mTracerA->setInterpolating(true);

    mTracerTextA = new QCPItemText(ui->widgetGraph);
    mTracerTextA->setColor(Qt::red);
    mTracerTextA->setVisible(false);
    // mTracerTextA->setBrush(QColor(255, 255, 255, 190));


    mTracerB = new QCPItemTracer(ui->widgetGraph);
    mTracerB->setPen(QPen(Qt::darkGray,0));
    mTracerB->setBrush(Qt::NoBrush);
    mTracerB->setStyle(QCPItemTracer::tsCrosshair);
    mTracerB->setVisible(false);
    mTracerB->setInterpolating(true);

    mTracerTextB = new QCPItemText(ui->widgetGraph);
    mTracerTextB->setColor(Qt::blue);
    mTracerTextB->setVisible(false);
    //mTracerTextB->setBrush(QColor(255, 255, 255, 190));

    mDraggedTracer = nullptr;
    mDragMode = DragMode::None;
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::plot(const QVector<double> &x, const QVector<double> &y, const QColor &color, const QString &name, Qt::PenStyle style)
{
    QCustomPlot *customPlot = ui->widgetGraph;
    customPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);
    customPlot->setSelectionRectMode(QCP::srmZoom);
    customPlot->setRangeDragButton(Qt::RightButton);
    customPlot->setSelectionRectButton(Qt::LeftButton);

    int graphCount = customPlot->graphCount();
    customPlot->addGraph();
    customPlot->graph(graphCount)->setData(x, y);
    customPlot->graph(graphCount)->setAntialiased(false);

    QPen pen(color,0);
    pen.setStyle(style);
    customPlot->graph(graphCount)->setPen(pen);
    customPlot->graph(graphCount)->setName(name);
    customPlot->graph(graphCount)->addToLegend();

    customPlot->xAxis->setNumberFormat("g");
    customPlot->xAxis->setNumberPrecision(3);
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
            m_file_colors[path] = color;

            plot(xValuesQVector, yValuesQVector, color, filename + " s21");

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

void MainWindow::on_checkBoxCursorA_stateChanged(int arg1)
{
    if (ui->widgetGraph->graphCount() == 0)
    {
        ui->checkBoxCursorA->setCheckState(Qt::Unchecked);
        return;
    }

    mTracerA->setVisible(arg1 == Qt::Checked);
    mTracerTextA->setVisible(arg1 == Qt::Checked);

    if (arg1 == Qt::Checked)
    {
        QCPGraph *graph = ui->widgetGraph->graph(0);
        mTracerA->setGraph(graph);
        mTracerA->setGraphKey(ui->widgetGraph->xAxis->range().center());
    }
    updateTracers();
}


void MainWindow::on_checkBoxCursorB_stateChanged(int arg1)
{
    if (ui->widgetGraph->graphCount() == 0)
    {
        ui->checkBoxCursorB->setCheckState(Qt::Unchecked);
        return;
    }

    mTracerB->setVisible(arg1 == Qt::Checked);
    mTracerTextB->setVisible(arg1 == Qt::Checked);

    if (arg1 == Qt::Checked)
    {
        QCPGraph *graph = ui->widgetGraph->graph(0);
        mTracerB->setGraph(graph);
        mTracerB->setGraphKey(ui->widgetGraph->xAxis->range().center());
    }
    updateTracers();
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

void MainWindow::mousePress(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        mDraggedTracer = nullptr;
        mDragMode = DragMode::None;

        // Check for tracer A
        if (mTracerA->visible())
        {
            const QPointF tracerPos = mTracerA->position->pixelPosition();
            if (qAbs(event->pos().x() - tracerPos.x()) < 5) // Vertical line drag
            {
                mDraggedTracer = mTracerA;
                mDragMode = DragMode::Vertical;
            }
            else if (qAbs(event->pos().y() - tracerPos.y()) < 5) // Horizontal line drag
            {
                mDraggedTracer = mTracerA;
                mDragMode = DragMode::Horizontal;
            }
        }
        // Check for tracer B if A was not hit
        if (!mDraggedTracer && mTracerB->visible())
        {
            const QPointF tracerPos = mTracerB->position->pixelPosition();
            if (qAbs(event->pos().x() - tracerPos.x()) < 5) // Vertical line drag
            {
                mDraggedTracer = mTracerB;
                mDragMode = DragMode::Vertical;
            }
            else if (qAbs(event->pos().y() - tracerPos.y()) < 5) // Horizontal line drag
            {
                mDraggedTracer = mTracerB;
                mDragMode = DragMode::Horizontal;
            }
        }

        if (mDraggedTracer)
        {
            ui->widgetGraph->setSelectionRectMode(QCP::srmNone);
        }
    }
}

void MainWindow::mouseMove(QMouseEvent *event)
{
    if (mDraggedTracer)
    {
        if (mDragMode == DragMode::Vertical)
        {
            double key = ui->widgetGraph->xAxis->pixelToCoord(event->pos().x());
            mDraggedTracer->setGraphKey(key);
        }
        else if (mDragMode == DragMode::Horizontal)
        {
            QCPGraph *graph = qobject_cast<QCPGraph*>(ui->widgetGraph->plottableAt(event->pos(), true));
            if (graph && graph != mDraggedTracer->graph())
            {
                mDraggedTracer->setGraph(graph);
            }
            double key = ui->widgetGraph->xAxis->pixelToCoord(event->pos().x());
            mDraggedTracer->setGraphKey(key);
        }

        updateTracers();
    }
}

void MainWindow::mouseRelease(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        if (mDraggedTracer)
        {
            mDraggedTracer = nullptr;
            mDragMode = DragMode::None;
            ui->widgetGraph->setSelectionRectMode(QCP::srmZoom);
        }
    }
}

void MainWindow::updateTracerText(QCPItemTracer *tracer, QCPItemText *text)
{
    if (!tracer->visible() || !tracer->graph())
        return;

    tracer->updatePosition();
    double x = tracer->position->coords().x();
    double y = tracer->position->coords().y();

    QString labelText = QString::number(x, 'g', 4) + "Hz " + QString::number(y, 'f', 2);

    if (tracer == mTracerB && mTracerA->visible())
    {
        mTracerA->updatePosition();
        double xA = mTracerA->position->coords().x();
        double yA = mTracerA->position->coords().y();
        double dx = x - xA;
        double dy = y - yA;
        labelText += QString("\nΔx: %1Hz\nΔy: %2").arg(QString::number(dx, 'g', 4)).arg(QString::number(dy, 'f', 2));
    }

    text->setText(labelText);
    text->position->setCoords(x, y);
    text->position->setType(QCPItemPosition::ptPlotCoords);
    text->setPositionAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    text->setPadding(QMargins(5, 0, 0, 15));
}

void MainWindow::updateTracers()
{
    if (mTracerA && mTracerA->visible())
        updateTracerText(mTracerA, mTracerTextA);
    if (mTracerB && mTracerB->visible())
        updateTracerText(mTracerB, mTracerTextB);

    ui->widgetGraph->replot();
}

void MainWindow::on_checkBox_checkStateChanged(const Qt::CheckState &arg1)
{
    if(arg1 == Qt::Checked){
        std::cout << "set to freqLog" << std::endl;
        ui->widgetGraph->xAxis->setScaleType(QCPAxis::stLogarithmic);
        //ui->widgetGraph->xAxis->setTicker(QSharedPointer<QCPAxisTickerLog>(new QCPAxisTickerLog));
    } else {
        std::cout << "set to freqLin" << std::endl;
        ui->widgetGraph->xAxis->setScaleType(QCPAxis::stLinear);
        //ui->widgetGraph->xAxis->setTicker(QSharedPointer<QCPAxisTickerFixed>(new QCPAxisTickerFixed));
    }
    ui->widgetGraph->replot();
}


// Function to unwrap phase angles in an Eigen Array
Eigen::ArrayXd unwrap(const Eigen::ArrayXd& phase)
{
    Eigen::ArrayXd unwrapped_phase = phase;
    for (int i = 1; i < phase.size(); ++i) {
        double diff = unwrapped_phase(i) - unwrapped_phase(i - 1);
        if (diff > M_PI) {
            for (int j = i; j < phase.size(); ++j) {
                unwrapped_phase(j) -= 2 * M_PI;
            }
        } else if (diff < -M_PI) {
            for (int j = i; j < phase.size(); ++j) {
                unwrapped_phase(j) += 2 * M_PI;
            }
        }
    }
    return unwrapped_phase;
}

void MainWindow::updateSparamPlot(const QString &paramName, int s_param_idx, const Qt::CheckState &checkState)
{
    for (int i = ui->widgetGraph->graphCount() - 1; i >= 0; --i) {
        if (ui->widgetGraph->graph(i)->name().endsWith(" " + paramName)) {
            ui->widgetGraph->removeGraph(i);
        }
    }

    if (checkState == Qt::Checked) {
        for (auto const& [path, data] : parsed_data) {
            Eigen::ArrayXd xValues = data->freq;
            Eigen::ArrayXd yValues;

            if (ui->checkBoxPhase->isChecked()) {
                // Calculate phase in degrees
                Eigen::ArrayXcd s_param_col = data->sparams.col(s_param_idx);
                Eigen::ArrayXd phase_rad = s_param_col.arg();
                Eigen::ArrayXd unwrapped_phase_rad = unwrap(phase_rad);
                yValues = unwrapped_phase_rad * (180.0 / M_PI);
                ui->widgetGraph->yAxis->setLabel("Phase (deg)");
            } else {
                // Calculate magnitude in dB
                yValues = data->sparams.col(s_param_idx).abs().log10() * 20;
                ui->widgetGraph->yAxis->setLabel("Amplitude (dB)");
            }

            std::vector<double> xValuesStdVector(xValues.data(), xValues.data() + xValues.rows() * xValues.cols());
            std::vector<double> yValuesStdVector(yValues.data(), yValues.data() + yValues.rows() * yValues.cols());

            QVector<double> xValuesQVector = QVector<double>(xValuesStdVector.begin(), xValuesStdVector.end());
            QVector<double> yValuesQVector = QVector<double>(yValuesStdVector.begin(), yValuesStdVector.end());

            QFileInfo fileInfo(QString::fromStdString(path));
            QString filename = fileInfo.fileName();

            QColor color = m_file_colors.at(path);
            Qt::PenStyle style = Qt::SolidLine;
            if (paramName == "s11") {
                style = Qt::DashLine;
            } else if (paramName == "s22") {
                style = Qt::DotLine;
            } else if (paramName == "s12") {
                style = Qt::DashDotLine;
            }

            plot(xValuesQVector, yValuesQVector, color, filename + " " + paramName, style);
        }
    }
    ui->widgetGraph->replot();
}

void MainWindow::on_checkBoxS11_checkStateChanged(const Qt::CheckState &arg1)
{
    updateSparamPlot("s11", 0, arg1);
}


void MainWindow::on_checkBoxS21_checkStateChanged(const Qt::CheckState &arg1)
{
    updateSparamPlot("s21", 1, arg1);
}


void MainWindow::on_checkBoxS12_checkStateChanged(const Qt::CheckState &arg1)
{
    updateSparamPlot("s12", 2, arg1);
}


void MainWindow::on_checkBoxS22_checkStateChanged(const Qt::CheckState &arg1)
{
    updateSparamPlot("s22", 3, arg1);
}

void MainWindow::on_checkBoxPhase_checkStateChanged(const Qt::CheckState &arg1)
{
    // When the phase checkbox is toggled, we need to replot all active s-params
    updateSparamPlot("s11", 0, ui->checkBoxS11->checkState());
    updateSparamPlot("s21", 1, ui->checkBoxS21->checkState());
    updateSparamPlot("s12", 2, ui->checkBoxS12->checkState());
    updateSparamPlot("s22", 3, ui->checkBoxS22->checkState());
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Delete) {
        QList<QCPAbstractPlottable*> selection = ui->widgetGraph->selectedPlottables();
        if (!selection.isEmpty()) {
            for (QCPAbstractPlottable* plottable : selection) {
                ui->widgetGraph->removePlottable(plottable);
            }
            ui->widgetGraph->replot();
        }
    } else {
        QMainWindow::keyPressEvent(event);
    }
}

