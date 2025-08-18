#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    QStringList args = a.arguments();
    for (int i = 1; i < args.size(); ++i) {
        w.plotFile(args.at(i));
    }

    return a.exec();
}
