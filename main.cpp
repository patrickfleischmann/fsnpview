#include "mainwindow.h"
#include <QApplication>
#include <QLocalSocket>
#include <QDataStream>
#include <iostream>
#include <QtGlobal>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
#include <chrono>
#include <thread>

using namespace std;

#ifdef Q_OS_WIN
#define OSWIN true
#else
#define OSWIN false
#endif

int main(int argc, char *argv[])
{
    std::cout << "fsnpview start" << std::endl;

#ifdef Q_OS_WIN
    HANDLE hMutex = CreateMutexA(NULL, TRUE, "MutexFsnpview");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        const QString serverName = "fsnpview-server";
        QLocalSocket socket;
        socket.connectToServer(serverName);

        if (socket.waitForConnected(500)) {
            std::cout << "Sending arguments to first instance" << std::endl;
            QDataStream stream(&socket);
            QStringList args;
            for (int i = 1; i < argc; ++i) {
                args.append(QString::fromLocal8Bit(argv[i]));
            }
            stream << args;
            if (socket.waitForBytesWritten()) {
                socket.flush();
                socket.waitForDisconnected();
            }
        }
        return 0;
    }
#else
    const QString serverName = "fsnpview-server";
    QLocalSocket socket;
    socket.connectToServer(serverName);

    if (socket.waitForConnected(500)) {
        std::cout << "Sending arguments to first instance" << std::endl;
        QDataStream stream(&socket);
        QStringList args;
        for (int i = 1; i < argc; ++i) {
            args.append(QString::fromLocal8Bit(argv[i]));
        }
        stream << args;
        if (socket.waitForBytesWritten()) {
            socket.flush();
            socket.waitForDisconnected();
        }
        return 0;
    }
#endif

    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    QStringList args = a.arguments();
    args.removeFirst(); // remove the program name
    w.processFiles(args);

    int result = a.exec();

#ifdef Q_OS_WIN
    ReleaseMutex(hMutex);
    CloseHandle(hMutex);
#endif

    return result;
}
