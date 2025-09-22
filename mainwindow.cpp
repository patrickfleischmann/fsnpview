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
#include <QApplication>
#include <QStringList>
#include <QLineEdit>

#include <algorithm>
#include <cmath>

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
    , m_lumpedParameterCount(0)
    , m_lastSelectionOrigin(SelectionOrigin::None)
{
    ui->setupUi(this);
    m_plot_manager = new PlotManager(ui->widgetGraph, this);

    ui->lineEditGateStart->installEventFilter(this);
    ui->lineEditGateStop->installEventFilter(this);

    Network::TimeGateSettings gateSettings;
    gateSettings.enabled = false;
    gateSettings.startDistance = 0.0;
    gateSettings.stopDistance = 1.0;
    gateSettings.epsilonR = 2.9;
    Network::setTimeGateSettings(gateSettings);
    refreshGateControls();
    ui->checkBoxGate->setChecked(gateSettings.enabled);

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
    m_network_files_model->setHorizontalHeaderLabels({"  ", "  ", "File"}); //Plot, Color, File
    connect(m_network_files_model, &QStandardItemModel::itemChanged, this, &MainWindow::onNetworkFilesModelChanged);

    m_network_lumped_model->setColumnCount(3);
    m_network_lumped_model->setHorizontalHeaderLabels({"  ", "  ", "Name"});
    connect(m_network_lumped_model, &QStandardItemModel::itemChanged, this, &MainWindow::onNetworkLumpedModelChanged);

    m_network_cascade_model->setColumnCount(3);
    m_network_cascade_model->setHorizontalHeaderLabels({"  ", "  ", "Name"});
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

void MainWindow::configureLumpedAndCascadeColumns(int parameterCount)
{
    m_lumpedParameterCount = parameterCount;

    QStringList headers = {"  ", "  ", "Name"};
    for (int i = 0; i < parameterCount; ++i) {
        headers << QStringLiteral("Par%1").arg(i + 1);
        headers << QStringLiteral("Val%1").arg(i + 1);
    }

    m_network_lumped_model->setColumnCount(headers.size());
    m_network_lumped_model->setHorizontalHeaderLabels(headers);

    m_network_cascade_model->setColumnCount(headers.size());
    m_network_cascade_model->setHorizontalHeaderLabels(headers);
}

void MainWindow::appendParameterItems(QList<QStandardItem*>& row, Network* network)
{
    auto lumped = dynamic_cast<NetworkLumped*>(network);
    for (int paramIndex = 0; paramIndex < m_lumpedParameterCount; ++paramIndex) {
        QStandardItem* descriptionItem = new QStandardItem();
        descriptionItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

        QStandardItem* valueItem = new QStandardItem();
        if (lumped && paramIndex < lumped->parameterCount()) {
            descriptionItem->setText(lumped->parameterDescription(paramIndex));
            valueItem->setText(Network::formatEngineering(lumped->parameterValue(paramIndex)));
            valueItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled);
        } else {
            valueItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        }

        row.append(descriptionItem);
        row.append(valueItem);
    }
}

bool MainWindow::isParameterValueColumn(int column) const
{
    if (column < 4)
        return false;

    int maxColumn = 3 + 2 * m_lumpedParameterCount;
    if (column >= maxColumn)
        return false;

    return column % 2 == 0;
}

int MainWindow::parameterIndexFromColumn(int column) const
{
    if (!isParameterValueColumn(column))
        return -1;
    return (column - 4) / 2;
}

void MainWindow::populateLumpedNetworkTable()
{
    m_networks.append(new NetworkLumped(NetworkLumped::NetworkType::R_series, {50.0}));
    m_networks.append(new NetworkLumped(NetworkLumped::NetworkType::R_shunt, {50.0}));
    m_networks.append(new NetworkLumped(NetworkLumped::NetworkType::C_series, {1.0}));
    m_networks.append(new NetworkLumped(NetworkLumped::NetworkType::C_shunt, {1.0}));
    m_networks.append(new NetworkLumped(NetworkLumped::NetworkType::L_series, {1.0, 1.0}));
    m_networks.append(new NetworkLumped(NetworkLumped::NetworkType::L_shunt, {1.0, 1.0}));
    m_networks.append(new NetworkLumped(NetworkLumped::NetworkType::TransmissionLine, {1e-3, 50.0}));

    m_lumpedParameterCount = 0;
    for (auto network_ptr : qAsConst(m_networks)) {
        if (auto lumped = dynamic_cast<NetworkLumped*>(network_ptr)) {
            m_lumpedParameterCount = std::max(m_lumpedParameterCount, lumped->parameterCount());
        }
    }

    configureLumpedAndCascadeColumns(m_lumpedParameterCount);

    m_network_lumped_model->removeRows(0, m_network_lumped_model->rowCount());

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
            QStandardItem* nameItem = new QStandardItem(network_ptr->displayName());
            nameItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            row.append(nameItem);
            appendParameterItems(row, network_ptr);
            m_network_lumped_model->appendRow(row);
        }
    }

    setupTableColumns(ui->tableViewNetworkLumped);
    setupTableColumns(ui->tableViewCascade);
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
        QStandardItem* nameItem = new QStandardItem(cloned->displayName());
        nameItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        items.append(nameItem);
        appendParameterItems(items, cloned);
        m_network_cascade_model->insertRow(row, items);
    }

    updatePlots();
}

void MainWindow::onGraphSelectionChanged(QCPAbstractPlottable *plottable,
                                        int, QMouseEvent *)
{
    m_lastSelectionOrigin = SelectionOrigin::Graph;

    if (!plottable)
        return;

    quintptr ptrVal = plottable->property("network_ptr").value<quintptr>();
    Network *network = reinterpret_cast<Network *>(ptrVal);
    if (!network || network == m_cascade)
        return;

    auto selectInView = [network](NetworkItemModel *model, QTableView *view) {
        if (!model || !view)
            return;

        if (QItemSelectionModel *selectionModel = view->selectionModel()) {
            QSignalBlocker blocker(selectionModel);
            selectionModel->clearSelection();
            for (int r = 0; r < model->rowCount(); ++r) {
                QStandardItem *item = model->item(r, 0);
                if (!item)
                    continue;
                quintptr val = item->data(Qt::UserRole).value<quintptr>();
                if (reinterpret_cast<Network *>(val) == network) {
                    QModelIndex idx = model->index(r, 0);
                    selectionModel->select(idx, QItemSelectionModel::Select | QItemSelectionModel::Rows);
                    view->scrollTo(idx);
                    break;
                }
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
    if (ui->checkBoxS31->isChecked()) checked_sparams << "s31";
    if (ui->checkBoxS12->isChecked()) checked_sparams << "s12";
    if (ui->checkBoxS22->isChecked()) checked_sparams << "s22";
    if (ui->checkBoxS32->isChecked()) checked_sparams << "s32";
    if (ui->checkBoxS13->isChecked()) checked_sparams << "s13";
    if (ui->checkBoxS23->isChecked()) checked_sparams << "s23";
    if (ui->checkBoxS33->isChecked()) checked_sparams << "s33";

    PlotType type = PlotType::Magnitude;
    if (ui->checkBoxTDR->isChecked())
        type = PlotType::TDR;
    else if (ui->checkBoxPhase->isChecked())
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
    } else if (isParameterValueColumn(item->column())) {
        int parameterIndex = parameterIndexFromColumn(item->column());
        if (parameterIndex < 0)
            return;
        quintptr net_ptr_val = m_network_lumped_model->item(item->row(), 0)->data(Qt::UserRole).value<quintptr>();
        NetworkLumped* network = dynamic_cast<NetworkLumped*>(reinterpret_cast<Network*>(net_ptr_val));
        if(network && parameterIndex < network->parameterCount()) {
            bool ok = false;
            double val = item->text().toDouble(&ok);
            if (ok) {
                network->setParameterValue(parameterIndex, val);
                m_network_lumped_model->item(item->row(), 2)->setText(network->displayName());
                updatePlots();
            }
            {
                QSignalBlocker blocker(m_network_lumped_model);
                item->setText(Network::formatEngineering(network->parameterValue(parameterIndex)));
            }
        } else {
            QSignalBlocker blocker(m_network_lumped_model);
            item->setText(QString());
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
    } else if (isParameterValueColumn(item->column())) {
        int parameterIndex = parameterIndexFromColumn(item->column());
        if (parameterIndex < 0)
            return;
        quintptr net_ptr_val = m_network_cascade_model->item(item->row(), 0)->data(Qt::UserRole).value<quintptr>();
        Network* network_base = reinterpret_cast<Network*>(net_ptr_val);
        if(auto network = dynamic_cast<NetworkLumped*>(network_base)) {
            if (parameterIndex >= network->parameterCount()) {
                QSignalBlocker blocker(m_network_cascade_model);
                item->setText(QString());
                return;
            }
            bool ok = false;
            double val = item->text().toDouble(&ok);
            if (ok) {
                network->setParameterValue(parameterIndex, val);
                m_network_cascade_model->item(item->row(), 2)->setText(network->displayName());
                updatePlots();
            }
            {
                QSignalBlocker blocker(m_network_cascade_model);
                item->setText(Network::formatEngineering(network->parameterValue(parameterIndex)));
            }
        } else {
            QSignalBlocker blocker(m_network_cascade_model);
            item->setText(QString());
        }
    }
}

void MainWindow::onGraphSelectionChangedByUser()
{
    m_lastSelectionOrigin = SelectionOrigin::Graph;

    QSet<Network*> selectedNetworks;
    bool cascadeSelected = false;
    const auto plottables = ui->widgetGraph->selectedPlottables();
    for (QCPAbstractPlottable *plottable : plottables) {
        if (!plottable)
            continue;

        quintptr ptrVal = plottable->property("network_ptr").value<quintptr>();
        Network* network = reinterpret_cast<Network*>(ptrVal);
        if (!network)
            continue;
        if (network == m_cascade) {
            cascadeSelected = true;
            continue;
        }
        selectedNetworks.insert(network);
    }

    auto syncSelection = [&](QTableView *view, NetworkItemModel *model) {
        if (!view || !model)
            return;

        if (QItemSelectionModel *selModel = view->selectionModel()) {
            QSignalBlocker blocker(selModel);
            selModel->clearSelection();
            for (int row = 0; row < model->rowCount(); ++row) {
                QStandardItem *item = model->item(row, 0);
                if (!item)
                    continue;
                quintptr ptrVal = item->data(Qt::UserRole).value<quintptr>();
                Network *network = reinterpret_cast<Network*>(ptrVal);
                if (selectedNetworks.contains(network)) {
                    selModel->select(model->index(row, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
                }
            }
        }
    };

    syncSelection(ui->tableViewNetworkFiles, m_network_files_model);
    syncSelection(ui->tableViewNetworkLumped, m_network_lumped_model);
    syncSelection(ui->tableViewCascade, m_network_cascade_model);

    if (cascadeSelected) {
        if (QItemSelectionModel *selModel = ui->tableViewCascade->selectionModel()) {
            QSignalBlocker blocker(selModel);
            selModel->clearSelection();
            for (int row = 0; row < m_network_cascade_model->rowCount(); ++row) {
                selModel->select(m_network_cascade_model->index(row, 0),
                                 QItemSelectionModel::Select | QItemSelectionModel::Rows);
            }
        }
    }
}

void MainWindow::onNetworkFilesSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    Q_UNUSED(selected);
    Q_UNUSED(deselected);
    QWidget *focusWidget = QApplication::focusWidget();
    if ((focusWidget && (focusWidget == ui->tableViewNetworkFiles || ui->tableViewNetworkFiles->isAncestorOf(focusWidget))) ||
        ui->tableViewNetworkFiles->hasFocus() || ui->tableViewNetworkFiles->viewport()->hasFocus()) {
        m_lastSelectionOrigin = SelectionOrigin::Files;
    }
    updateGraphSelectionFromTables();
}

void MainWindow::onNetworkLumpedSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    Q_UNUSED(selected);
    Q_UNUSED(deselected);
    QWidget *focusWidget = QApplication::focusWidget();
    if ((focusWidget && (focusWidget == ui->tableViewNetworkLumped || ui->tableViewNetworkLumped->isAncestorOf(focusWidget))) ||
        ui->tableViewNetworkLumped->hasFocus() || ui->tableViewNetworkLumped->viewport()->hasFocus()) {
        m_lastSelectionOrigin = SelectionOrigin::Lumped;
    }
    updateGraphSelectionFromTables();
}

void MainWindow::onNetworkCascadeSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    Q_UNUSED(selected);
    Q_UNUSED(deselected);
    QWidget *focusWidget = QApplication::focusWidget();
    if ((focusWidget && (focusWidget == ui->tableViewCascade || ui->tableViewCascade->isAncestorOf(focusWidget))) ||
        ui->tableViewCascade->hasFocus() || ui->tableViewCascade->viewport()->hasFocus()) {
        m_lastSelectionOrigin = SelectionOrigin::Cascade;
    }
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

bool MainWindow::nearlyEqual(double lhs, double rhs)
{
    double scale = std::max({1.0, std::abs(lhs), std::abs(rhs)});
    return std::abs(lhs - rhs) <= scale * 1e-12;
}

bool MainWindow::applyGateSettingsFromUi()
{
    Network::TimeGateSettings previous = Network::timeGateSettings();
    Network::TimeGateSettings settings = previous;
    settings.enabled = ui->checkBoxGate->isChecked();

    auto parseValue = [](QLineEdit *edit, double fallback) {
        bool ok = false;
        double value = edit->text().trimmed().toDouble(&ok);
        return ok ? value : fallback;
    };

    double start = parseValue(ui->lineEditGateStart, previous.startDistance);
    double stop = parseValue(ui->lineEditGateStop, previous.stopDistance);
    double epsilon = parseValue(ui->lineEditEpsilonR, previous.epsilonR);

    if (!(start >= 0.0))
        start = 0.0;
    if (!(stop >= 0.0))
        stop = 0.0;
    if (stop < start)
        stop = start;
    if (!(epsilon > 0.0))
        epsilon = previous.epsilonR;
    epsilon = std::max(1.0, epsilon);

    settings.startDistance = start;
    settings.stopDistance = stop;
    settings.epsilonR = epsilon;

    bool changed = settings.enabled != previous.enabled
            || !nearlyEqual(settings.startDistance, previous.startDistance)
            || !nearlyEqual(settings.stopDistance, previous.stopDistance)
            || !nearlyEqual(settings.epsilonR, previous.epsilonR);

    if (changed)
        Network::setTimeGateSettings(settings);

    return changed;
}

void MainWindow::refreshGateControls()
{
    Network::TimeGateSettings settings = Network::timeGateSettings();

    {
        QSignalBlocker blocker(ui->lineEditGateStart);
        ui->lineEditGateStart->setText(Network::formatEngineering(settings.startDistance, false));
    }
    {
        QSignalBlocker blocker(ui->lineEditGateStop);
        ui->lineEditGateStop->setText(Network::formatEngineering(settings.stopDistance, false));
    }
    {
        QSignalBlocker blocker(ui->lineEditEpsilonR);
        ui->lineEditEpsilonR->setText(QString::number(settings.epsilonR, 'g', 6));
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
        m_plot_manager->setXAxisScaleType(QCPAxis::stLogarithmic);
    } else {
        m_plot_manager->setXAxisScaleType(QCPAxis::stLinear);
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
        QWidget *focusWidget = QApplication::focusWidget();
        auto viewHasFocus = [focusWidget](QTableView *view) {
            if (!view)
                return false;
            if (focusWidget) {
                if (focusWidget == view)
                    return true;
                if (view->isAncestorOf(focusWidget))
                    return true;
            }
            return view->hasFocus() || view->viewport()->hasFocus();
        };

        bool graphContext = (focusWidget == ui->widgetGraph || (ui->widgetGraph && ui->widgetGraph->isAncestorOf(focusWidget)) ||
                             m_lastSelectionOrigin == SelectionOrigin::Graph);
        bool filesContext = viewHasFocus(ui->tableViewNetworkFiles) || m_lastSelectionOrigin == SelectionOrigin::Files;
        bool lumpedContext = viewHasFocus(ui->tableViewNetworkLumped) || m_lastSelectionOrigin == SelectionOrigin::Lumped;
        bool cascadeContext = viewHasFocus(ui->tableViewCascade) || m_lastSelectionOrigin == SelectionOrigin::Cascade;

        bool plotsNeedUpdate = false;
        bool networksChanged = false;

        auto uncheckInModel = [](NetworkItemModel *model, Network *network) {
            if (!model || !network)
                return;
            for (int r = 0; r < model->rowCount(); ++r) {
                QStandardItem *item = model->item(r, 0);
                if (!item)
                    continue;
                quintptr ptrVal = item->data(Qt::UserRole).value<quintptr>();
                if (reinterpret_cast<Network *>(ptrVal) == network) {
                    if (item->checkState() != Qt::Unchecked) {
                        item->setCheckState(Qt::Unchecked);
                    }
                    break;
                }
            }
        };

        if (graphContext) {
            m_plot_manager->removeSelectedMathPlots();
            const auto plottables = ui->widgetGraph->selectedPlottables();
            QSet<Network*> networksToHide;
            bool cascadeGraphSelected = false;
            for (QCPAbstractPlottable *plottable : plottables) {
                if (!plottable)
                    continue;
                quintptr ptrVal = plottable->property("network_ptr").value<quintptr>();
                Network *network = reinterpret_cast<Network*>(ptrVal);
                if (!network)
                    continue;
                if (network == m_cascade) {
                    cascadeGraphSelected = true;
                    continue;
                }
                networksToHide.insert(network);
            }

            for (Network *network : qAsConst(networksToHide)) {
                if (!network)
                    continue;
                network->setVisible(false);
                uncheckInModel(m_network_files_model, network);
                uncheckInModel(m_network_lumped_model, network);
                plotsNeedUpdate = true;
            }

            if (cascadeGraphSelected && m_cascade) {
                const QList<Network*> cascadeNetworks = m_cascade->getNetworks();
                for (Network *network : cascadeNetworks) {
                    if (!network)
                        continue;
                    network->setActive(false);
                    uncheckInModel(m_network_cascade_model, network);
                }
                plotsNeedUpdate = true;
            }
        }

        if (filesContext && ui->tableViewNetworkFiles->selectionModel()) {
            QModelIndexList selectedRows = ui->tableViewNetworkFiles->selectionModel()->selectedRows();
            if (!selectedRows.isEmpty()) {
                std::sort(selectedRows.begin(), selectedRows.end(),
                          [](const QModelIndex &a, const QModelIndex &b) { return a.row() > b.row(); });

                for (const QModelIndex &index : selectedRows) {
                    QStandardItem *item = m_network_files_model->item(index.row(), 0);
                    if (!item)
                        continue;
                    quintptr ptrVal = item->data(Qt::UserRole).value<quintptr>();
                    Network *network = reinterpret_cast<Network*>(ptrVal);
                    if (!network)
                        continue;

                    m_network_files_model->removeRow(index.row());
                    m_networks.removeOne(network);
                    delete network;
                    networksChanged = true;
                }
            }
        }

        if (lumpedContext && ui->tableViewNetworkLumped->selectionModel()) {
            QModelIndexList selectedRows = ui->tableViewNetworkLumped->selectionModel()->selectedRows();
            for (const QModelIndex &index : selectedRows) {
                QStandardItem *item = m_network_lumped_model->item(index.row(), 0);
                if (!item)
                    continue;
                quintptr ptrVal = item->data(Qt::UserRole).value<quintptr>();
                Network *network = reinterpret_cast<Network*>(ptrVal);
                if (!network)
                    continue;

                network->setVisible(false);
                if (item->checkState() != Qt::Unchecked) {
                    item->setCheckState(Qt::Unchecked);
                }
                plotsNeedUpdate = true;
            }
        }

        if (cascadeContext && ui->tableViewCascade->selectionModel()) {
            QModelIndexList selectedRows = ui->tableViewCascade->selectionModel()->selectedRows();
            if (!selectedRows.isEmpty()) {
                std::sort(selectedRows.begin(), selectedRows.end(),
                          [](const QModelIndex &a, const QModelIndex &b) { return a.row() > b.row(); });

                for (const QModelIndex &index : selectedRows) {
                    QStandardItem *item = m_network_cascade_model->item(index.row(), 0);
                    if (!item)
                        continue;
                    quintptr ptrVal = item->data(Qt::UserRole).value<quintptr>();
                    Network *network = reinterpret_cast<Network*>(ptrVal);
                    if (!network)
                        continue;

                    int cascadeIndex = m_cascade->getNetworks().indexOf(network);
                    if (cascadeIndex >= 0) {
                        m_cascade->removeNetwork(cascadeIndex);
                    }
                    if (network->parent() == m_cascade) {
                        delete network;
                    }
                    m_network_cascade_model->removeRow(index.row());
                    plotsNeedUpdate = true;
                }
            }
        }

        if (networksChanged) {
            m_plot_manager->setNetworks(m_networks);
            plotsNeedUpdate = true;
        }

        if (plotsNeedUpdate) {
            updatePlots();
        }

        event->accept();
        return;
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

void MainWindow::on_checkBoxS31_checkStateChanged(const Qt::CheckState &arg1)
{
    Q_UNUSED(arg1);
    updatePlots();
}

void MainWindow::on_checkBoxS32_checkStateChanged(const Qt::CheckState &arg1)
{
    Q_UNUSED(arg1);
    updatePlots();
}

void MainWindow::on_checkBoxS13_checkStateChanged(const Qt::CheckState &arg1)
{
    Q_UNUSED(arg1);
    updatePlots();
}

void MainWindow::on_checkBoxS23_checkStateChanged(const Qt::CheckState &arg1)
{
    Q_UNUSED(arg1);
    updatePlots();
}

void MainWindow::on_checkBoxS33_checkStateChanged(const Qt::CheckState &arg1)
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
        ui->checkBoxTDR->setChecked(false);
    }
    updatePlots();
}

void MainWindow::on_checkBoxVSWR_checkStateChanged(const Qt::CheckState &arg1)
{
    Q_UNUSED(arg1);
    if (ui->checkBoxVSWR->isChecked()) {
        ui->checkBoxPhase->setChecked(false);
        ui->checkBoxSmith->setChecked(false);
        ui->checkBoxTDR->setChecked(false);
    }
    updatePlots();
}

void MainWindow::on_checkBoxSmith_checkStateChanged(const Qt::CheckState &arg1)
{
    Q_UNUSED(arg1);
    if (ui->checkBoxSmith->isChecked()) {
        ui->checkBoxPhase->setChecked(false);
        ui->checkBoxVSWR->setChecked(false);
        ui->checkBoxTDR->setChecked(false);
    }
    updatePlots();
}

void MainWindow::on_checkBoxTDR_checkStateChanged(const Qt::CheckState &arg1)
{
    Q_UNUSED(arg1);
    if (ui->checkBoxTDR->isChecked()) {
        ui->checkBoxPhase->setChecked(false);
        ui->checkBoxVSWR->setChecked(false);
        ui->checkBoxSmith->setChecked(false);
        if (ui->checkBox->isChecked()) {
            ui->checkBox->setChecked(false);
        }
    }
    updatePlots();
}

void MainWindow::on_checkBoxGate_stateChanged(int state)
{
    Q_UNUSED(state);
    bool changed = applyGateSettingsFromUi();
    refreshGateControls();
    if (changed)
        updatePlots();
}

void MainWindow::on_lineEditGateStart_editingFinished()
{
    bool changed = applyGateSettingsFromUi();
    refreshGateControls();
    if (changed)
        updatePlots();
}

void MainWindow::on_lineEditGateStop_editingFinished()
{
    bool changed = applyGateSettingsFromUi();
    refreshGateControls();
    if (changed)
        updatePlots();
}

void MainWindow::on_lineEditEpsilonR_editingFinished()
{
    bool changed = applyGateSettingsFromUi();
    refreshGateControls();
    if (changed)
        updatePlots();
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::Wheel) {
        if (obj == ui->lineEditGateStart || obj == ui->lineEditGateStop) {
            QWheelEvent *wheelEvent = static_cast<QWheelEvent*>(event);
            if (wheelEvent->angleDelta().y() == 0)
                return true;

            QLineEdit *edit = qobject_cast<QLineEdit*>(obj);
            if (!edit)
                return QMainWindow::eventFilter(obj, event);

            Network::TimeGateSettings settings = Network::timeGateSettings();
            const double fallback = (obj == ui->lineEditGateStop)
                    ? settings.stopDistance
                    : settings.startDistance;

            bool ok = false;
            double value = edit->text().trimmed().toDouble(&ok);
            if (!ok)
                value = fallback;

            const double magnitude = std::max(std::abs(value), 1e-3);
            const double step = magnitude * 0.1;
            if (wheelEvent->angleDelta().y() > 0)
                value += step;
            else if (wheelEvent->angleDelta().y() < 0) {
                value -= step;
                if (value < 0.0)
                    value = 0.0;
            }

            {
                QSignalBlocker blocker(edit);
                edit->setText(Network::formatEngineering(value, false));
            }

            bool changed = applyGateSettingsFromUi();
            refreshGateControls();
            if (changed)
                updatePlots();
            return true;
        }

        auto handleValueWheel = [&](QTableView *view, NetworkItemModel *model) -> bool {
            QWheelEvent *wheelEvent = static_cast<QWheelEvent*>(event);
            QModelIndex index = view->indexAt(wheelEvent->position().toPoint());
            if (!index.isValid() || !isParameterValueColumn(index.column()))
                return false;

            int parameterIndex = parameterIndexFromColumn(index.column());
            if (parameterIndex < 0)
                return false;

            QStandardItem *ptrItem = model->item(index.row(), 0);
            QStandardItem *valueItem = model->item(index.row(), index.column());
            if (!ptrItem || !valueItem)
                return false;

            quintptr ptrVal = ptrItem->data(Qt::UserRole).value<quintptr>();
            Network *network_base = reinterpret_cast<Network*>(ptrVal);
            auto network = dynamic_cast<NetworkLumped*>(network_base);
            if (!network || parameterIndex >= network->parameterCount())
                return false;

            bool ok = false;
            double val = valueItem->text().toDouble(&ok);
            if (ok) {
                if (wheelEvent->angleDelta().y() > 0)
                    val *= 1.1;
                else if (wheelEvent->angleDelta().y() < 0)
                    val *=  1 / 1.1;
                valueItem->setText(Network::formatEngineering(val));
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
