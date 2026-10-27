#include "tcpthread.h"

TcpThread::TcpThread(QObject *parent)
    : QThread{parent}
{
    createThread();
}

TcpThread::~TcpThread()
{
    if(m_thread.isRunning()) {
        m_thread.quit();
        m_thread.wait();
    }
}

TcpThread *TcpThread::getInstance()
{
    static TcpThread instance;
    return &instance;
}

bool TcpThread::createThread()
{
    m_tcpServer = new TcpServer;
    m_tcpServer->moveToThread(&m_thread);
    // connect(&thread, &QThread::finished, m_tcpServer, &QObject::deleteLater);  //удалил, иначе двойное удаление
    connect(this, &TcpThread::startServer, m_tcpServer, &TcpServer::startServer);
    connect(this, &TcpThread::stopServer, m_tcpServer, &TcpServer::stopServer);
    connect(this, &TcpThread::disconnectClient, m_tcpServer, &TcpServer::disconnectClient);

    connect(m_tcpServer, &TcpServer::serverUpdated, this, &TcpThread::serverUpdated);
    connect(m_tcpServer, &TcpServer::clientsUpdated, this, &TcpThread::clientsUpdated);
    connect(m_tcpServer, &TcpServer::status, this, &TcpThread::status);

    ParsingData *parsingData = ParsingData::getInstance();
    if (parsingData->thread() != &m_thread) {
        parsingData->moveToThread(&m_thread); //проверяем что parsingData не в другом потоке (т.к. синглтон)
    }
    // connect(&thread, &QThread::finished, parsingData, &QObject::deleteLater); //не удаляем через deleteLater, т.к. ParsingData - синглтон
    connect(this, &TcpThread::setServerInfo, parsingData, &ParsingData::setServerInfo);
    connect(this, &TcpThread::setListTags, parsingData, &ParsingData::setListTags);
    connect(this, &TcpThread::setDataChanged, parsingData, &ParsingData::setDataChanged);
    connect(this, &TcpThread::setEventsChanged, parsingData, &ParsingData::setEventsChanged);
    connect(parsingData, &ParsingData::updateHistoryCounter, this, &TcpThread::updateHistoryCounter);
    connect(parsingData, &ParsingData::status, this, &TcpThread::status);

    m_thread.start();

    //кол-во логических процессоров (отображаем с задержкой, чтобы Ui с логгером успел создаться)
    QTimer::singleShot(50, this, [this]() {
        if (this->m_thread.isRunning()) emit status(QString("[Info] %1, idealThreadCount:%2").arg(Q_FUNC_INFO, QString::number(this->m_thread.idealThreadCount())));
    });
    return true;
}

template<typename T, typename Class>
T TcpThread::callMethodInThread(Class *obj, T (Class::*method)())
{
    /* вызываем методы из объекта, перемещенного в другой поток,
     * основной поток блокируется, используем только для быстрый операций */
    T result;
    if (!obj) return result;
    QMetaObject::invokeMethod(obj, [obj, method, &result]() {
        result = (obj->*method)();
    }, Qt::BlockingQueuedConnection);
    return result;
}

QStringList TcpThread::getIPList()
{
    return callMethodInThread(m_tcpServer, &TcpServer::getIPList);
}

