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

#include <QItemSelectionModel>
#include <QSignalBlocker>
#include <QColorDialog>
#include <QVariant>
#include <QHeaderView>
#include <QWheelEvent>
#include <QStyledItemDelegate>
#include <QFont>
#include <QStyle>

namespace {

class SelectionBoldDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

protected:
    void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override
    {
        QStyledItemDelegate::initStyleOption(option, index);
        if (option->state.testFlag(QStyle::State_Selected)) {
            option->state.setFlag(QStyle::State_Selected, false);
            option->font.setBold(true);
        }
    }
};

} // namespace

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
    ui->tableViewNetworkLumped->viewport()->installEventFilter(this);
    ui->tableViewCascade->viewport()->installEventFilter(this);

    m_plot_manager->setNetworks(m_networks);
    m_plot_manager->setCascade(m_cascade);

    connect(ui->widgetGraph, &QCustomPlot::selectionChangedByUser,
            this, &MainWindow::onGraphSelectionChangedByUser);
    connect(ui->tableViewNetworkFiles->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &MainWindow::onNetworkFilesSelectionChanged);
    connect(ui->tableViewNetworkLumped->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &MainWindow::onNetworkLumpedSelectionChanged);
    connect(ui->tableViewCascade->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &MainWindow::onNetworkCascadeSelectionChanged);
    connect(ui->tableViewNetworkFiles, &QTableView::clicked, this, &MainWindow::onColorColumnClicked);
    connect(ui->tableViewNetworkLumped, &QTableView::clicked, this, &MainWindow::onColorColumnClicked);
    connect(ui->tableViewCascade, &QTableView::clicked, this, &MainWindow::onColorColumnClicked);

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
    m_network_files_model->setColumnCount(3);
    m_network_files_model->setHorizontalHeaderLabels({"Plot", "Color", "File"});
    connect(m_network_files_model, &QStandardItemModel::itemChanged, this, &MainWindow::onNetworkFilesModelChanged);

    m_network_lumped_model->setColumnCount(4);
    m_network_lumped_model->setHorizontalHeaderLabels({"Plot", "Color", "Name", "Value"});
    connect(m_network_lumped_model, &QStandardItemModel::itemChanged, this, &MainWindow::onNetworkLumpedModelChanged);

    m_network_cascade_model->setColumnCount(4);
    m_network_cascade_model->setHorizontalHeaderLabels({"Active", "Color", "Name", "Value"});
    connect(m_network_cascade_model, &QStandardItemModel::itemChanged, this, &MainWindow::onNetworkCascadeModelChanged);
    connect(m_network_cascade_model, &NetworkItemModel::networkDropped, this, &MainWindow::onNetworkDropped);
}

void MainWindow::setupViews()
{
    ui->tableViewNetworkFiles->setModel(m_network_files_model);
    ui->tableViewNetworkFiles->setDragDropMode(QAbstractItemView::DragOnly);
    ui->tableViewNetworkFiles->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->tableViewNetworkFiles->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableViewNetworkFiles->setItemDelegate(new SelectionBoldDelegate(ui->tableViewNetworkFiles));
    setupTableColumns(ui->tableViewNetworkFiles);

    ui->tableViewNetworkLumped->setModel(m_network_lumped_model);
    ui->tableViewNetworkLumped->setDragDropMode(QAbstractItemView::DragOnly);
    ui->tableViewNetworkLumped->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->tableViewNetworkLumped->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableViewNetworkLumped->setItemDelegate(new SelectionBoldDelegate(ui->tableViewNetworkLumped));
    setupTableColumns(ui->tableViewNetworkLumped);


    ui->tableViewCascade->setModel(m_network_cascade_model);
    ui->tableViewCascade->setDragEnabled(true);
    ui->tableViewCascade->setAcceptDrops(true);
    ui->tableViewCascade->setDragDropMode(QAbstractItemView::DragDrop);
    ui->tableViewCascade->setDefaultDropAction(Qt::MoveAction);
    ui->tableViewCascade->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->tableViewCascade->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableViewCascade->setDropIndicatorShown(true);
    ui->tableViewCascade->setStyleSheet("QTableView::item:drop-indicator { border-top: 2px solid #0000ff; border-bottom: 2px solid #0000ff; }");
    ui->tableViewCascade->setItemDelegate(new SelectionBoldDelegate(ui->tableViewCascade));
    setupTableColumns(ui->tableViewCascade);
}

void MainWindow::setupTableColumns(QTableView* view)
{
    view->resizeColumnToContents(0);
    view->resizeColumnToContents(1);
    QHeaderView *header = view->horizontalHeader();
    header->setSectionResizeMode(0, QHeaderView::Fixed);
    header->setSectionResizeMode(1, QHeaderView::Fixed);
    for (int i = 2; i < view->model()->columnCount(); ++i) {
        header->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    }
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
            network_ptr->setColor(m_plot_manager->nextColor());
            QList<QStandardItem*> row;
            QStandardItem* checkItem = new QStandardItem();
            checkItem->setCheckable(true);
            checkItem->setCheckState(Qt::Unchecked);
            checkItem->setData(QVariant::fromValue(reinterpret_cast<quintptr>(network_ptr)), Qt::UserRole);
            row.append(checkItem);
            QStandardItem* colorItem = new QStandardItem();
            colorItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            colorItem->setBackground(network_ptr->color());
            row.append(colorItem);
            QStandardItem* nameItem = new QStandardItem(network_ptr->name());
            nameItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            row.append(nameItem);
            NetworkLumped* lumped = static_cast<NetworkLumped*>(network_ptr);
            QStandardItem* valueItem = new QStandardItem(QString::number(lumped->value()));
            row.append(valueItem);
            m_network_lumped_model->appendRow(row);
        }
    }
}

void MainWindow::processFiles(const QStringList &files, bool autoscale)
{
    for (const QString &file : files) {
        Network* network = new NetworkFile(file);
        network->setColor(m_plot_manager->nextColor());
        QList<QStandardItem*> row;
        QStandardItem* checkItem = new QStandardItem();
        checkItem->setCheckable(true);
        checkItem->setCheckState(Qt::Checked);
        checkItem->setData(QVariant::fromValue(reinterpret_cast<quintptr>(network)), Qt::UserRole);
        row.append(checkItem);
        QStandardItem* colorItem = new QStandardItem();
        colorItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        colorItem->setBackground(network->color());
        row.append(colorItem);
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

void MainWindow::onNetworkDropped(Network* network, int row, const QModelIndex& parent)
{
    Q_UNUSED(parent);

    if (row < 0 || row > m_network_cascade_model->rowCount())
        row = m_network_cascade_model->rowCount();

    const QList<Network*> cascadeNets = m_cascade->getNetworks();
    int existingIndex = cascadeNets.indexOf(network);

    if (existingIndex >= 0) {
        if (existingIndex != row) {
            m_cascade->moveNetwork(existingIndex, row);
            auto items = m_network_cascade_model->takeRow(existingIndex);
            m_network_cascade_model->insertRow(row, items);
        }
    } else {
        Network* cloned = network->clone(m_cascade);
        m_cascade->insertNetwork(row, cloned);

        QList<QStandardItem*> items;
        QStandardItem* checkItem = new QStandardItem();
        checkItem->setCheckable(true);
        checkItem->setCheckState(Qt::Checked);
        checkItem->setData(QVariant::fromValue(reinterpret_cast<quintptr>(cloned)), Qt::UserRole);
        items.append(checkItem);
        QStandardItem* colorItem = new QStandardItem();
        colorItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        colorItem->setBackground(cloned->color());
        items.append(colorItem);
        QStandardItem* nameItem = new QStandardItem(cloned->name());
        nameItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        items.append(nameItem);

        QStandardItem* valueItem = new QStandardItem();
        if (auto lumped = dynamic_cast<NetworkLumped*>(cloned)) {
            valueItem->setText(QString::number(lumped->value()));
        } else {
            valueItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        }
        items.append(valueItem);
        m_network_cascade_model->insertRow(row, items);
    }

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

    PlotType type = PlotType::Magnitude;
    if (ui->checkBoxPhase->isChecked())
        type = PlotType::Phase;
    else if (ui->checkBoxVSWR->isChecked())
        type = PlotType::VSWR;
    else if (ui->checkBoxSmith->isChecked())
        type = PlotType::Smith;

    m_plot_manager->updatePlots(checked_sparams, type);
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
    } else if (item->column() == 3) {
        quintptr net_ptr_val = m_network_lumped_model->item(item->row(), 0)->data(Qt::UserRole).value<quintptr>();
        NetworkLumped* network = dynamic_cast<NetworkLumped*>(reinterpret_cast<Network*>(net_ptr_val));
        if(network) {
            bool ok = false;
            double val = item->text().toDouble(&ok);
            if (ok) {
                network->setValue(val);
                m_network_lumped_model->item(item->row(), 2)->setText(network->name());
                updatePlots();
            }
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
    } else if (item->column() == 3) {
        quintptr net_ptr_val = m_network_cascade_model->item(item->row(), 0)->data(Qt::UserRole).value<quintptr>();
        Network* network_base = reinterpret_cast<Network*>(net_ptr_val);
        if(auto network = dynamic_cast<NetworkLumped*>(network_base)) {
            bool ok = false;
            double val = item->text().toDouble(&ok);
            if (ok) {
                network->setValue(val);
                m_network_cascade_model->item(item->row(), 2)->setText(network->name());
                updatePlots();
            }
        }
    }
}

void MainWindow::onGraphSelectionChangedByUser()
{
    QSet<Network*> selectedNetworks;
    const auto graphs = ui->widgetGraph->selectedGraphs();
    for (QCPGraph *graph : graphs) {
        quintptr ptrVal = graph->property("network_ptr").value<quintptr>();
        Network* network = reinterpret_cast<Network*>(ptrVal);
        if (network)
            selectedNetworks.insert(network);
    }

    auto syncSelection = [&](QTableView *view, NetworkItemModel *model) {
        QItemSelectionModel *selModel = view->selectionModel();
        QSignalBlocker blocker(selModel);
        selModel->clearSelection();
        for (int row = 0; row < model->rowCount(); ++row) {
            quintptr ptrVal = model->item(row, 0)->data(Qt::UserRole).value<quintptr>();
            Network *network = reinterpret_cast<Network*>(ptrVal);
            if (selectedNetworks.contains(network)) {
                selModel->select(model->index(row, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
            }
        }
    };

    syncSelection(ui->tableViewNetworkFiles, m_network_files_model);
    syncSelection(ui->tableViewNetworkLumped, m_network_lumped_model);
    syncSelection(ui->tableViewCascade, m_network_cascade_model);
}

void MainWindow::onNetworkFilesSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    Q_UNUSED(selected);
    Q_UNUSED(deselected);
    updateGraphSelectionFromTables();
}

void MainWindow::onNetworkLumpedSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    Q_UNUSED(selected);
    Q_UNUSED(deselected);
    updateGraphSelectionFromTables();
}

void MainWindow::onNetworkCascadeSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    Q_UNUSED(selected);
    Q_UNUSED(deselected);
    updateGraphSelectionFromTables();
}

void MainWindow::onColorColumnClicked(const QModelIndex &index)
{
    if (index.column() != 1)
        return;

    const QStandardItemModel* model = qobject_cast<const QStandardItemModel*>(index.model());
    quintptr net_ptr_val = model->item(index.row(), 0)->data(Qt::UserRole).value<quintptr>();
    Network* network = reinterpret_cast<Network*>(net_ptr_val);
    if (!network)
        return;

    QColor color = QColorDialog::getColor(network->color(), this, tr("Select Color"));
    if (!color.isValid())
        return;

    network->setColor(color);
    auto updateColor = [&](NetworkItemModel* m){
        for (int r = 0; r < m->rowCount(); ++r) {
            quintptr ptrVal = m->item(r, 0)->data(Qt::UserRole).value<quintptr>();
            if (reinterpret_cast<Network*>(ptrVal) == network) {
                m->item(r, 1)->setBackground(color);
            }
        }
    };
    updateColor(m_network_files_model);
    updateColor(m_network_lumped_model);
    updateColor(m_network_cascade_model);
    updatePlots();
}

void MainWindow::updateGraphSelectionFromTables()
{
    QSet<Network*> selectedNetworks;
    auto collect = [&](QTableView *view, NetworkItemModel *model) {
        const QModelIndexList rows = view->selectionModel()->selectedRows();
        for (const QModelIndex &index : rows) {
            quintptr ptrVal = model->item(index.row(), 0)->data(Qt::UserRole).value<quintptr>();
            Network *network = reinterpret_cast<Network*>(ptrVal);
            if (network)
                selectedNetworks.insert(network);
        }
    };

    collect(ui->tableViewNetworkFiles, m_network_files_model);
    collect(ui->tableViewNetworkLumped, m_network_lumped_model);
    collect(ui->tableViewCascade, m_network_cascade_model);

    for (int i = 0; i < ui->widgetGraph->graphCount(); ++i) {
        QCPGraph *graph = ui->widgetGraph->graph(i);
        Network *network = reinterpret_cast<Network*>(graph->property("network_ptr").value<quintptr>());
        if (network && selectedNetworks.contains(network)) {
            graph->setSelection(QCPDataSelection(graph->data()->dataRange()));
        } else {
            graph->setSelection(QCPDataSelection());
        }
    }
    ui->widgetGraph->replot();
    m_plot_manager->selectionChanged();
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
        QSharedPointer<QCPAxisTickerLog> logTicker(new QCPAxisTickerLog);
        logTicker->setSubTickCount(8);
        logTicker->setLogBase(10);
        ui->widgetGraph->xAxis->setTicker(logTicker);
    } else {
        ui->widgetGraph->xAxis->setScaleType(QCPAxis::stLinear);
        ui->widgetGraph->xAxis->setTicker(QSharedPointer<QCPAxisTicker>(new QCPAxisTicker));
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
            // remove from cascade if present; network may appear multiple times
            int index = -1;
            while ((index = m_cascade->getNetworks().indexOf(network)) != -1) {
                m_cascade->removeNetwork(index);
                m_network_cascade_model->removeRow(index);
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
    if (ui->checkBoxPhase->isChecked()) {
        ui->checkBoxVSWR->setChecked(false);
        ui->checkBoxSmith->setChecked(false);
    }
    updatePlots();
}

void MainWindow::on_checkBoxVSWR_checkStateChanged(const Qt::CheckState &arg1)
{
    Q_UNUSED(arg1);
    if (ui->checkBoxVSWR->isChecked()) {
        ui->checkBoxPhase->setChecked(false);
        ui->checkBoxSmith->setChecked(false);
    }
    updatePlots();
}

void MainWindow::on_checkBoxSmith_checkStateChanged(const Qt::CheckState &arg1)
{
    Q_UNUSED(arg1);
    if (ui->checkBoxSmith->isChecked()) {
        ui->checkBoxPhase->setChecked(false);
        ui->checkBoxVSWR->setChecked(false);
    }
    updatePlots();
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::Wheel) {
        auto handleValueWheel = [&](QTableView *view, NetworkItemModel *model) -> bool {
            QWheelEvent *wheelEvent = static_cast<QWheelEvent*>(event);
            QModelIndex index = view->indexAt(wheelEvent->position().toPoint());
            if (!index.isValid() || index.column() != 3)
                return false;

            QStandardItem *ptrItem = model->item(index.row(), 0);
            QStandardItem *valueItem = model->item(index.row(), 3);
            if (!ptrItem || !valueItem)
                return false;

            quintptr ptrVal = ptrItem->data(Qt::UserRole).value<quintptr>();
            Network *network_base = reinterpret_cast<Network*>(ptrVal);
            auto network = dynamic_cast<NetworkLumped*>(network_base);
            if (!network)
                return false;

            bool ok = false;
            double val = valueItem->text().toDouble(&ok);
            if (ok) {
                if (wheelEvent->angleDelta().y() > 0)
                    val *= 1.25;
                else if (wheelEvent->angleDelta().y() < 0)
                    val *= 0.8;
                valueItem->setText(QString::number(val));
            }
            return true;
        };

        if (obj == ui->tableViewNetworkLumped->viewport()) {
            if (handleValueWheel(ui->tableViewNetworkLumped, m_network_lumped_model))
                return true;
        } else if (obj == ui->tableViewCascade->viewport()) {
            if (handleValueWheel(ui->tableViewCascade, m_network_cascade_model))
                return true;
        }
    }
    return QMainWindow::eventFilter(obj, event);
}
