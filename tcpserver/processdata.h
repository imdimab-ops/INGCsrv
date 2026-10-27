#ifndef PROCESSDATA_H
#define PROCESSDATA_H

#include <QObject>
#include <QtNetwork/QTcpSocket>
#include <QDateTime>
#include <QTimer>
#include <QRandomGenerator>
#include <QByteArray>
#include <QCryptographicHash>
#include <QCoreApplication>
#include <openssl/aes.h>
#include <openssl/evp.h>

//----регестрируем глобальные типы данных (UDT - пользовательские типы данных)----//

struct Buffer { //буфер для сбора TCP пакетов (мах размер передаваемого пакета 1448байт)
    QByteArray abytes = 0;
    quint32 bodySize = 0;
    bool headerRead = false;
    bool bodyCompressed = false;
};

//слово-состояние клиента
struct STWcli {
    QDateTime connectDateTime;
    QTime uptime;
    QString name;
    QString OSver;
    QString appver;
    QString Rx; //количество принятых байт (с дескриптором)
    QString Tx; //количество переданых байт (с дескриптором)
    quint64 rx; //количество принятых байт
    quint64 tx; //количество переданных байт
    Buffer buffer;
};
//-------------------------------------------------------------------//

class ProcessData : public QObject
{
    Q_OBJECT
public:
    explicit ProcessData(QObject *parent = nullptr);
    ~ProcessData();

private:
    struct Credentials  { //если логин и пароль уникальны, то данную структуру хранить в хэше клиентов hClient в стуктуре STWcli
        QString login = "userINGCgts";   //логин и пароль для авторизации на сервере
        QString pass = "userINGC";
        QByteArray key; //ключ шифрования
        QByteArray iv;  //вектор шифрования
    };
    Credentials m_cred;

    static constexpr qint64 MAX_COMPRESS_SIZE = 1073741824; // 2^30 = 1 GiB (~1GB)
    static constexpr qsizetype HEADER_SIZE   = sizeof(quint32) + sizeof(quint8) + sizeof(quint8) + sizeof(quint16); //8байт
    static constexpr quint32 MAX_BODY_SIZE = 64u * 1024u * 1024u;   //64 МБ (максимальный размер буфера данных)
    static constexpr quint8 FLAG_COMPRESSED = 0x01;

    //используем для исключения повторяющихся имен
    //т.к по имени закрываем сокет и удаляем клиентов из QList<QTcpSocket *> clients
    static quint16 random();
    void generateCryptKey(const QString &login, const QString &pass);
    QByteArray encrypt(const QByteArray &crypba, const QByteArray &key, const QByteArray &iv);
    QByteArray decrypt(const QByteArray &encrba, const QByteArray &key, const QByteArray &iv);
    QByteArray compressData(const QByteArray &ba, bool &compressed);
    QByteArray uncompressData(const QByteArray &ba);

public slots:
    QByteArray createPacket(const QVariantMap &map);
    void readPacket(QTcpSocket *socket, QHash <QTcpSocket*, STWcli> &hSocket, const QByteArray &packet);
    qint64 writeSocketWidthControl(QTcpSocket *socket, const QByteArray &data); //записть в сокет с контролем буфера TCP
    bool authorization(QTcpSocket *socket, QHash <QTcpSocket*, STWcli> &hSocket, const QVariantMap &map);
    bool authorization(QTcpSocket *socket);
    bool isfirstscan(QTcpSocket *socket, const QVariantMap &map, qint64 &bytes);
    qint64 firstscan(QTcpSocket *socket, const QVariantMap &map);
    qint64 sendHistoryData(QTcpSocket *socket, const QVariantMap &map);

signals:
    void status(const QString &);
    void readPacketCompleted(QTcpSocket*, const QVariantMap &);

};

#endif // PROCESSDATA_H
