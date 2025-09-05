#ifndef SERVER_H
#define SERVER_H

#include <QObject>
#include <QStringList>

class QLocalServer;
class QLocalSocket;

class Server : public QObject
{
    Q_OBJECT
public:
    explicit Server(QObject *parent = nullptr);

signals:
    void filesReceived(const QStringList &files);

private slots:
    void newConnection();
    void readyRead();

private:
    QLocalServer *m_localServer;
};

#endif // SERVER_H
