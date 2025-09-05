#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "server.h"
#include "networks.h"
#include <iostream>
#include <math.h>
#include <QVector>
#include <QFileInfo>
#include "qcustomplot.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_server(new Server(this))
    , m_networks(new Networks(this))
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

    connect(m_server, &Server::filesReceived, this, &MainWindow::onFilesReceived);
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

void MainWindow::plot(const QVector<double> &x, const QVector<double> &y, const QColor &color, const QString &name, const QString &filePath, Qt::PenStyle style)
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
    customPlot->graph(graphCount)->setProperty("filePath", filePath);

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
    for (const QString &filePath : files) {
        m_networks->addFile(filePath);
        int s21_idx = m_networks->getSparamIndex("s21");
        QPair<QVector<double>, QVector<double>> plotData = m_networks->getPlotData(filePath, s21_idx, false);
        QColor color = m_networks->getFileColor(filePath);
        QString filename = m_networks->getFileName(filePath);
        plot(plotData.first, plotData.second, color, filename + " s21", filePath);
    }
}

void MainWindow::on_pushButtonAutoscale_clicked()
{
    ui->widgetGraph->rescaleAxes();
    ui->widgetGraph->replot();
}

void MainWindow::onFilesReceived(const QStringList &files)
{
    processFiles(files);
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


void MainWindow::updateSparamPlot(const QString &paramName, const Qt::CheckState &checkState)
{
    for (int i = ui->widgetGraph->graphCount() - 1; i >= 0; --i) {
        if (ui->widgetGraph->graph(i)->name().endsWith(" " + paramName)) {
            ui->widgetGraph->removeGraph(i);
        }
    }

    if (checkState == Qt::Checked) {
        const QStringList filePaths = m_networks->getFilePaths();
        for (const QString &filePath : filePaths) {
            int s_param_idx = m_networks->getSparamIndex(paramName);
            if(s_param_idx == -1) continue;

            bool isPhase = ui->checkBoxPhase->isChecked();
            QPair<QVector<double>, QVector<double>> plotData = m_networks->getPlotData(filePath, s_param_idx, isPhase);

            if (isPhase) {
                ui->widgetGraph->yAxis->setLabel("Phase (deg)");
            } else {
                ui->widgetGraph->yAxis->setLabel("Amplitude (dB)");
            }

            QString filename = m_networks->getFileName(filePath);
            QColor color = m_networks->getFileColor(filePath);
            Qt::PenStyle style = Qt::SolidLine;
            if (paramName == "s11") {
                style = Qt::DashLine;
            } else if (paramName == "s22") {
                style = Qt::DotLine;
            } else if (paramName == "s12") {
                style = Qt::DashDotLine;
            }

            plot(plotData.first, plotData.second, color, filename + " " + paramName, filePath, style);
        }
    }
    ui->widgetGraph->replot();
}

void MainWindow::on_checkBoxS11_checkStateChanged(const Qt::CheckState &arg1)
{
    updateSparamPlot("s11", arg1);
}


void MainWindow::on_checkBoxS21_checkStateChanged(const Qt::CheckState &arg1)
{
    updateSparamPlot("s21", arg1);
}


void MainWindow::on_checkBoxS12_checkStateChanged(const Qt::CheckState &arg1)
{
    updateSparamPlot("s12", arg1);
}


void MainWindow::on_checkBoxS22_checkStateChanged(const Qt::CheckState &arg1)
{
    updateSparamPlot("s22", arg1);
}

void MainWindow::on_checkBoxPhase_checkStateChanged(const Qt::CheckState &arg1)
{
    Q_UNUSED(arg1);
    // When the phase checkbox is toggled, we need to replot all active s-params
    updateSparamPlot("s11", ui->checkBoxS11->checkState());
    updateSparamPlot("s21", ui->checkBoxS21->checkState());
    updateSparamPlot("s12", ui->checkBoxS12->checkState());
    updateSparamPlot("s22", ui->checkBoxS22->checkState());
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Delete) {
        QList<QCPAbstractPlottable*> selection = ui->widgetGraph->selectedPlottables();
        if (!selection.isEmpty()) {
            // Get the file path from the first selected plottable
            QString filePath = selection.first()->property("filePath").toString();

            if (!filePath.isEmpty()) {
                // Remove data from network handler
                m_networks->removeFile(filePath);

                // Remove all graphs associated with this file path
                for (int i = ui->widgetGraph->graphCount() - 1; i >= 0; --i) {
                    if (ui->widgetGraph->graph(i)->property("filePath").toString() == filePath) {
                        ui->widgetGraph->removeGraph(i);
                    }
                }
                ui->widgetGraph->replot();
            }
        }
    } else {
        QMainWindow::keyPressEvent(event);
    }
}

