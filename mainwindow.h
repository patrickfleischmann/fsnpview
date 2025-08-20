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

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class QCPItemLine;
class QCPItemText;
class QCPGraph;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void plot(const QVector<double> &x, const QVector<double> &y, const QColor &color);
    void processFiles(const QStringList &files);

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

    void updateCursor(QCPGraph *graph, QCPItemLine *ver_line, QCPItemLine *hor_line, QCPItemText *text, double x);

    // Cursor A
    QCPItemLine *m_cursorA_ver_line;
    QCPItemLine *m_cursorA_hor_line;
    QCPItemText *m_cursorA_text;
    QCPGraph *m_cursorA_graph;
    bool m_cursorA_dragging;

    // Cursor B
    QCPItemLine *m_cursorB_ver_line;
    QCPItemLine *m_cursorB_hor_line;
    QCPItemText *m_cursorB_text;
    QCPGraph *m_cursorB_graph;
    bool m_cursorB_dragging;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
};
#endif // MAINWINDOW_H
