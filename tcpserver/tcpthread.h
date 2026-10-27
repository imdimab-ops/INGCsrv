#ifndef TCPTHREAD_H
#define TCPTHREAD_H

#include <QThread>
#include <QTimer>
#include "tcpserver.h"
#include "parsingdata.h"

class TcpThread : public QThread
{
    Q_OBJECT

private:
    explicit TcpThread(QObject *parent = nullptr);
    ~TcpThread();

    TcpServer *m_tcpServer = nullptr;
    QThread m_thread;
    bool createThread();
    /* вызов метода на прямую с блокировкой основного потока */
    template<typename T, typename Class> T callMethodInThread(Class *obj, T (Class::*method)());

public:
    static TcpThread *getInstance();

    /* getters */
    QStringList getIPList();

signals:
    /* In */
    void startServer(const QString &host, const QString &port);
    void stopServer();
    void disconnectClient(const QString &client);

    /* From ParsingData */
    void setServerInfo(const QVariantMap &map);
    void setListTags(const QList<QString> &tags);
    void setDataChanged(const QVariantMap &map);
    void setEventsChanged(const QVariantMap &map);

    /* Out */
    void serverUpdated(const STWsrv &);   //слово состояния
    void clientsUpdated(const QList<ClientSnapshot> &);  //передаем QHash <QTcpSocket*, STWcli> hClient; по всем клиентам
    void updateHistoryCounter(const QString &tagapp, const quint16 &size); //обновляем на UI счетчики исторических данных
    void status(const QString &);

};

#endif // TCPTHREAD_H
