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
    ui->widgetGraph->legend->setSelectableParts(QCPLegend::spItems | QCPLegend::spLegendBox);
    ui->widgetGraph->setMultiSelectModifier(Qt::ControlModifier);

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
    mLegendDrag = false;
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::plot(const QVector<double> &x, const QVector<double> &y, const QColor &color, const QString &name, const QString &filePath, const QString &yAxisLabel, Qt::PenStyle style)
{
    QCustomPlot *customPlot = ui->widgetGraph;
    customPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables | QCP::iMultiSelect);
    customPlot->setSelectionRectMode(QCP::srmZoom);
    customPlot->setRangeDragButton(Qt::RightButton);
    customPlot->setSelectionRectButton(Qt::LeftButton);

    int graphCount = customPlot->graphCount();
    customPlot->addGraph();
    customPlot->graph(graphCount)->setData(x, y);
    customPlot->graph(graphCount)->setAntialiased(true);
    customPlot->graph(graphCount)->setProperty("filePath", filePath);

    QPen pen(color,1.5);
    pen.setStyle(style);
    customPlot->graph(graphCount)->setPen(pen);
    customPlot->graph(graphCount)->setName(name);
    customPlot->graph(graphCount)->addToLegend();

    customPlot->xAxis->setNumberFormat("g");
    customPlot->xAxis->setNumberPrecision(3);
    customPlot->xAxis->setLabel("Frequency");
    customPlot->yAxis->setLabel(yAxisLabel);

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
    }

    // Replot all active s-parameters for all files
    updateSparamPlot("s11", ui->checkBoxS11->checkState());
    updateSparamPlot("s21", ui->checkBoxS21->checkState());
    updateSparamPlot("s12", ui->checkBoxS12->checkState());
    updateSparamPlot("s22", ui->checkBoxS22->checkState());
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
        if (ui->widgetGraph->legend->selectTest(event->pos(), false) >= 0)
        {
            mLegendDrag = true;
            mLegendDragStart = event->pos();
        }
        else
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
}

void MainWindow::mouseMove(QMouseEvent *event)
{
    if (mLegendDrag)
    {
        QPointF delta = event->pos() - mLegendDragStart;
        mLegendDragStart = event->pos();
        ui->widgetGraph->legend->setOuterRect(ui->widgetGraph->legend->outerRect().translated(delta.toPoint()));
        ui->widgetGraph->replot();
    }
    else if (mDraggedTracer)
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
        mLegendDrag = false;
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
        labelText += QString("\n\nΔx: %1Hz Δy: %2").arg(QString::number(dx, 'g', 4)).arg(QString::number(dy, 'f', 2));
    }

    text->setText(labelText);
    text->position->setCoords(x, y);
    text->position->setType(QCPItemPosition::ptPlotCoords);
    if (tracer == mTracerA) {
        text->setPositionAlignment(Qt::AlignRight | Qt::AlignBottom);
        text->setPadding(QMargins(0, 0, 5, 9));
    } else {
        text->setTextAlignment(Qt::AlignLeft);
        text->setPositionAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        text->setPadding(QMargins(5, 0, 0, 0));
    }


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
        QSharedPointer<QCPAxisTickerLog> logTicker(new QCPAxisTickerLog);
        logTicker->setLogBase(10);
        logTicker->setSubTickCount(9);
        ui->widgetGraph->xAxis->setTicker(logTicker);
    } else {
        std::cout << "set to freqLin" << std::endl;
        ui->widgetGraph->xAxis->setScaleType(QCPAxis::stLinear);
        ui->widgetGraph->xAxis->setTicker(QSharedPointer<QCPAxisTicker>(new QCPAxisTicker));
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

            QString yAxisLabel = isPhase ? "Phase (deg)" : "Amplitude (dB)";
            ui->widgetGraph->yAxis->setLabel(yAxisLabel);

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

            plot(plotData.first, plotData.second, color, filename + " " + paramName, filePath, yAxisLabel, style);
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
            QSet<QString> filePathsToRemove;
            for (QCPAbstractPlottable *plottable : selection) {
                QString filePath = plottable->property("filePath").toString();
                if (!filePath.isEmpty()) {
                    filePathsToRemove.insert(filePath);
                }
            }

            for (const QString &filePath : filePathsToRemove) {
                if (filePath.startsWith("math:")) {
                    // This is a math plot, just remove the graph
                    for (int i = ui->widgetGraph->graphCount() - 1; i >= 0; --i) {
                        if (ui->widgetGraph->graph(i)->property("filePath").toString() == filePath) {
                            ui->widgetGraph->removeGraph(i);
                        }
                    }
                } else {
                    m_networks->removeFile(filePath);
                    for (int i = ui->widgetGraph->graphCount() - 1; i >= 0; --i) {
                        if (ui->widgetGraph->graph(i)->property("filePath").toString() == filePath) {
                            ui->widgetGraph->removeGraph(i);
                        }
                    }
                }
            }
            ui->widgetGraph->replot();
        }
    } else if (event->key() == Qt::Key_Minus) {
        QList<QCPAbstractPlottable*> selection = ui->widgetGraph->selectedPlottables();
        if (selection.size() != 2) {
            return;
        }

        QCPGraph *graph1 = qobject_cast<QCPGraph*>(selection.at(0));
        QCPGraph *graph2 = qobject_cast<QCPGraph*>(selection.at(1));

        if (!graph1 || !graph2) {
            return;
        }

        QVector<double> x1, y1, x2, y2;
        for (auto it = graph1->data()->begin(); it != graph1->data()->end(); ++it) {
            x1.append(it->key);
            y1.append(it->value);
        }
        for (auto it = graph2->data()->begin(); it != graph2->data()->end(); ++it) {
            x2.append(it->key);
            y2.append(it->value);
        }

        QVector<double> y2_interp;
        for (double x : x1) {
            int j = 0;
            while (j < x2.size() - 1 && x2[j+1] < x) {
                j++;
            }

            if (j == x2.size() - 1) {
                y2_interp.append(y2[j]);
            } else {
                double x_j = x2[j];
                double x_j1 = x2[j+1];
                double y_j = y2[j];
                double y_j1 = y2[j+1];
                y2_interp.append(y_j + (y_j1 - y_j) * (x - x_j) / (x_j1 - x_j));
            }
        }

        QVector<double> y_diff;
        for (int i = 0; i < y1.size(); ++i) {
            y_diff.append(y1[i] - y2_interp[i]);
        }

        static int diff_count = 0;
        QString name = QString("diff%1").arg(++diff_count);

        plot(x1, y_diff, Qt::red, name, name, ui->widgetGraph->yAxis->label());
    } else {
        QMainWindow::keyPressEvent(event);
    }
}

