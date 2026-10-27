//TcpServer серверная часть

#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <QObject>
#include <QVariant>
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QNetworkInterface>
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QHostAddress>
#include <QList>
#include <QDebug>
#include "processdata.h"
#include "parsingdata.h"

//----регестрируем глобальные типы данных (UDT - пользовательские типы данных)---//
//Слово-состояние сервера
struct STWsrv {
    bool start = false;
    QString host = "";
    QString port = "";
    quint16 client_size = 0;
};
Q_DECLARE_METATYPE(STWsrv)

//Снимок-состояния списка всех клиентов для обновления UI
struct ClientSnapshot {
    QVariant id;              // уникальный идентификатор (objectName сокета)
    QVariant connectDateTime; // строка, а не QDateTime — дешевле копировать
    QVariant objectName;
    QVariant osver;
    QVariant appver;
    QVariant uptime;          // "hh:mm:ss"
    QVariant peer_ip;
    QVariant peer_port;
    QVariant Tx;              // "↑ 1.23 MB"
    QVariant Rx;              // "↓ 4.56 MB"
};
Q_DECLARE_METATYPE(ClientSnapshot)

//-------------------------------------------------------------------//

class TcpServer : public QObject
{
    Q_OBJECT

public:
    explicit TcpServer(QObject *parent = nullptr);
    ~TcpServer();

private:
    int m_maxConnections = 80; //максимальное количество входящих подключений

    QTcpServer *m_tcpServer = nullptr;
    QList<QTcpSocket *> m_clients; //в Qt6 только QList (QVector оставлен для обратной совместимости)

    STWsrv m_stw;
    QTimer *m_timer = nullptr;
    uint m_pingCounter = 0;
    ProcessData *m_processData = nullptr;
    ParsingData *parsingData = ParsingData::getInstance();

    //Используем QHash, (не вектор стурктур) т.к. часть необходимых данных лежат в QTcpSocket
    //(но если передаем через указатель можем управлять из другого класса всеми QTcpSocket*!!!)
    QHash <QTcpSocket*, STWcli> m_hClients;
    QString bytesToString(const quint64 &bytes);
    void bytesTransmit(STWcli &client);
    QList<ClientSnapshot> makeSnapshot() const; //делаем снимок клиентов
    void createdTimer();

private slots:
    void slotNewConnection();       //подключаем новых клиентов в разные слоты
    void slotClientDisconnected();
    void slotHandleError(QTcpSocket::SocketError error);
    void socketReadyRead();
    void packetCompleted(QTcpSocket *socket, const QVariantMap &map);

public slots:
    void startServer(const QString &host, const QString &port);
    void stopServer();
    void disconnectClient(const QString &client);
    void socketWrite(enum SendType type, QTcpSocket *socket, const QVariantMap &map, const QVariantMap &allMap);

public:
    /* getters */
    QStringList getIPList();  //Выводим на UI список доступных IP адресов на хосте

signals:
    void serverUpdated(const STWsrv &);   //слово состояния
    void clientsUpdated(const QList<ClientSnapshot> &);  //передаем лист ClientSnapshot (передавать указатель QTcpSocket* в хэше не потокобезопасно!)
    void status(const QString &);
};

#endif // TCPSERVER_H
