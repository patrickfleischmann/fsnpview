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
#include <QDebug>

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

    m_network_lumped_model->setColumnCount(3);
    m_network_lumped_model->setHorizontalHeaderLabels({"Name", "Value", "Unit"});
    connect(m_network_lumped_model, &QStandardItemModel::itemChanged, this, &MainWindow::onNetworkLumpedModelChanged);

    m_network_cascade_model->setColumnCount(3);
    m_network_cascade_model->setHorizontalHeaderLabels({"Active", "Value", "Unit"});
    connect(m_network_cascade_model, &QStandardItemModel::itemChanged, this, &MainWindow::onNetworkCascadeModelChanged);
    connect(m_network_cascade_model, &NetworkItemModel::networkDropped, this, &MainWindow::onCascadeNetworkDropped);
    connect(m_network_lumped_model, &NetworkItemModel::networkDropped, this, &MainWindow::onLumpedNetworkDropped);
    connect(m_network_files_model, &NetworkItemModel::networkDropped, this, &MainWindow::onCascadeNetworkDropped);
}

void MainWindow::setupViews()
{
    ui->tableViewNetworkFiles->setModel(m_network_files_model);
    ui->tableViewNetworkFiles->setDragDropMode(QAbstractItemView::DragOnly);
    ui->tableViewNetworkFiles->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableViewNetworkFiles->setSelectionBehavior(QAbstractItemView::SelectRows);

    ui->tableViewNetworkLumped->setModel(m_network_lumped_model);
    ui->tableViewNetworkLumped->setDragDropMode(QAbstractItemView::DragDrop);
    ui->tableViewNetworkLumped->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableViewNetworkLumped->setSelectionBehavior(QAbstractItemView::SelectRows);

    m_unit_delegate = new UnitDelegate(this);
    ui->tableViewNetworkLumped->setItemDelegateForColumn(2, m_unit_delegate);
    ui->tableViewCascade->setItemDelegateForColumn(2, m_unit_delegate);

    ui->tableViewCascade->setModel(m_network_cascade_model);
    ui->tableViewCascade->setDragDropMode(QAbstractItemView::DragDrop);
    ui->tableViewCascade->setAcceptDrops(true);
}

void MainWindow::populateLumpedNetworkTable()
{
    QList<NetworkLumped*> lumped_networks;
    lumped_networks.append(new NetworkLumped(NetworkLumped::NetworkType::R_series, 50));
    lumped_networks.append(new NetworkLumped(NetworkLumped::NetworkType::R_shunt, 50));
    lumped_networks.append(new NetworkLumped(NetworkLumped::NetworkType::C_series, 1e-12));
    lumped_networks.append(new NetworkLumped(NetworkLumped::NetworkType::C_shunt, 1e-12));
    lumped_networks.append(new NetworkLumped(NetworkLumped::NetworkType::L_series, 1e-9));
    lumped_networks.append(new NetworkLumped(NetworkLumped::NetworkType::L_shunt, 1e-9));

    for (NetworkLumped* network : lumped_networks) {
        QList<QStandardItem*> row_items;
        QStandardItem* name_item = new QStandardItem(network->name());
        name_item->setEditable(false);
        name_item->setData(QVariant::fromValue(qobject_cast<Network*>(network)), Qt::UserRole);
        row_items.append(name_item);

        QStandardItem* value_item = new QStandardItem(QString::number(network->value()));
        row_items.append(value_item);

        QStandardItem* unit_item = new QStandardItem(network->unit());
        row_items.append(unit_item);

        m_network_lumped_model->appendRow(row_items);
        m_networks.append(network);
    }
}

void MainWindow::processFiles(const QStringList &files, bool autoscale)
{
    for (const QString &file : files) {
        Network* network = new NetworkFile(file);
        QList<QStandardItem*> row;
        QStandardItem* checkItem = new QStandardItem();
        checkItem->setCheckable(true);
        checkItem->setCheckState(Qt::Unchecked);
        checkItem->setData(QVariant::fromValue(network), Qt::UserRole);
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

void MainWindow::onLumpedNetworkDropped(Network* network, const QModelIndex& parent)
{
    qDebug() << "Lumped network dropped";
    addNetworkToCascade(network, parent.row());
}

void MainWindow::onCascadeNetworkDropped(Network* network, const QModelIndex& parent)
{
    qDebug() << "Cascade network dropped";
    QModelIndex source_index;
    for (int i=0; i < m_network_cascade_model->rowCount(); ++i) {
        if (m_network_cascade_model->item(i,0)->data(Qt::UserRole).value<Network*>() == network) {
            source_index = m_network_cascade_model->index(i, 0);
            break;
        }
    }

    if (source_index.isValid()) {
        qDebug() << "reordering";
        QList<QStandardItem*> taken_items = m_network_cascade_model->takeRow(source_index.row());
        m_network_cascade_model->insertRow(parent.row(), taken_items);
        m_cascade->removeNetwork(source_index.row());
        m_cascade->addNetwork(network, parent.row());
    } else {
        addNetworkToCascade(network, parent.row());
    }
    updatePlots();
}

void MainWindow::addNetworkToCascade(Network* network, int row)
{
    if (!network) return;

    Network* new_network = network->clone();
    m_networks.append(new_network);
    m_cascade->addNetwork(new_network, row);
    addItemsToCascadeModel(new_network, row);

    updatePlots();
}

void MainWindow::removeNetworkFromCascade(Network* network)
{
    int i = 0;
    for (auto net : m_cascade->getNetworks()) {
        if (net == network) {
            m_cascade->removeNetwork(i);
            break;
        }
        i++;
    }
}

void MainWindow::addItemsToCascadeModel(Network* network, int row)
{
    QList<QStandardItem*> row_items;
    QStandardItem* name_item = new QStandardItem(network->name());
    name_item->setCheckable(true);
    name_item->setCheckState(Qt::Checked);
    name_item->setData(QVariant::fromValue(network), Qt::UserRole);
    row_items.append(name_item);

    NetworkLumped* lumped_network = qobject_cast<NetworkLumped*>(network);
    if (lumped_network) {
        QStandardItem* value_item = new QStandardItem(QString::number(lumped_network->value()));
        row_items.append(value_item);

        QStandardItem* unit_item = new QStandardItem(lumped_network->unit());
        row_items.append(unit_item);
    } else {
        row_items.append(new QStandardItem("N/A"));
        row_items.append(new QStandardItem("N/A"));
    }
    m_network_cascade_model->insertRow(row, row_items);
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
        Network* network = item->data(Qt::UserRole).value<Network*>();
        if(network) {
            network->setVisible(item->checkState() == Qt::Checked);
            updatePlots();
        }
    }
}

void MainWindow::onNetworkLumpedModelChanged(QStandardItem *item)
{
    QStandardItem* name_item = m_network_lumped_model->item(item->row(), 0);
    NetworkLumped* network = qobject_cast<NetworkLumped*>(name_item->data(Qt::UserRole).value<Network*>());
    if (network) {
        if (item->column() == 1) {
            network->setValue(item->text().toDouble());
        } else if (item->column() == 2) {
            network->setUnit(item->text());
        }
        updatePlots();
    }
}

void MainWindow::onNetworkCascadeModelChanged(QStandardItem *item)
{
    QStandardItem* name_item = m_network_cascade_model->item(item->row(), 0);
    Network* network_base = name_item->data(Qt::UserRole).value<Network*>();
    if (network_base) {
        if (item->column() == 0) {
            network_base->setActive(item->checkState() == Qt::Checked);
        }
        NetworkLumped* network_lumped = qobject_cast<NetworkLumped*>(network_base);
        if (network_lumped) {
            if (item->column() == 1) {
                network_lumped->setValue(item->text().toDouble());
            } else if (item->column() == 2) {
                network_lumped->setUnit(item->text());
            }
        }
        updatePlots();
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
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        QModelIndexList selected_indexes = ui->tableViewCascade->selectionModel()->selectedIndexes();
        QList<int> rows_to_remove;
        for (const QModelIndex& index : selected_indexes) {
            if (!rows_to_remove.contains(index.row())) {
                rows_to_remove.append(index.row());
            }
        }
        std::sort(rows_to_remove.rbegin(), rows_to_remove.rend());

        for (int row : rows_to_remove) {
            QStandardItem* item = m_network_cascade_model->item(row);
            if (item) {
                Network* network = item->data(Qt::UserRole).value<Network*>();
                removeNetworkFromCascade(network);
                m_networks.removeOne(network);
                m_network_cascade_model->removeRow(row);
            }
        }
        updatePlots();
    } else if (event->key() == Qt::Key_Minus) {
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
