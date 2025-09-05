#include "server.h"
#include <QLocalServer>
#include <QLocalSocket>
#include <QDataStream>
#include <iostream>

Server::Server(QObject *parent) : QObject(parent)
{
    const QString serverName = "fsnpview-server";
    m_localServer = new QLocalServer(this);
    if (!m_localServer->listen(serverName)) {
        if (m_localServer->serverError() == QAbstractSocket::AddressInUseError) {
            QLocalServer::removeServer(serverName);
            m_localServer->listen(serverName);
        }
    }
    connect(m_localServer, &QLocalServer::newConnection, this, &Server::newConnection);
}

void Server::newConnection()
{
    QLocalSocket *socket = m_localServer->nextPendingConnection();
    if (socket) {
        connect(socket, &QLocalSocket::readyRead, this, &Server::readyRead);
        connect(socket, &QLocalSocket::disconnected, socket, &QLocalSocket::deleteLater);
        std::cout << "New connection received." << std::endl;
    }
}

void Server::readyRead()
{
    QLocalSocket *socket = qobject_cast<QLocalSocket*>(sender());
    if (socket) {
        QDataStream stream(socket);
        stream.startTransaction();
        QStringList files;
        stream >> files;
        if (stream.commitTransaction()) {
            std::cout << "Received files from new instance: " << files.join(", ").toStdString() << std::endl;
            emit filesReceived(files);
            socket->disconnectFromServer();
        }
    }
}
