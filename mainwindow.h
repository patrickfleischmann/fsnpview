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

private slots:
    void on_pushButtonAutoscale_clicked();
    void newConnection();
    void readyRead();

    void on_checkBoxCursorA_checkStateChanged(const Qt::CheckState &arg1);

    void on_checkBoxCursorB_checkStateChanged(const Qt::CheckState &arg1);

    void on_checkBoxLegend_checkStateChanged(const Qt::CheckState &arg1);

private:
    Ui::MainWindow *ui;
    std::map<std::string, std::unique_ptr<ts::TouchstoneData>> parsed_data;
    QLocalServer *localServer;
};
#endif // MAINWINDOW_H
