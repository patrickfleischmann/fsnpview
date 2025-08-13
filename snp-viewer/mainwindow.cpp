#include "mainwindow.h"

#include <QMenuBar>
#include <QToolBar>
#include <QFileDialog>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    QChart *chart = new QChart();
    m_chartView = new QChartView(chart);
    setCentralWidget(m_chartView);

    QAction *openAction = new QAction("&Open", this);
    connect(openAction, &QAction::triggered, this, &MainWindow::openFile);

    QMenu *fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction(openAction);

    QToolBar *toolBar = addToolBar("main");
    toolBar->addAction(openAction);
}

MainWindow::~MainWindow()
{
}

void MainWindow::openFile()
{
    QString filePath = QFileDialog::getOpenFileName(this, "Open SnP File", "", "Touchstone files (*.s*p)");
    if (!filePath.isEmpty()) {
        QMessageBox::information(this, "File Opened", "File path: " + filePath);
    }
}
