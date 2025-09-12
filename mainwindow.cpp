#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "networkfile.h"
#include "networklumped.h"
#include "networkitemmodel.h"
#include "qcustomplot.h"
#include "server.h"
#include "plotmanager.h"
#include <QFileDialog>
#include <QMenu>
#include <QMenuBar>
#include <QCheckBox>
#include <QSet>
#include <QTableView>

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
    m_plot_manager = new PlotManager(ui->widgetGraph, this);

    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    QAction *openAct = new QAction(tr("&Open..."), this);
    fileMenu->addAction(openAct);
    connect(openAct, &QAction::triggered, this, &MainWindow::on_actionOpen_triggered);

    setupModels();
    setupViews();
    populateLumpedNetworkTable();

    m_plot_manager->setNetworks(m_networks);
    m_plot_manager->setCascade(m_cascade);

    connect(m_server, &Server::filesReceived, this, &MainWindow::onFilesReceived);
    connect(ui->widgetGraph, &QCustomPlot::plottableClick,
            this, &MainWindow::onGraphSelectionChanged);

    connect(ui->checkBoxCursorA, &QCheckBox::stateChanged, this, [this](int state){
        m_plot_manager->setCursorAVisible(state == Qt::Checked);
    });

    connect(ui->checkBoxCursorB, &QCheckBox::stateChanged, this, [this](int state){
        m_plot_manager->setCursorBVisible(state == Qt::Checked);
    });

    ui->checkBoxS21->setChecked(true);
    updatePlots();
    m_plot_manager->autoscale();
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
    ui->tableViewNetworkFiles->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->tableViewNetworkFiles->setSelectionBehavior(QAbstractItemView::SelectRows);

    ui->tableViewNetworkLumped->setModel(m_network_lumped_model);
    ui->tableViewNetworkLumped->setDragDropMode(QAbstractItemView::DragOnly);
    ui->tableViewNetworkLumped->setSelectionMode(QAbstractItemView::ExtendedSelection);
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

void MainWindow::processFiles(const QStringList &files, bool autoscale)
{
    for (const QString &file : files) {
        Network* network = new NetworkFile(file);
        QList<QStandardItem*> row;
        QStandardItem* checkItem = new QStandardItem();
        checkItem->setCheckable(true);
        checkItem->setCheckState(Qt::Checked);
        checkItem->setData(QVariant::fromValue(reinterpret_cast<quintptr>(network)), Qt::UserRole);
        row.append(checkItem);
        row.append(new QStandardItem(network->name()));
        m_network_files_model->appendRow(row);
        m_networks.append(network);
    }
    m_plot_manager->setNetworks(m_networks);
    updatePlots();
    if(autoscale) m_plot_manager->autoscale();
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
    checkItem->setData(QVariant::fromValue(reinterpret_cast<quintptr>(network)), Qt::UserRole);
    row.append(checkItem);
    row.append(new QStandardItem(network->name()));
    m_network_cascade_model->appendRow(row);

    updatePlots();
}

void MainWindow::onGraphSelectionChanged(QCPAbstractPlottable *plottable,
                                        int, QMouseEvent *)
{
    QCPGraph *graph = qobject_cast<QCPGraph *>(plottable);
    if (!graph)
        return;

    quintptr ptrVal = graph->property("network_ptr").value<quintptr>();
    Network *network = reinterpret_cast<Network *>(ptrVal);
    if (!network)
        return;

    auto selectInView = [network](NetworkItemModel *model, QTableView *view) {
        view->clearSelection();
        for (int r = 0; r < model->rowCount(); ++r) {
            quintptr val = model->item(r, 0)->data(Qt::UserRole).value<quintptr>();
            if (reinterpret_cast<Network *>(val) == network) {
                view->selectRow(r);
                break;
            }
        }
    };

    selectInView(m_network_files_model, ui->tableViewNetworkFiles);
    selectInView(m_network_lumped_model, ui->tableViewNetworkLumped);
    selectInView(m_network_cascade_model, ui->tableViewCascade);
}

void MainWindow::updatePlots()
{
    QStringList checked_sparams;
    if (ui->checkBoxS11->isChecked()) checked_sparams << "s11";
    if (ui->checkBoxS21->isChecked()) checked_sparams << "s21";
    if (ui->checkBoxS12->isChecked()) checked_sparams << "s12";
    if (ui->checkBoxS22->isChecked()) checked_sparams << "s22";

    m_plot_manager->updatePlots(checked_sparams, ui->checkBoxPhase->isChecked());
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
        quintptr net_ptr_val = item->data(Qt::UserRole).value<quintptr>();
        Network* network = reinterpret_cast<Network*>(net_ptr_val);
        if(network) {
            network->setActive(item->checkState() == Qt::Checked);
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
    m_plot_manager->autoscale();
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

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Delete) {
        qInfo() << "Delete key pressed";

        QSet<Network*> fileNetworksToDelete;
        QSet<Network*> lumpedNetworksToHide;

        // Collect from selected graphs
        const auto selectedGraphs = ui->widgetGraph->selectedGraphs();
        for (QCPGraph *graph : selectedGraphs) {
            quintptr ptrVal = graph->property("network_ptr").value<quintptr>();
            Network* network = reinterpret_cast<Network*>(ptrVal);
            if (!network)
                continue;
            if (dynamic_cast<NetworkFile*>(network)) {
                fileNetworksToDelete.insert(network);
            } else if (dynamic_cast<NetworkLumped*>(network)) {
                lumpedNetworksToHide.insert(network);
            }
        }

        // Collect from selected rows in file list
        QModelIndexList selectedFileRows = ui->tableViewNetworkFiles->selectionModel()->selectedRows();
        for (const QModelIndex &index : selectedFileRows) {
            quintptr ptrVal = m_network_files_model->item(index.row(), 0)->data(Qt::UserRole).value<quintptr>();
            Network* network = reinterpret_cast<Network*>(ptrVal);
            if (network)
                fileNetworksToDelete.insert(network);
        }

        // Collect from selected rows in lumped list
        QModelIndexList selectedLumpedRows = ui->tableViewNetworkLumped->selectionModel()->selectedRows();
        for (const QModelIndex &index : selectedLumpedRows) {
            quintptr ptrVal = m_network_lumped_model->item(index.row(), 0)->data(Qt::UserRole).value<quintptr>();
            Network* network = reinterpret_cast<Network*>(ptrVal);
            if (network)
                lumpedNetworksToHide.insert(network);
        }

        bool changed = false;

        // Delete file networks
        for (Network* network : fileNetworksToDelete) {
            // remove from file model
            for (int r = 0; r < m_network_files_model->rowCount(); ++r) {
                quintptr ptrVal = m_network_files_model->item(r, 0)->data(Qt::UserRole).value<quintptr>();
                if (reinterpret_cast<Network*>(ptrVal) == network) {
                    m_network_files_model->removeRow(r);
                    break;
                }
            }
            // remove from cascade if present
            const QList<Network*> cascadeNets = m_cascade->getNetworks();
            for (int i = 0; i < cascadeNets.size(); ++i) {
                if (cascadeNets[i] == network) {
                    m_cascade->removeNetwork(i);
                    m_network_cascade_model->removeRow(i);
                    break;
                }
            }
            m_networks.removeOne(network);
            delete network;
            changed = true;
        }

        // Hide lumped networks
        for (Network* network : lumpedNetworksToHide) {
            for (int r = 0; r < m_network_lumped_model->rowCount(); ++r) {
                quintptr ptrVal = m_network_lumped_model->item(r, 0)->data(Qt::UserRole).value<quintptr>();
                if (reinterpret_cast<Network*>(ptrVal) == network) {
                    QStandardItem *item = m_network_lumped_model->item(r, 0);
                    if (item->checkState() == Qt::Checked)
                        item->setCheckState(Qt::Unchecked);
                    else
                        network->setVisible(false);
                    changed = true;
                    break;
                }
            }
        }

        if (changed) {
            m_plot_manager->setNetworks(m_networks);
            updatePlots();
        }

    } else if (event->key() == Qt::Key_Minus)
    {
        m_plot_manager->createMathPlot();
    }
    QMainWindow::keyPressEvent(event);
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
