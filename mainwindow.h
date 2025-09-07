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

class QCPItemTracer;
class QCPItemText;
class Server;
class Networks;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void plot(const QVector<double> &x, const QVector<double> &y, const QColor &color, const QString &name, const QString &filePath, const QString &yAxisLabel, Qt::PenStyle style = Qt::SolidLine);
    void processFiles(const QStringList &files);

public slots:
    void mouseDoubleClick(QMouseEvent *event);
    void mousePress(QMouseEvent *event);
    void mouseMove(QMouseEvent *event);
    void mouseRelease(QMouseEvent *event);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void on_pushButtonAutoscale_clicked();
    void onFilesReceived(const QStringList &files);

    void on_checkBoxCursorA_stateChanged(int arg1);

    void on_checkBoxCursorB_stateChanged(int arg1);

    void on_checkBoxLegend_checkStateChanged(const Qt::CheckState &arg1);

    void on_checkBox_checkStateChanged(const Qt::CheckState &arg1);

    void on_checkBoxS11_checkStateChanged(const Qt::CheckState &arg1);

    void on_checkBoxS21_checkStateChanged(const Qt::CheckState &arg1);

    void on_checkBoxS12_checkStateChanged(const Qt::CheckState &arg1);

    void on_checkBoxS22_checkStateChanged(const Qt::CheckState &arg1);

    void on_checkBoxPhase_checkStateChanged(const Qt::CheckState &arg1);

private:
    void updateSparamPlot(const QString &paramName, const Qt::CheckState &checkState);
    enum class DragMode { None, Vertical, Horizontal };
    void updateTracerText(QCPItemTracer *tracer, QCPItemText *text);
    void updateTracers();
    Ui::MainWindow *ui;
    Server *m_server;
    Networks *m_networks;
    QCPItemTracer *mTracerA;
    QCPItemText *mTracerTextA;
    QCPItemTracer *mTracerB;
    QCPItemText *mTracerTextB;
    QCPItemTracer *mDraggedTracer;
    DragMode mDragMode;
    bool mLegendDrag;
    QPointF mLegendDragStart;
};
#endif // MAINWINDOW_H
