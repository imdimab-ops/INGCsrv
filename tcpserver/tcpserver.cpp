#include "tcpserver.h"

TcpServer::TcpServer(QObject *parent)
{

    m_processData = new ProcessData(this);
    connect(m_processData, &ProcessData::status, this, &TcpServer::status);
    connect(m_processData, &ProcessData::readPacketCompleted, this, &TcpServer::packetCompleted);
    //Обрабатываем запросы клиента
    connect(parsingData, &ParsingData::sendData, this, &TcpServer::socketWrite);
}

TcpServer::~TcpServer()
{
    if (parsingData) {
        disconnect(parsingData, nullptr, this, nullptr);
    }
    stopServer();
}

QString TcpServer::bytesToString(const quint64 &bytes)
{
    const char *units[] = {" B", " KB", " MB", " GB", " TB"};
    double size = static_cast<double>(bytes);
    uint unit = 0;
    while (size >= 1024 && unit < 4) {
        size /= 1024;
        unit++;
    }
    return QString::number(size, 'f', unit ? 2 : 0) + units[unit];
}

void TcpServer::bytesTransmit(STWcli &client)
{
    client.Tx = QString("\u2191 %1").arg(bytesToString(client.tx));
    auto secondsPassed = client.connectDateTime.secsTo(QDateTime::currentDateTime());
    client.uptime = QTime(0, 0).addSecs(secondsPassed);
}

QList<ClientSnapshot> TcpServer::makeSnapshot() const
{
    QList<ClientSnapshot> list;
    for (auto it = m_hClients.constBegin(); it != m_hClients.constEnd(); ++it) {
        QTcpSocket *socket = it.key();
        const STWcli &client = it.value();
        if (!socket) continue;

        ClientSnapshot s;
        s.id = socket->objectName();
        s.connectDateTime = client.connectDateTime;
        s.objectName = socket->objectName();
        s.osver = client.OSver;
        s.appver = client.appver;
        s.uptime = client.uptime;
        s.peer_ip = socket->peerAddress().toString();
        s.peer_port = socket->peerPort();
        s.Tx = client.Tx;
        s.Rx = client.Rx;
        list.append(s);
    }
    return list;
}

void TcpServer::createdTimer()
{
    /* Создаем таймер после запуска серевера, чтобы он был создан в том же потоке где и TcpServer, а не главном!!! */
    if (!m_timer) {
        m_timer = new QTimer(this);

        connect(m_timer, &QTimer::timeout, this, [this] {
            QVariantMap updatedMap, allMap;
            parsingData->getSendMap(updatedMap, allMap);
            socketWrite(SendType::CURRENT_DATA, nullptr, updatedMap, allMap);

            //Проверка соединения (ping), отправляем ping, если соединение разорвано физически сработает QAbstractSocket::KeepAliveOption и разорвет соединение
            if (m_pingCounter >= 8) { //8 циклов и пишем ping
                QVariantMap pingMap = QVariantMap({ {"PING", QDateTime::currentMSecsSinceEpoch()} });
                socketWrite(SendType::PING, nullptr, pingMap, pingMap);
                m_pingCounter = 0;
            }
            ++m_pingCounter;
            emit clientsUpdated(makeSnapshot()); //чтобы исключить частые обновления UI, обновляем только по таймеру
        });
    }
    if (m_timer) m_timer->start(850);
}

void TcpServer::slotNewConnection()
{
    QTcpSocket *socket = m_tcpServer->nextPendingConnection();
    if (!socket) return;
    socket->setSocketOption(QAbstractSocket::KeepAliveOption, 1); //если соединение разорвано физически сработает QAbstractSocket::KeepAliveOption и разорвет соединение
    if (socket->state() == QTcpSocket::ConnectedState) {
        connect(socket, &QTcpSocket::readyRead, this, &TcpServer::socketReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &TcpServer::slotClientDisconnected);   //удаляем, если в сокете сработал метод disconnected
        connect(socket, &QTcpSocket::errorOccurred, this, &TcpServer::slotHandleError);         //обработка ошибок
        m_clients.append(socket);
        m_stw.client_size = m_clients.size();
    } else {
        socket->deleteLater();
        return;
    }
    emit serverUpdated(m_stw);
    parsingData->setServerInfo(QVariantMap{{"clientSize", m_stw.client_size}});
    emit status(QString("[Info] %1, IP:%2:%3, m_clients: %4, status: %5").arg(Q_FUNC_INFO, socket->peerAddress().toString(), QString::number(socket->peerPort()),
                                                                              QString::number(m_stw.client_size), socket->errorString()));
}

void TcpServer::slotClientDisconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    /* список клиентов: find + erase по итератору */
    if (auto it = std::find(m_clients.begin(), m_clients.end(), socket); it != m_clients.end()) { //C++17 init-statement, it живет только внутри if
        m_clients.erase(it);
    }
    m_stw.client_size = m_clients.size();

    /* получаем ссылку значение, через find поиск осуществляется 1 раз
     * (более производительно чем через contains(key), hash[key] )  */
    if (auto it = m_hClients.find(socket); it != m_hClients.end()) { //C++17 init-statement, it живет только внутри if
        const STWcli &info = it.value();
        emit status(QString("[Info] %1, %2, IP:%3:%4, uptime: %5, %6, %7, m_clients: %8").arg(Q_FUNC_INFO, socket->objectName(),
                                                                                              socket->peerAddress().toString(),
                                                                                              QString::number(socket->peerPort()),
                                                                                              info.uptime.toString("hh:mm:ss"),
                                                                                              info.Tx,
                                                                                              info.Rx,
                                                                                              QString::number(m_stw.client_size)));

        //удаляем из хэша клиентов по итератору it и отправляем только имя отключенного клиента
        m_hClients.erase(it);
    }
    disconnect(socket, nullptr, this, nullptr); //отключаем сигналы (чтобы не было повторных вызовов)
    socket->deleteLater();  //безопасное удаление объекта

    emit serverUpdated(m_stw);
    emit clientsUpdated(makeSnapshot());
    parsingData->setServerInfo(QVariantMap{{"clientSize", m_stw.client_size}});
}

void TcpServer::slotHandleError(QTcpSocket::SocketError error)
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (socket) {
        emit status(QString("[Error] %1, %2, IP:%3:%4, %5").arg(Q_FUNC_INFO, socket->objectName(),
                                                                socket->peerAddress().toString(),
                                                                QString::number(socket->peerPort()),
                                                                socket->errorString()));
    }
}

void TcpServer::socketReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (socket && socket->state() == QAbstractSocket::ConnectedState) {
        //вначате получаем количество пришедших байт, потом читаем из слота (не наоборот)
        qint64 bytes = socket->bytesAvailable();
        m_hClients[socket].rx += bytes; //добавляем в хэш, если клиент не авторизуется, удаляем из хэша в слоте slotClientDisconnected()
        m_processData->readPacket(socket, m_hClients, socket->readAll());
    }
}

void TcpServer::packetCompleted(QTcpSocket *socket, const QVariantMap &map)
{
    if (!socket || map.isEmpty()) return;
    //проверяем авторизацию и авторизовываемся
    if (!m_processData->authorization(socket, m_hClients, map)) return;
    //на предыдущей итерации client уже будет удален, поэтому прекращаем дальнейшее выполнение метода

    //обновляем статистику сервера и клиентов
    auto it = m_hClients.find(socket);
    if (it != m_hClients.end()) it.value().Rx = QString("\u2193 %1").arg(bytesToString(it.value().rx));

    //дальше парсим, что нам надо
    parsingData->engineRequest(socket, map);
    /* для запрса истории узла получаем tag, в хеше берем по данному тэгу nodeId узла
     * (nodeId надо добавить в хэш тегов в классе Engine), формируем запрос исторических
     * данных на сервере OPCUA! */
    // qDebug() << "socketReadyRead" << hSocket.value(client).Rx << map;
}

void TcpServer::startServer(const QString &host, const QString &port)
{
    /* если сервер запущен, то он повторно не создатся */
    if (m_tcpServer == nullptr) {
        m_tcpServer = new QTcpServer(this);
        m_tcpServer->setMaxPendingConnections(m_maxConnections); //максимальное количество входящих подключений
        if (m_tcpServer->listen(QHostAddress(host), port.toUShort())) {    //запускаем сервер, если не запущен, смотрим ошибку
            createdTimer();
            m_stw.start = true;
            m_stw.host = host;
            m_stw.port = port;

            connect(m_tcpServer, &QTcpServer::newConnection, this, &TcpServer::slotNewConnection);
            emit status(QString("[Info] %1, IP:%2:%3").arg(Q_FUNC_INFO, m_tcpServer->serverAddress().toString(), QString::number(m_tcpServer->serverPort())));
        } else {
            m_stw.start = false;
            m_stw.host.clear();
            m_stw.port.clear();
            emit status(QString("[Error] %1, Error: %2").arg(Q_FUNC_INFO, m_tcpServer->errorString()));
            return;
        }
    };
    emit serverUpdated(m_stw);
}

void TcpServer::stopServer()
{
    /* QObject::killTimer: Timers cannot be stopped from another thread
     * QObject::~QObject: Timers cannot be stopped from another thread
     * !!! Во избежании ошибок завершения работы таймера в отдельном потоке вызываем данный метод
     * в деструкторе основного потока, чтобы успели завершиться все асинхронные операции до разрушения
     * системы событий Qt */

    /* безопасно перебираем вектор клиентов, начинаем с конца (если с 0 элемента закрывать, то вектор смещается
     * при каждой итерации на 0 элемент и мы начинаем обращаться в пустым элементам QList) */
    while(!m_clients.isEmpty()) {
        /* удаляем так, т.к. при работе деструктора и вызове m_clients.last()->close(); система событий Qt уже удалена
         * и данный цикл зацикливается, слот slotClientDisconnected, где очищается ветор m_clients не вызывается */
        QTcpSocket *client = m_clients.takeLast();
        if (!client) continue;
        client->abort();
        client->deleteLater();
    }
    m_clients.clear();
    m_hClients.clear();
    if (m_tcpServer != nullptr && m_clients.isEmpty()) {
        if (m_timer) m_timer->stop();
        m_tcpServer->close();
        m_tcpServer->deleteLater();   //безопасное удаление объектов
        m_tcpServer = nullptr;

        m_stw.start = false;
        m_stw.host.clear();
        m_stw.port.clear();
        m_stw.client_size = m_clients.size();
        emit serverUpdated(m_stw);
        emit status(QString("[Info] %1").arg(Q_FUNC_INFO));
    }
}

void TcpServer::disconnectClient(const QString &client)
{
    for (QTcpSocket *socket : std::as_const(m_clients)) {
        if (socket && socket->objectName() == client) {
            socket->disconnectFromHost();
            break;
        }
    }
}

void TcpServer::socketWrite(enum SendType type, QTcpSocket *socket, const QVariantMap &map, const QVariantMap &allMap)
{
    if (m_tcpServer == nullptr || m_clients.size() == 0 || map.isEmpty() || allMap.isEmpty()) return;

    switch (type) {
    case SendType::CURRENT_DATA:
        for (QTcpSocket *pclient : std::as_const(m_clients)) { //в ф-ии serverResponse() QTcpSocket *client = nullptr
            //дополнительно проверяем авторизацию на тот случай, если клиент подключился но еще не авторизовался
            if (pclient && m_processData->authorization(pclient)) {
                qint64 bytes = 0;

                /* получаем ссылку значение, через find поиск осуществляется 1 раз
                 * (более производительно чем через contains(key), hash[key] )  */
                auto it = m_hClients.find(pclient);
                if (it == m_hClients.end()) continue;
                if (m_processData->isfirstscan(pclient, allMap, bytes)) {
                    it.value().tx += bytes;
                } else {
                    it.value().tx += m_processData->writeSocketWidthControl(pclient, m_processData->createPacket(map));
                };
                bytesTransmit(it.value());
            }
        }
        break;
    case SendType::RESPONCE_DATA:
        if (socket && m_processData->authorization(socket)) {
            auto it = m_hClients.find(socket);
            if (it == m_hClients.end()) break;
            it.value().tx += m_processData->sendHistoryData(socket, map); //проверка исторических данных внутри метода sendHistoryData()
            bytesTransmit(it.value());
        }
        break;
    case SendType::PING:
        for (QTcpSocket *pclient : std::as_const(m_clients)) {
            if (pclient && m_processData->authorization(pclient)) {
                qint64 bytes = 0;
                auto it = m_hClients.find(pclient);
                if (it == m_hClients.end()) continue;
                it.value().tx += m_processData->writeSocketWidthControl(pclient, m_processData->createPacket(map));
                bytesTransmit(it.value());
            }
        }
        break;
    default:
        break;
    }
}

QStringList TcpServer::getIPList()
{
    QList<QHostAddress> ipAddressesList = QNetworkInterface::allAddresses();
    QStringList list;
    //узнаем количество доступных IP адресов
    for (auto i = 0; i < ipAddressesList.count(); i++) {
        if (ipAddressesList.at(i).toIPv4Address() && ipAddressesList.at(i) != QHostAddress::LocalHost) {
            list.append(ipAddressesList.at(i).toString());
        };
    }
    return list;
}
