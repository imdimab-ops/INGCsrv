#ifndef ENGINE_H
#define ENGINE_H

#include <QObject>
#include "opcua/opcuaclient.h"
#include "tcpserver/tcpthread.h"
#include "viewmapping/modelmapping.h"
#include "viewmapping/modelevent.h"
#include "applib.h"

class Engine : public QObject
{
    Q_OBJECT

private:
    explicit Engine(QObject *parent = nullptr);
    ~Engine();

    void setupConnections();

    QList<QString> nodeIds_mem;
    QList<QString> eventNodeIds_mem;
    bool bsubscribeDataEnable = false;   //флаг разрешения на создание запроса на чтение
    bool bsubscribeEventEnable = false;  //флаг разрешения на создание запроса на получение сообщений
    bool bconnectOPC = false;            //флаг подключения к серверу OPC

    OPCUAClient *opcClient = OPCUAClient::getInstance();
    TcpThread *tcpsrv = TcpThread::getInstance();
    AppLib *applib = AppLib::getInstance();

public:
    static Engine *getInstance();
    void setSubscribeEnable(const bool &data, const bool &event);
    void createSubscribeOPC();
    void createSubscribeOPCTimer();

};

#endif // ENGINE_H
