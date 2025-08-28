#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "parser_touchstone.h"
#include <QMainWindow>
#include <QVector>
#include <QColor>
#include <QStringList>
#include <map>
#include <string>
#include <memory>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMouseEvent>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class QCPItemTracer;
class QCPItemText;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void plot(const QVector<double> &x, const QVector<double> &y, const QColor &color, const QString &name);
    void processFiles(const QStringList &files);

public slots:
    void mouseDoubleClick(QMouseEvent *event);
    void mousePress(QMouseEvent *event);
    void mouseMove(QMouseEvent *event);
    void mouseRelease(QMouseEvent *event);

private slots:
    void on_pushButtonAutoscale_clicked();
    void newConnection();
    void readyRead();

    void on_checkBoxCursorA_stateChanged(int arg1);

    void on_checkBoxCursorB_stateChanged(int arg1);

    void on_checkBoxLegend_checkStateChanged(const Qt::CheckState &arg1);

    void on_checkBox_checkStateChanged(const Qt::CheckState &arg1);

    void on_checkBoxS11_checkStateChanged(const Qt::CheckState &arg1);

    void on_checkBoxS21_checkStateChanged(const Qt::CheckState &arg1);

    void on_checkBoxS12_checkStateChanged(const Qt::CheckState &arg1);

    void on_checkBoxS22_checkStateChanged(const Qt::CheckState &arg1);

private:
    void updateSparamPlot(const QString &paramName, int s_param_idx, const QColor &color, const Qt::CheckState &checkState);
    enum class DragMode { None, Vertical, Horizontal };
    void updateTracerText(QCPItemTracer *tracer, QCPItemText *text);
    void updateTracers();
    Ui::MainWindow *ui;
    std::map<std::string, std::unique_ptr<ts::TouchstoneData>> parsed_data;
    QLocalServer *localServer;
    QCPItemTracer *mTracerA;
    QCPItemText *mTracerTextA;
    QCPItemTracer *mTracerB;
    QCPItemText *mTracerTextB;
    QCPItemTracer *mDraggedTracer;
    DragMode mDragMode;
};
#endif // MAINWINDOW_H
