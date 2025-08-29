#include "mainwindow.h"
#include <QApplication>
#include <QLocalSocket>
#include <QDataStream>
#include <iostream>
#include <QtGlobal>
#ifdef Q_OS_WIN
#include <windows.h>
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
        std::cout << "Another instance of the program is already running." << std::endl;
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
        return 0; // Exit if another instance is running
    }
#endif

    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    QStringList args = a.arguments();
    args.removeFirst(); // remove the program name
    w.processFiles(args);

    int ret = a.exec();

#ifdef Q_OS_WIN
    if(hMutex != nullptr){
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
    }
#endif

    return ret;
}
