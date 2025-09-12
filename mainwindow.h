#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVector>
#include <QColor>
#include <QStringList>
#include <QMouseEvent>
#include <QKeyEvent>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

#include "network.h"
#include "networkcascade.h"
#include "networkitemmodel.h"
#include "unitdelegate.h"
#include <memory>

class Server;
class NetworkFile;
class PlotManager;

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
    void onLumpedNetworkDropped(Network* network, const QModelIndex& parent);
    void onCascadeNetworkDropped(Network* network, const QModelIndex& parent);


protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    void updatePlots();
    void setupModels();
    void setupViews();
    void populateLumpedNetworkTable();
    void addNetworkToCascade(Network* network, int row);
    void removeNetworkFromCascade(Network* network);
    void addItemsToCascadeModel(Network* network, int row);


    Ui::MainWindow *ui;
    Server *m_server;
    PlotManager* m_plot_manager;
    UnitDelegate* m_unit_delegate;

    QList<Network*> m_networks;
    NetworkCascade* m_cascade;
    NetworkItemModel* m_network_files_model;
    NetworkItemModel* m_network_lumped_model;
    NetworkItemModel* m_network_cascade_model;
};
#endif // MAINWINDOW_H
