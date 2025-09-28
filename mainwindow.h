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
class QTableWidget;
class QCPAbstractPlottable;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void processFiles(const QStringList &files, bool autoscale = false);
    void clearCascade();
    void addNetworkToCascade(Network* network);
    void setCascadeFrequencyRange(double fmin, double fmax);
    void setCascadePointCount(int pointCount);
    void initializeFrequencyControls(bool freqSpecified, double fmin, double fmax, int pointCount, bool hasInitialFiles);
    NetworkCascade* cascade() const;

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
    void on_checkBoxS31_checkStateChanged(const Qt::CheckState &arg1);
    void on_checkBoxS32_checkStateChanged(const Qt::CheckState &arg1);
    void on_checkBoxS13_checkStateChanged(const Qt::CheckState &arg1);
    void on_checkBoxS23_checkStateChanged(const Qt::CheckState &arg1);
    void on_checkBoxS33_checkStateChanged(const Qt::CheckState &arg1);

    void on_checkBoxPhase_checkStateChanged(const Qt::CheckState &arg1);
    void on_checkBoxPhaseUnwrap_stateChanged(int state);
    void on_checkBoxVSWR_checkStateChanged(const Qt::CheckState &arg1);
    void on_checkBoxSmith_checkStateChanged(const Qt::CheckState &arg1);
    void on_checkBoxTDR_checkStateChanged(const Qt::CheckState &arg1);

    void on_checkBoxGate_stateChanged(int state);
    void on_lineEditGateStart_editingFinished();
    void on_lineEditGateStop_editingFinished();
    void on_lineEditEpsilonR_editingFinished();
    void on_lineEditFminNetworks_editingFinished();
    void on_lineEditFmaxNetworks_editingFinished();
    void on_lineEditNpointsNetworks_editingFinished();

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
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void updatePlots();
    void setupModels();
    void setupViews();
    void setupTableColumns(QTableWidget* view);
    void populateLumpedNetworkTable();
    void configureLumpedAndCascadeColumns(int parameterCount);
    void appendParameterItems(QList<QStandardItem*>& row, Network* network);
    bool isParameterValueColumn(int column) const;
    int parameterIndexFromColumn(int column) const;
    void updateGraphSelectionFromTables();
    bool applyGateSettingsFromUi();
    void refreshGateControls();
    bool applyNetworkFrequencySettingsFromUi();
    void refreshNetworkFrequencyControls();
    void updateNetworkFrequencySettings(double fmin, double fmax, int pointCount, bool manualOverride = true);
    static bool nearlyEqual(double lhs, double rhs);
    void applyPhaseUnwrapSetting(bool unwrap);


    Ui::MainWindow *ui;
    Server *m_server;
    PlotManager* m_plot_manager;

    QList<Network*> m_networks;
    NetworkCascade* m_cascade;
    NetworkItemModel* m_network_files_model;
    NetworkItemModel* m_network_lumped_model;
    NetworkItemModel* m_network_cascade_model;

    int m_lumpedParameterCount;

    enum class SelectionOrigin
    {
        None,
        Graph,
        Files,
        Lumped,
        Cascade
    };

    SelectionOrigin m_lastSelectionOrigin;
    double m_networkFrequencyMin;
    double m_networkFrequencyMax;
    int m_networkFrequencyPoints;
    bool m_initialFrequencyConfigured;
};
#endif // MAINWINDOW_H
