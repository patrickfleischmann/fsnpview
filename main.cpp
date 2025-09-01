#include "mainwindow.h"
#include <QApplication>
#include <QLocalSocket>
#include <QDataStream>
#include <iostream>
#include <QtSystemDetection>
#include <QtGlobal>
#include <windows.h>
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

    bool isSecondInstance = true; //assume true to work on non-windows systems

    HANDLE hMutex;
    if (OSWIN){
        //Using mutex should safer and may be faster for detecting other instances on windows
        //using only the socket-server based method didnt entirely prevent multiple instances
        hMutex = CreateMutexA(NULL, TRUE, "MutexFsnpview");
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            std::cout << "Another instance of the program is already running." << std::endl;
        } else {
            isSecondInstance = false;
            std::cout << "This is the first instance" << std::endl;
        }
    }

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

    ReleaseMutex(hMutex);
    CloseHandle(hMutex);

    return 0;
}
