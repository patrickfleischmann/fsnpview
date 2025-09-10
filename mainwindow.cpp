#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "networkfile.h"
#include "networklumped.h"
#include "networkitemmodel.h"
#include "qcustomplot.h"
#include "server.h"
#include <QFileDialog>
#include <QMenu>
#include <QMenuBar>
#include <QCheckBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_server(new Server(this))
    , m_cascade(new NetworkCascade(this))
    , m_network_files_model(new NetworkItemModel(this))
    , m_network_lumped_model(new NetworkItemModel(this))
    , m_network_cascade_model(new NetworkItemModel(this))
{
    ui->setupUi(this);

    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    QAction *openAct = new QAction(tr("&Open..."), this);
    fileMenu->addAction(openAct);
    connect(openAct, &QAction::triggered, this, &MainWindow::on_actionOpen_triggered);

    QCheckBox* cascade_checkbox = new QCheckBox("Plot Cascade", this);
    ui->horizontalLayout->insertWidget(5, cascade_checkbox);
    connect(cascade_checkbox, &QCheckBox::stateChanged, this, [this](int state){
        m_cascade->setVisible(state == Qt::Checked);
        updatePlots();
    });

    setupModels();
    setupViews();
    populateLumpedNetworkTable();

    connect(m_server, &Server::filesReceived, this, &MainWindow::onFilesReceived);
}

MainWindow::~MainWindow()
{
    delete ui;
    qDeleteAll(m_networks);
}

void MainWindow::setupModels()
{
    m_network_files_model->setColumnCount(2);
    m_network_files_model->setHorizontalHeaderLabels({"Plot", "File"});
    connect(m_network_files_model, &QStandardItemModel::itemChanged, this, &MainWindow::onNetworkFilesModelChanged);

    m_network_lumped_model->setColumnCount(2);
    m_network_lumped_model->setHorizontalHeaderLabels({"Plot", "Name"});
    connect(m_network_lumped_model, &QStandardItemModel::itemChanged, this, &MainWindow::onNetworkLumpedModelChanged);

    m_network_cascade_model->setColumnCount(2);
    m_network_cascade_model->setHorizontalHeaderLabels({"Active", "Name"});
    connect(m_network_cascade_model, &QStandardItemModel::itemChanged, this, &MainWindow::onNetworkCascadeModelChanged);
    connect(m_network_cascade_model, &NetworkItemModel::networkDropped, this, &MainWindow::onNetworkDropped);
}

void MainWindow::setupViews()
{
    ui->tableViewNetworkFiles->setModel(m_network_files_model);
    ui->tableViewNetworkFiles->setDragDropMode(QAbstractItemView::DragOnly);
    ui->tableViewNetworkFiles->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableViewNetworkFiles->setSelectionBehavior(QAbstractItemView::SelectRows);

    ui->tableViewNetworkLumped->setModel(m_network_lumped_model);
    ui->tableViewNetworkLumped->setDragDropMode(QAbstractItemView::DragOnly);
    ui->tableViewNetworkLumped->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableViewNetworkLumped->setSelectionBehavior(QAbstractItemView::SelectRows);


    ui->tableViewCascade->setModel(m_network_cascade_model);
    ui->tableViewCascade->setDragDropMode(QAbstractItemView::DropOnly);
    ui->tableViewCascade->setAcceptDrops(true);
}

void MainWindow::populateLumpedNetworkTable()
{
    m_networks.append(new NetworkLumped(NetworkLumped::NetworkType::R_series, 50));
    m_networks.append(new NetworkLumped(NetworkLumped::NetworkType::R_shunt, 50));
    m_networks.append(new NetworkLumped(NetworkLumped::NetworkType::C_series, 1e-12));
    m_networks.append(new NetworkLumped(NetworkLumped::NetworkType::C_shunt, 1e-12));
    m_networks.append(new NetworkLumped(NetworkLumped::NetworkType::L_series, 1e-9));
    m_networks.append(new NetworkLumped(NetworkLumped::NetworkType::L_shunt, 1e-9));

    for (auto network_ptr : qAsConst(m_networks)) {
        if (dynamic_cast<NetworkLumped*>(network_ptr)) {
            QList<QStandardItem*> row;
            QStandardItem* checkItem = new QStandardItem();
            checkItem->setCheckable(true);
            checkItem->setCheckState(Qt::Unchecked);
            checkItem->setData(QVariant::fromValue(reinterpret_cast<quintptr>(network_ptr)), Qt::UserRole);
            row.append(checkItem);
            row.append(new QStandardItem(network_ptr->name()));
            m_network_lumped_model->appendRow(row);
        }
    }
}

void MainWindow::processFiles(const QStringList &files)
{
    for (const QString &file : files) {
        Network* network = new NetworkFile(file);
        QList<QStandardItem*> row;
        QStandardItem* checkItem = new QStandardItem();
        checkItem->setCheckable(true);
        checkItem->setCheckState(Qt::Unchecked);
        checkItem->setData(QVariant::fromValue(reinterpret_cast<quintptr>(network)), Qt::UserRole);
        row.append(checkItem);
        row.append(new QStandardItem(network->name()));
        m_network_files_model->appendRow(row);
        m_networks.append(network);
    }
}

void MainWindow::onFilesReceived(const QStringList &files)
{
    processFiles(files);
}

void MainWindow::onNetworkDropped(Network* network, const QModelIndex& parent)
{
    Q_UNUSED(parent);
    m_cascade->addNetwork(network);

    QList<QStandardItem*> row;
    QStandardItem* checkItem = new QStandardItem();
    checkItem->setCheckable(true);
    checkItem->setCheckState(Qt::Checked);
    row.append(checkItem);
    row.append(new QStandardItem(network->name()));
    m_network_cascade_model->appendRow(row);

    updatePlots();
}


void MainWindow::plot(const QVector<double> &x, const QVector<double> &y, const QColor &color, const QString &name, const QString &filePath, const QString &yAxisLabel, Qt::PenStyle style)
{
    Q_UNUSED(filePath);
    QCPGraph *graph = ui->widgetGraph->addGraph(ui->widgetGraph->xAxis, ui->widgetGraph->yAxis);
    graph->setData(x, y);
    graph->setPen(QPen(color, 2, style));
    graph->setName(name);
    ui->widgetGraph->yAxis->setLabel(yAxisLabel);
}

void MainWindow::updatePlots()
{
    QStringList required_graphs;
    bool isPhase = ui->checkBoxPhase->isChecked();
    QString yAxisLabel = isPhase ? "Phase (deg)" : "Magnitude (dB)";

    // Determine which s-parameters are checked
    QStringList checked_sparams;
    if (ui->checkBoxS11->isChecked()) checked_sparams << "s11";
    if (ui->checkBoxS21->isChecked()) checked_sparams << "s21";
    if (ui->checkBoxS12->isChecked()) checked_sparams << "s12";
    if (ui->checkBoxS22->isChecked()) checked_sparams << "s22";

    // Build list of required graphs from individual networks
    for (auto network : qAsConst(m_networks)) {
        if (network->isVisible()) {
            for (const auto& sparam : checked_sparams) {
                required_graphs << network->name() + "_" + sparam;
            }
        }
    }

    // Build list of required graphs from cascade
    if (m_cascade->isVisible()) {
        for (const auto& sparam : checked_sparams) {
            required_graphs << m_cascade->name() + "_" + sparam;
        }
    }

    // Remove graphs that are no longer needed
    for (int i = ui->widgetGraph->graphCount() - 1; i >= 0; --i) {
        if (!required_graphs.contains(ui->widgetGraph->graph(i)->name())) {
            ui->widgetGraph->removeGraph(i);
        }
    }

    // Add new graphs
    for (const auto& sparam : checked_sparams) {
        int sparam_idx_to_plot = -1;
        if (sparam == "s11") sparam_idx_to_plot = 0;
        else if (sparam == "s21") sparam_idx_to_plot = 1;
        else if (sparam == "s12") sparam_idx_to_plot = 2;
        else if (sparam == "s22") sparam_idx_to_plot = 3;

        // Individual networks
        for (auto network : qAsConst(m_networks)) {
            if (network->isVisible()) {
                QString graph_name = network->name() + "_" + sparam;
                bool graph_exists = false;
                for(int i=0; i<ui->widgetGraph->graphCount(); ++i) {
                    if(ui->widgetGraph->graph(i)->name() == graph_name) {
                        graph_exists = true;
                        break;
                    }
                }
                if (!graph_exists) {
                    auto plotData = network->getPlotData(sparam_idx_to_plot, isPhase);
                    plot(plotData.first, plotData.second, network->color(), graph_name, "", yAxisLabel);
                }
            }
        }

        // Cascade
        if (m_cascade->isVisible()) {
            QString graph_name = m_cascade->name() + "_" + sparam;
             bool graph_exists = false;
            for(int i=0; i<ui->widgetGraph->graphCount(); ++i) {
                if(ui->widgetGraph->graph(i)->name() == graph_name) {
                    graph_exists = true;
                    break;
                }
            }
            if(!graph_exists) {
                auto plotData = m_cascade->getPlotData(sparam_idx_to_plot, isPhase);
                plot(plotData.first, plotData.second, m_cascade->color(), graph_name, "", yAxisLabel, Qt::DashLine);
            }
        }
    }

    ui->widgetGraph->replot();
}


void MainWindow::onNetworkFilesModelChanged(QStandardItem *item)
{
    if (item->column() == 0) {
        quintptr net_ptr_val = item->data(Qt::UserRole).value<quintptr>();
        Network* network = reinterpret_cast<Network*>(net_ptr_val);
        if(network) {
            network->setVisible(item->checkState() == Qt::Checked);
            updatePlots();
        }
    }
}

void MainWindow::onNetworkLumpedModelChanged(QStandardItem *item)
{
    if (item->column() == 0) {
        quintptr net_ptr_val = item->data(Qt::UserRole).value<quintptr>();
        Network* network = reinterpret_cast<Network*>(net_ptr_val);
        if(network) {
            network->setVisible(item->checkState() == Qt::Checked);
            updatePlots();
        }
    }
}

void MainWindow::onNetworkCascadeModelChanged(QStandardItem *item)
{
    if (item->column() == 0) {
        int row = item->row();
        const auto& cascade_nets = m_cascade->getNetworks();
        if(row < cascade_nets.size()) {
            cascade_nets[row]->setActive(item->checkState() == Qt::Checked);
            updatePlots();
        }
    }
}


void MainWindow::on_actionOpen_triggered()
{
    QStringList filePaths = QFileDialog::getOpenFileNames(
        this,
        tr("Open S-Parameter File"),
        "",
        tr("Touchstone files (*.s*p);;All files (*.*)"));
    if (!filePaths.isEmpty()) {
        processFiles(filePaths);
    }
}

void MainWindow::on_pushButtonAutoscale_clicked()
{
    ui->widgetGraph->rescaleAxes();
    ui->widgetGraph->replot();
}

void MainWindow::on_checkBoxLegend_checkStateChanged(const Qt::CheckState &arg1)
{
    ui->widgetGraph->legend->setVisible(arg1 == Qt::Checked);
    ui->widgetGraph->replot();
}

void MainWindow::on_checkBox_checkStateChanged(const Qt::CheckState &arg1)
{
    if (arg1 == Qt::Checked) {
        ui->widgetGraph->xAxis->setScaleType(QCPAxis::stLogarithmic);
    } else {
        ui->widgetGraph->xAxis->setScaleType(QCPAxis::stLinear);
    }
    ui->widgetGraph->replot();
}

void MainWindow::on_checkBoxS11_checkStateChanged(const Qt::CheckState &arg1)
{
    Q_UNUSED(arg1);
    updatePlots();
}

void MainWindow::on_checkBoxS21_checkStateChanged(const Qt::CheckState &arg1)
{
    Q_UNUSED(arg1);
    updatePlots();
}

void MainWindow::on_checkBoxS12_checkStateChanged(const Qt::CheckState &arg1)
{
    Q_UNUSED(arg1);
    updatePlots();
}

void MainWindow::on_checkBoxS22_checkStateChanged(const Qt::CheckState &arg1)
{
    Q_UNUSED(arg1);
    updatePlots();
}

void MainWindow::on_checkBoxPhase_checkStateChanged(const Qt::CheckState &arg1)
{
    Q_UNUSED(arg1);
    updatePlots();
}
