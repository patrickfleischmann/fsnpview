#include "mainwindow.h"
#include <QApplication>
#include <QLocalSocket>
#include <QDataStream>
#include <iostream>

int main(int argc, char *argv[])
{
    const QString serverName = "fsnpview-server";
    QLocalSocket socket;
    socket.connectToServer(serverName);

    if (socket.waitForConnected(500)) {
        std::cout << "Another instance is running. Sending arguments to it." << std::endl;
        QDataStream stream(&socket);
        QStringList args;
        for (int i = 1; i < argc; ++i) {
            args.append(QString::fromLocal8Bit(argv[i]));
        }
        stream << args;
        socket.waitForBytesWritten(1000);
        return 0;
    }

    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    QStringList args = a.arguments();
    args.removeFirst(); // remove the program name
    w.processFiles(args);

    return a.exec();
}
