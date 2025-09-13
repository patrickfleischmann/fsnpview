#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVector>
#include <QColor>
#include <QStringList>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QItemSelection>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

#include "network.h"
#include "networkcascade.h"
#include "networkitemmodel.h"
#include <memory>

class Server;
class NetworkFile;
class PlotManager;
class QTableView;
class QCPAbstractPlottable;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void processFiles(const QStringList &files, bool autoscale = false);

private slots:
    void on_actionOpen_triggered();
    void on_pushButtonAutoscale_clicked();
    void onFilesReceived(const QStringList &files);

    void on_checkBoxLegend_checkStateChanged(const Qt::CheckState &arg1);

    void on_checkBox_checkStateChanged(const Qt::CheckState &arg1);

    void on_checkBoxS11_checkStateChanged(const Qt::CheckState &arg1);
    void on_checkBoxS21_checkStateChanged(const Qt::CheckState &arg1);
    void on_checkBoxS12_checkStateChanged(const Qt::CheckState &arg1);
    void on_checkBoxS22_checkStateChanged(const Qt::CheckState &arg1);

    void on_checkBoxPhase_checkStateChanged(const Qt::CheckState &arg1);

    void onNetworkFilesModelChanged(QStandardItem *item);
    void onNetworkLumpedModelChanged(QStandardItem *item);
    void onNetworkCascadeModelChanged(QStandardItem *item);

    void onNetworkDropped(Network* network, int row, const QModelIndex& parent);
    void onGraphSelectionChanged(QCPAbstractPlottable *plottable, int dataIndex, QMouseEvent *event);
    void onGraphSelectionChangedByUser();
    void onNetworkFilesSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
    void onNetworkLumpedSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
    void onNetworkCascadeSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
    void onColorColumnClicked(const QModelIndex &index);

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    void updatePlots();
    void setupModels();
    void setupViews();
    void setupTableColumns(QTableView* view);
    void populateLumpedNetworkTable();
    void updateGraphSelectionFromTables();


    Ui::MainWindow *ui;
    Server *m_server;
    PlotManager* m_plot_manager;

    QList<Network*> m_networks;
    NetworkCascade* m_cascade;
    NetworkItemModel* m_network_files_model;
    NetworkItemModel* m_network_lumped_model;
    NetworkItemModel* m_network_cascade_model;
};
#endif // MAINWINDOW_H
