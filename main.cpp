#include "mainwindow.h"
#include <QApplication>
#include <QLocalSocket>
#include <QDataStream>
#include <iostream>
#include <QtGlobal>
#include <chrono>
#include <thread>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

using namespace std;

int main(int argc, char *argv[])
{
    std::cout << "fsnpview start" << std::endl;

    bool isSecondInstance = true;

#ifdef Q_OS_WIN
    HANDLE hMutex = CreateMutexA(NULL, TRUE, "MutexFsnpview");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        std::cout << "Another instance of the program is already running." << std::endl;
    } else {
        isSecondInstance = false;
        std::cout << "This is the first instance" << std::endl;
    }
#else
    // On non-Windows systems, we just assume it's the first instance for now.
    // A more robust solution would use a different IPC mechanism, like QSharedMemory or lock files.
    isSecondInstance = false;
#endif

    if(isSecondInstance){
        const QString serverName = "fsnpview-server";
        QLocalSocket socket;
        this_thread::sleep_for(chrono::milliseconds(100)); //debugging..
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
        return 1;
    }

    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    QStringList args = a.arguments();
    args.removeFirst(); // remove the program name
    w.processFiles(args);

    a.exec();

#ifdef Q_OS_WIN
    ReleaseMutex(hMutex);
    CloseHandle(hMutex);
#endif

    return 0;
}
