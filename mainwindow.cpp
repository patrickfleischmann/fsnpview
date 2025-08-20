#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "qcustomplot.h"
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
    , localServer(nullptr)
    , m_cursorA_ver_line(nullptr)
    , m_cursorA_hor_line(nullptr)
    , m_cursorA_text(nullptr)
    , m_cursorA_graph(nullptr)
    , m_cursorA_dragging(false)
    , m_cursorB_ver_line(nullptr)
    , m_cursorB_hor_line(nullptr)
    , m_cursorB_text(nullptr)
    , m_cursorB_graph(nullptr)
    , m_cursorB_dragging(false)
{
    ui->setupUi(this);
    ui->widgetGraph->installEventFilter(this);

    const QString serverName = "fsnpview-server";
    localServer = new QLocalServer(this);
    if (!localServer->listen(serverName)) {
        if (localServer->serverError() == QAbstractSocket::AddressInUseError) {
            QLocalServer::removeServer(serverName);
            localServer->listen(serverName);
        }
    }
    connect(localServer, &QLocalServer::newConnection, this, &MainWindow::newConnection);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::plot(const QVector<double> &x, const QVector<double> &y, const QColor &color)
{
    QCustomPlot *customPlot = ui->widgetGraph;
    customPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);
    customPlot->setSelectionRectMode(QCP::srmZoom);

    int graphCount = customPlot->graphCount();
    customPlot->addGraph();
    customPlot->graph(graphCount)->setData(x, y);
    customPlot->graph(graphCount)->setAntialiased(true);

   // QPen pen;
   // pen.setColor(color);
    customPlot->graph(graphCount)->setPen(QPen(color,2));

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
            std::string path = files.at(i).toStdString();
            auto data = std::make_unique<ts::TouchstoneData>(ts::parse_touchstone(path));

            Eigen::ArrayXd xValues = data->freq;
            Eigen::ArrayXd yValues = data->sparams.col(1).abs().log10() * 20; // s21 dB

            std::vector<double> xValuesStdVector(xValues.data(), xValues.data() + xValues.rows() * xValues.cols());
            std::vector<double> yValuesStdVector(yValues.data(), yValues.data() + yValues.rows() * yValues.cols());

            QVector<double> xValuesQVector = QVector<double>(xValuesStdVector.begin(), xValuesStdVector.end());
            QVector<double> yValuesQVector = QVector<double>(yValuesStdVector.begin(), yValuesStdVector.end());

            QColor color = colors.at(parsed_data.size() % colors.size());

            plot(xValuesQVector, yValuesQVector, color);

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
    if (arg1 == Qt::Checked) {
        if (ui->widgetGraph->graphCount() == 0) {
            ui->checkBoxCursorA->setChecked(false);
            return;
        }

        // The cursor will be attached to the selected graph, or the first graph if none is selected.
        QList<QCPGraph*> selected_graphs = ui->widgetGraph->selectedGraphs();
        if (!selected_graphs.empty()) {
            m_cursorA_graph = selected_graphs.first();
        } else {
            m_cursorA_graph = ui->widgetGraph->graph(0);
        }

        m_cursorA_ver_line = new QCPItemLine(ui->widgetGraph);
        m_cursorA_hor_line = new QCPItemLine(ui->widgetGraph);
        m_cursorA_text = new QCPItemText(ui->widgetGraph);

        m_cursorA_ver_line->setPen(QPen(Qt::red));
        m_cursorA_hor_line->setPen(QPen(Qt::red));
        m_cursorA_text->setColor(Qt::red);
        m_cursorA_text->setFont(QFont(font().family(), 8));
        m_cursorA_text->setPen(QPen(Qt::black));

        double initial_x = ui->widgetGraph->xAxis->range().center();
        updateCursor(m_cursorA_graph, m_cursorA_ver_line, m_cursorA_hor_line, m_cursorA_text, initial_x);
        ui->widgetGraph->replot();

    } else {
        if (m_cursorA_ver_line) { ui->widgetGraph->removeItem(m_cursorA_ver_line); m_cursorA_ver_line = nullptr; }
        if (m_cursorA_hor_line) { ui->widgetGraph->removeItem(m_cursorA_hor_line); m_cursorA_hor_line = nullptr; }
        if (m_cursorA_text) { ui->widgetGraph->removeItem(m_cursorA_text); m_cursorA_text = nullptr; }
        m_cursorA_graph = nullptr;
        ui->widgetGraph->replot();
    }
}


void MainWindow::on_checkBoxCursorB_checkStateChanged(const Qt::CheckState &arg1)
{
    if (arg1 == Qt::Checked) {
        if (ui->widgetGraph->graphCount() == 0) {
            ui->checkBoxCursorB->setChecked(false);
            return;
        }

        // The cursor will be attached to the selected graph, or the first graph if none is selected.
        QList<QCPGraph*> selected_graphs = ui->widgetGraph->selectedGraphs();
        if (!selected_graphs.empty()) {
            m_cursorB_graph = selected_graphs.first();
        } else {
            m_cursorB_graph = ui->widgetGraph->graph(0);
        }

        m_cursorB_ver_line = new QCPItemLine(ui->widgetGraph);
        m_cursorB_hor_line = new QCPItemLine(ui->widgetGraph);
        m_cursorB_text = new QCPItemText(ui->widgetGraph);

        m_cursorB_ver_line->setPen(QPen(Qt::blue));
        m_cursorB_hor_line->setPen(QPen(Qt::blue));
        m_cursorB_text->setColor(Qt::blue);
        m_cursorB_text->setFont(QFont(font().family(), 8));
        m_cursorB_text->setPen(QPen(Qt::black));

        double initial_x = ui->widgetGraph->xAxis->range().center();
        updateCursor(m_cursorB_graph, m_cursorB_ver_line, m_cursorB_hor_line, m_cursorB_text, initial_x);
        ui->widgetGraph->replot();

    } else {
        if (m_cursorB_ver_line) { ui->widgetGraph->removeItem(m_cursorB_ver_line); m_cursorB_ver_line = nullptr; }
        if (m_cursorB_hor_line) { ui->widgetGraph->removeItem(m_cursorB_hor_line); m_cursorB_hor_line = nullptr; }
        if (m_cursorB_text) { ui->widgetGraph->removeItem(m_cursorB_text); m_cursorB_text = nullptr; }
        m_cursorB_graph = nullptr;
        ui->widgetGraph->replot();
    }
}


void MainWindow::on_checkBoxLegend_checkStateChanged(const Qt::CheckState &arg1)
{

}

void MainWindow::updateCursor(QCPGraph *graph, QCPItemLine *ver_line, QCPItemLine *hor_line, QCPItemText *text, double x)
{
    QCustomPlot *customPlot = graph->parentPlot();
    if (!customPlot) return;

    QCPGraphDataContainer::const_iterator it = graph->data()->findBegin(x);
    double y = 0;
    if (it != graph->data()->constEnd()) {
        if (it == graph->data()->constBegin()) {
            y = it->value;
        } else {
            QCPGraphDataContainer::const_iterator itLeft = it - 1;
            if (it->key - itLeft->key > 0) {
                y = itLeft->value + (x - itLeft->key)*(it->value - itLeft->value)/(it->key - itLeft->key);
            } else {
                y = it->value;
            }
        }
    }

    ver_line->start->setCoords(x, customPlot->yAxis->range().lower);
    ver_line->end->setCoords(x, customPlot->yAxis->range().upper);

    hor_line->start->setCoords(customPlot->xAxis->range().lower, y);
    hor_line->end->setCoords(customPlot->xAxis->range().upper, y);

    text->setText(QString::number(y, 'f', 2));
    text->position->setCoords(x, y);
    text->setTextAlignment(Qt::AlignLeft | Qt::AlignBottom);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == ui->widgetGraph) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            double x = ui->widgetGraph->xAxis->pixelToCoord(mouseEvent->pos().x());

            if (ui->checkBoxCursorA->isChecked() && m_cursorA_ver_line) {
                if (qAbs(x - m_cursorA_ver_line->start->coords().x()) < (ui->widgetGraph->xAxis->pixelToCoord(5) - ui->widgetGraph->xAxis->pixelToCoord(0))) {
                    m_cursorA_dragging = true;
                    return true;
                }
            }
            if (ui->checkBoxCursorB->isChecked() && m_cursorB_ver_line) {
                if (qAbs(x - m_cursorB_ver_line->start->coords().x()) < (ui->widgetGraph->xAxis->pixelToCoord(5) - ui->widgetGraph->xAxis->pixelToCoord(0))) {
                    m_cursorB_dragging = true;
                    return true;
                }
            }
        } else if (event->type() == QEvent::MouseMove) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (m_cursorA_dragging) {
                double x = ui->widgetGraph->xAxis->pixelToCoord(mouseEvent->pos().x());
                updateCursor(m_cursorA_graph, m_cursorA_ver_line, m_cursorA_hor_line, m_cursorA_text, x);
                ui->widgetGraph->replot();
                return true;
            }
            if (m_cursorB_dragging) {
                double x = ui->widgetGraph->xAxis->pixelToCoord(mouseEvent->pos().x());
                updateCursor(m_cursorB_graph, m_cursorB_ver_line, m_cursorB_hor_line, m_cursorB_text, x);
                ui->widgetGraph->replot();
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            m_cursorA_dragging = false;
            m_cursorB_dragging = false;
        }
    }
    return QObject::eventFilter(obj, event);
}
