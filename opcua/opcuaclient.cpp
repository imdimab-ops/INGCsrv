#include "opcuaclient.h"

OPCUAClient::OPCUAClient(QObject *parent)
    : QObject{parent}
{
    ptimer = new QTimer(this);
    ptimer->setInterval(100);
    connect(ptimer, &QTimer::timeout, this, &OPCUAClient::processTimer);
    preadTimer = new QTimer(this);
    preadTimer->setInterval(350);
    connect(preadTimer, &QTimer::timeout, this, &OPCUAClient::processReadNodeAsync);
    qDebug() << Q_FUNC_INFO << getVersion();
}

OPCUAClient::~OPCUAClient()
{
    disconnectFromServer();
}

OPCUAClient *OPCUAClient::getInstance()
{
    static OPCUAClient instance;
    return &instance;
}

void OPCUAClient::freeMemory_dll(UA_ByteString &bstr)
{
    /* Очищаем память для динамически собранной библиотеки *.dll !!!
     * Для *.dll при инициализации значений UA_ByteString b_str = UA_BYTESTRING_NULL
     * и последующей очистки памяти UA_ByteString_clear(&b_str) программа вылетает.
     * UA_BYTESTRING_NULL и UA_ByteString_clear корректно работает в статической библиотеке.
     * Для использования UA_ByteString_clear в *.dll необходимо объявлять переменные ALLOC */

    if(bstr.data) {
        //NOLINTNEXTLINE(clang-analyzer-unix.Malloc)
        UA_free(bstr.data);
        bstr = UA_BYTESTRING_NULL;
    }
}

bool OPCUAClient::generateSelfSignedCertificate(UA_ByteString &cert, UA_ByteString &key)
{
    QString hostName = QSysInfo::machineHostName();
    UA_String subject = UA_STRING_ALLOC(QString("CN=ingc_opcuaclient@%1").arg(hostName).toUtf8().constData());
    //ВАЖНО!!! URI сертификата клиента и URI в настроенном клиенте должна совпадать!
    UA_String subjectAltName = UA_STRING_ALLOC(QString("DNS:%1, URI:%2").arg(hostName, applicationUri).toUtf8().constData());

    //Создаем структуру UA_KeyValueMap с правильной инициализацией
    UA_KeyValueMap kvm;
    kvm.mapSize = 3;
    kvm.map = (UA_KeyValuePair*)UA_Array_new(kvm.mapSize, &UA_TYPES[UA_TYPES_KEYVALUEPAIR]);

    //Добавляем параметры
    UA_UInt16 keySize = 2048; //разрядность ключа
    UA_String hashAlgo = UA_STRING_ALLOC("SHA-256");
    UA_UInt16 expires = 1820; //5 лет = 360 * 5

    UA_KeyValueMap_setScalar(&kvm,
                             UA_QUALIFIEDNAME_ALLOC(0, "key-size-bits"),
                             &keySize,
                             &UA_TYPES[UA_TYPES_UINT16]);

    UA_KeyValueMap_setScalar(&kvm,
                             UA_QUALIFIEDNAME_ALLOC(0, "hash-algorithm"),
                             &hashAlgo,
                             &UA_TYPES[UA_TYPES_STRING]);

    UA_KeyValueMap_setScalar(&kvm,
                             UA_QUALIFIEDNAME_ALLOC(0, "expires-in-days"),
                             &expires,
                             &UA_TYPES[UA_TYPES_UINT16]);


    UA_StatusCode retval = UA_CreateCertificate(
        UA_Log_Stdout,              // Логгер
        &subject,                   // Subject
        1,                          // Размер subject
        &subjectAltName,            // Subject Alternative Name
        1,                          // Размер subjectAltName
        UA_CERTIFICATEFORMAT_DER,   // Формат сертификата (DER-бинарный формат, PEM-текстовый формат с base64 кодировкой)
        &kvm,                       // Дополнительные параметры
        &key,                       // Выходной приватный ключ
        &cert                       // Выходной сертификат
        );

    // Очистка памяти
    UA_String_clear(&subject);
    UA_String_clear(&subjectAltName);
    UA_String_clear(&hashAlgo);
    UA_KeyValueMap_clear(&kvm); //Данный метод работает, если библиотека собрана динамическая

    emit status(QString("[Info] %1, status: %2").arg(Q_FUNC_INFO, UA_StatusCode_name(retval)));
    return (retval == UA_STATUSCODE_GOOD);
}

bool OPCUAClient::saveCertificate(const QString &fileName, const char *data, const qint64 &len)
{
    /* fileName - в формате Certificate.der */
    QFile file;
    QDir dir;
    QString path = dir.currentPath() + "/cert/";
    if (!dir.exists(path)) dir.mkpath(path);    //создаем дирректорию
    file.setFileName(path + fileName);          //создаем файл в дирректории path/

    if (!file.open(QIODevice::WriteOnly)) {  //file.open() создаст файл (с именем fileName) в определенной папке, если его там нету
        file.close();
        return false;
    }
    qint64 bytesWrite = file.write(data, len);
    emit status(QString("[Info] %1, %2: %3").arg(Q_FUNC_INFO, fileName, QString::number(bytesWrite)));
    file.close();
    return true;
}

UA_ByteString OPCUAClient::loadCertificate(const QString &fileName)
{
    /* UA_BYTESTRING_ALLOC, после использования не забываем очистить память UA_ByteString_clear(); */
    QFile file;
    QDir dir;
    QString path = dir.currentPath() + "/cert/" + fileName;
    file.setFileName(path);
    if (!file.open(QIODevice::ReadOnly)) {
        file.close();
        emit status(QString("[Error] %1: Failed to open file %2").arg(Q_FUNC_INFO, fileName));
        return UA_BYTESTRING_NULL;
    }

    //читаем все данные из файла
    QByteArray fileData = file.readAll();
    file.close();
    if (fileData.isEmpty()) {
        emit status(QString("[Error] %1: File is empty %2").arg(Q_FUNC_INFO, fileName));
        return UA_BYTESTRING_NULL;
    }

    UA_ByteString cert;
    UA_StatusCode result = UA_ByteString_allocBuffer(&cert, fileData.size());

    if (result != UA_STATUSCODE_GOOD) {
        emit status(QString("[Error] %1: Memory allocation failed for %2").arg(Q_FUNC_INFO, fileName));
        return UA_BYTESTRING_NULL;
    }

    memcpy(cert.data, fileData.constData(), cert.length);
    emit status(QString("[Info] %1, %2: %3 bytes loaded").arg(Q_FUNC_INFO, fileName, QString::number(cert.length)));
    return cert;
}

bool OPCUAClient::isConnected(UA_Client *&client)
{
    if (!client) return false;
    UA_SecureChannelState channelState;
    UA_SessionState sessionState;
    UA_StatusCode connectStatus;
    UA_Client_getState(client, &channelState, &sessionState, &connectStatus);
    return (channelState == UA_SECURECHANNELSTATE_OPEN && sessionState == UA_SESSIONSTATE_ACTIVATED);
}

void OPCUAClient::cleanupHashContexts(const UA_UInt32 &subscriptionId)
{
    /* очищаем хэш с подписками и удаляем контексты */
    if (auto it = hSubscribeGroupEvent.find(subscriptionId); it != hSubscribeGroupEvent.end()) { //C++17 init-statement, it живет только внутри if
        SubscribeGroupEvent *subscribeGroup = it.value();
        hSubscribeGroupEvent.erase(it); //erase() - сюда сразу передал итератор, по которому выполняется удаление без повторного поиска
        delete subscribeGroup; // все ресурсы очищаем в деструкторе ~ContextEvent()
        qDebug() << Q_FUNC_INFO << "subscriptionId" << subscriptionId << "hSubscribeGroupEvent.size()" << hSubscribeGroupEvent.size();
    }
    if (auto it = hSubscribeGroupNodes.find(subscriptionId); it != hSubscribeGroupNodes.end()) {
        SubscribeGroupNodes *subscribeGroup = it.value();
        hSubscribeGroupNodes.erase(it);
        delete subscribeGroup; // все ресурсы очищаем в деструкторе ~ContextEvent()
        qDebug() << Q_FUNC_INFO << "subscriptionId" << subscriptionId << "hSubscribeGroupNodes.size()" << hSubscribeGroupNodes.size();
    }
}

void OPCUAClient::cleanupResourcesSubscribeAsync(UA_EventFilter *&filter, UA_MonitoredItemCreateRequest *&monRequests, UA_UInt32 &monitoredIdsSize)
{
    /* передаем указатель по ссылке чтобы получить доступ о оригинальному объекту хранящемуся в структуре SubscribeGroup,
     * а не копии на указатель! При повторном вызове при закрытии подключениия указатель из структуры SubscribeGroup
     * будет уже обнулен (первый раз вызываю этот метод в callback при создании подписки для очистки ресурсов */
    if (filter) {
        UA_EventFilter_clear(filter);
        UA_EventFilter_delete(filter);
        filter = nullptr;
    }
    if (monRequests) {
        //не очищаем отдельно фильтры внутри - они удалятся автоматически
        UA_Array_delete(monRequests, monitoredIdsSize, &UA_TYPES[UA_TYPES_MONITOREDITEMCREATEREQUEST]);
        monRequests = nullptr;
    }
    qDebug() << Q_FUNC_INFO;
}

void OPCUAClient::cleanupResourcesSubscribeAsync(UA_DataChangeFilter *&filter, UA_MonitoredItemCreateRequest *&monRequests, UA_UInt32 &monitoredIdsSize)
{
    /* передаем указатель по ссылке чтобы получить доступ о оригинальному объекту хранящемуся в структуре SubscribeGroup,
     * а не копии на указатель! При повторном вызове при закрытии подключениия указатель из структуры SubscribeGroup
     * будет уже обнулен (первый раз вызываю этот метод в callback при создании подписки для очистки ресурсов */
    if (filter) {
        UA_DataChangeFilter_clear(filter);
        UA_DataChangeFilter_delete(filter);
        filter = nullptr;
    }
    if (monRequests) {
        //не очищаем отдельно фильтры внутри - они удалятся автоматически
        UA_Array_delete(monRequests, monitoredIdsSize, &UA_TYPES[UA_TYPES_MONITOREDITEMCREATEREQUEST]);
        monRequests = nullptr;
    }
    qDebug() << Q_FUNC_INFO;
}

void OPCUAClient::stateCallback(UA_Client *client, UA_SecureChannelState channelState, UA_SessionState sessionState, UA_StatusCode connectStatus)
{
    Q_UNUSED(connectStatus);
    //Эмитировать сигнал через QMetaObject, так как это статическая функция
    OPCUAClient *self = static_cast<OPCUAClient*>(UA_Client_getContext(client));
    if (!self) return;
    //Проверка полного подключения
    if (channelState == UA_SECURECHANNELSTATE_OPEN && sessionState == UA_SESSIONSTATE_ACTIVATED) {
        emit self->connected(true);
        emit self->status(QString("[Success] ✓ Fully connected to server!"));
    }
    //Если дисконнект на стороне сервера
    if (channelState == UA_SECURECHANNELSTATE_CLOSED && sessionState == UA_SESSIONSTATE_CREATED) emit self->connected(false);
    //Если дисконнект на стороне клиента
    if (channelState == UA_SECURECHANNELSTATE_CLOSED && sessionState == UA_SESSIONSTATE_CLOSED) emit self->connected(false);

    if (!self->fdisconnectQueued && channelState == UA_SECURECHANNELSTATE_CLOSED) {
        self->fdisconnectQueued = true;
        QMetaObject::invokeMethod(self, [self]() { //поставили вызов в очередь событий
            self->disconnectFromServer();
        }, Qt::QueuedConnection); //Qt::QueuedConnection - вызываем в очереди событий
    }
    // emit self->status(QString("Status_callback: %1").arg(UA_StatusCode_name(connectStatus)));
}

void OPCUAClient::loggingCallback(void *context, UA_LogLevel level, UA_LogCategory category, const char *msg, va_list args)
{
    Q_UNUSED(context)
    Q_UNUSED(category)
    //Эмитировать сигнал через QMetaObject, так как это статическая функция
    OPCUAClient *self = static_cast<OPCUAClient*>(context);
    if (!self) return;

    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), msg, args);
    QString message = QString::fromUtf8(buffer);

    switch(level) {
    case UA_LOGLEVEL_TRACE:
        break;
    case UA_LOGLEVEL_DEBUG:
        break;
    case UA_LOGLEVEL_INFO:
        emit self->status("open62541_[Info]: " + message);
        break;
    case UA_LOGLEVEL_WARNING:
        emit self->status("open62541_[Warning]: " + message);
        break;
    case UA_LOGLEVEL_ERROR:
        emit self->status("open62541_[Error]: " + message);
    case UA_LOGLEVEL_FATAL:
        break;
    }
}

void OPCUAClient::subscribeEventCallback(UA_Client *client, UA_UInt32 subId, void *subContext, UA_UInt32 monId, void *monContext, size_t nEventFields, UA_Variant *eventFields)
{
    /* статическая функция создается в единственном экземляре и обрабатывает множество разных подпсок через механизм контекстов */
    Q_UNUSED(subContext); Q_UNUSED(subId); Q_UNUSED(monId);
    ContextEvent *ctx = static_cast<ContextEvent*>(monContext); //для синхронного запроса контекс this (OPCUAClient)
    OPCUAClient *self = static_cast<OPCUAClient*>(UA_Client_getContext(client));
    if (!ctx || !self) return;

    //parsing
    QDataEvent qdataEvent = self->parsingEvent(eventFields, nEventFields, ctx->subscribeGroup->eventFields);
    qdataEvent.nodeId = ctx->nodeId;
    emit self->subscribeEventCompleted(qdataEvent);
    // qDebug() << Q_FUNC_INFO << subId << "eventId:" << qdataEvent.eventId << "sourceNode:" << qdataEvent.sourceNode << "sourceName:" << qdataEvent.sourceName
    //          << "message:" << qdataEvent.message << "severity:" << qdataEvent.severity << qdataEvent.time << qdataEvent.activeState << qdataEvent.bactiveState
    //          << qdataEvent.currentState;
}

void OPCUAClient::subscribeNodesCallback(UA_Client *client, UA_UInt32 subId, void *subContext, UA_UInt32 monId, void *monContext, UA_DataValue *value)
{
    /* статическая функция создается в единственном экземляре и обрабатывает множество разных подпсок через механизм контекстов */
    Q_UNUSED(subContext); Q_UNUSED(subId); Q_UNUSED(monId);
    ContextNodes *ctx = static_cast<ContextNodes*>(monContext); //контекст в callback при подписке передается через monContext
    OPCUAClient *self = static_cast<OPCUAClient*>(UA_Client_getContext(client));
    if (!ctx || !self) return;

    QDataRW qdataRW;
    qdataRW.nodeId = ctx->nodeId;
    qdataRW.status = UA_StatusCode_name(value->status);
    qdataRW.serverTimestamp = self->toQDateTime(value->serverTimestamp);
    if (UA_StatusCode_isGood(value->status) && value->hasValue && !UA_Variant_isEmpty(&value->value)) {
        qdataRW.variant = self->toQVariant(value->value);
        qdataRW.sourceTimestamp = self->toQDateTime(value->sourceTimestamp);
    }
    emit self->subscribeNodesCompleted(qdataRW);
    // qDebug() << Q_FUNC_INFO << "subscriptionId" << subId << "monitoredItemId" << monId << ctx->nodeId;
}

void OPCUAClient::deleteNodesCallback(UA_Client *client, UA_UInt32 subId, void *subContext, UA_UInt32 monId, void *monContext)
{
    /* callback-функция выполняется при удалении monitored item (сервером или клиентом) */
    // SubscriptionGroup *subGroup = static_cast<SubscriptionGroup*>(subContext);
    // if (!subGroup) return;

    // // Находим и удаляем monitored item из нашего списка
    // for (size_t i = 0; i < subGroup->monitoredIdsSize; i++) {
    //     if (subGroup->monitoredIds[i] && subGroup->monitoredIds[i] == monId) {
    //         subGroup->monitoredIds[i] = 0; //помечаем как удаленный
    //         break;
    //     }
    // }

    // OPCUAClient *self = static_cast<OPCUAClient*>(UA_Client_getContext(client));
    // if (self) {
    //     emit self->status(QString("Monitored item deleted - Subscription: %1, MonitoredItem: %2").arg(subId).arg(monId));
    // }
    // qDebug() << Q_FUNC_INFO << "subId" << subId << "monId" << monId;
}

void OPCUAClient::readCallback(UA_Client *client, void *userdata, UA_UInt32 requestId, UA_ReadResponse *rr)
{
    Q_UNUSED(requestId);
    UA_ReadResponse *resp = static_cast<UA_ReadResponse*>(rr);
    ContextRW *ctx = static_cast<ContextRW*>(userdata); //получаем доступ к readContext, переданному через void *userdata
    OPCUAClient *self = static_cast<OPCUAClient*>(UA_Client_getContext(client)); //Эмитируем сигнал через QMetaObject
    if (!ctx || !resp || !self) return;

    if (resp->responseHeader.serviceResult != UA_STATUSCODE_GOOD) {
        emit self->status(QString("[Error] %1 status: %2, size: %3").arg(Q_FUNC_INFO, UA_StatusCode_name(resp->responseHeader.serviceResult), QString::number(resp->resultsSize)));
        return;
    };

    if (ctx->qdataRWs.size() == resp->resultsSize) {
        QDataRW qdataRW;
        QList <QDataRW> qdataRWs;

        for (size_t i = 0; i < resp->resultsSize; i++) {
            qdataRWs.push_back(qdataRW);
            qdataRWs.last().status = UA_StatusCode_name(resp->results[i].status);
            if (UA_StatusCode_isGood(resp->results[i].status) && resp->results[i].value.data != NULL && resp->results[i].value.type != NULL) {
                qdataRWs.last().nodeId = ctx->qdataRWs[i].nodeId;
                qdataRWs.last().variant = self->toQVariant(resp->results[i].value);
                qdataRWs.last().sourceTimestamp = self->toQDateTime(resp->results[i].sourceTimestamp);
                qdataRWs.last().serverTimestamp = self->toQDateTime(resp->results[i].serverTimestamp);
            }
        }
        emit self->readCompleted(qdataRWs);
    }
    self->freadBusy = false;
    //Контекст не удаляем т.к. еще используем
    UA_ReadResponse_clear(resp);
}

void OPCUAClient::writeCallback(UA_Client *client, void *userdata, UA_UInt32 requestId, UA_WriteResponse *wr)
{
    ContextRW *ctx = static_cast<ContextRW*>(userdata);
    UA_WriteResponse *resp = static_cast<UA_WriteResponse*>(wr);
    OPCUAClient *self = static_cast<OPCUAClient*>(UA_Client_getContext(client)); //Эмитируем сигнал через QMetaObject
    if (!ctx || !resp || !self) return;

    if (resp->responseHeader.serviceResult == UA_STATUSCODE_GOOD) {
        //Проверяем результаты записи для каждого узла
        for (size_t i = 0; i < resp->resultsSize; i++) {
            if (resp->results[i] != UA_STATUSCODE_GOOD) {
                emit self->status(QString("[Error] %1 status: %2, nodeId: %3").arg(Q_FUNC_INFO, UA_StatusCode_name(resp->results[i]), ctx->qdataRWs[i].nodeId));
            }
        }
    }
    emit self->status(QString("[Info] %1 status: %2 size: %3").arg(Q_FUNC_INFO, UA_StatusCode_name(resp->responseHeader.serviceResult), QString::number(resp->resultsSize)));

    //Очищаем контекст
    delete ctx;
    ctx = nullptr;
    UA_WriteResponse_clear(resp);
}

UA_Boolean OPCUAClient::historicalEventCallback(UA_Client *client, const UA_NodeId *nodeId, UA_Boolean moreDataAvailable, const UA_ExtensionObject *data, void *callbackContext)
{
    Q_UNUSED(moreDataAvailable); Q_UNUSED(callbackContext);
    OPCUAClient *self = static_cast<OPCUAClient*>(UA_Client_getContext(client)); //Эмитируем сигнал через QMetaObject
    if (!self) return UA_FALSE;

    QString qnodeId = self->toQString(*nodeId);

    //Проверяем, что данные декодированы, Проверяем тип данных - должен быть HistoryData
    if (!data || data->encoding != UA_EXTENSIONOBJECT_DECODED || data->content.decoded.type != &UA_TYPES[UA_TYPES_HISTORYEVENT]) {
        emit self->status(QString("[Error] %1 nodeId: %2 Data is not decoded or Unexpected data type").arg(Q_FUNC_INFO, qnodeId));
        return UA_FALSE;
    }

    UA_HistoryEvent *historyEvent = (UA_HistoryEvent*)data->content.decoded.data;
    if (!historyEvent) return UA_FALSE;

    for (size_t i = 0; i < historyEvent->eventsSize; ++i) {
        UA_HistoryEventFieldList *eventFieldList = &historyEvent->events[i];
        if (!eventFieldList) continue;

        QDataEvent qdataEvent = self->parsingEvent(eventFieldList->eventFields, eventFieldList->eventFieldsSize, self->eventFields);
        qdataEvent.nodeId = qnodeId;
        emit self->historicalEventCompleted(qdataEvent);
        qDebug() << Q_FUNC_INFO << "eventId:" << qdataEvent.eventId << "sourceNode:" << qdataEvent.sourceNode << "sourceName:" << qdataEvent.sourceName
                 << "message:" << qdataEvent.message << "severity:" << qdataEvent.severity << qdataEvent.time << qdataEvent.activeState << qdataEvent.bactiveState
                 << qdataEvent.currentState;
    }
    return UA_TRUE;
}

UA_Boolean OPCUAClient::historicalDataCallback(UA_Client *client, const UA_NodeId *nodeId, UA_Boolean moreDataAvailable, const UA_ExtensionObject *data, void *callbackContext)
{
    /* узлы, которые пробовал прочитать поддерживают только HistoryRead, но не поодерживают Event, либо наоборот
     * при чтении родительского узла (Server) UA_Client_HistoryRead_events возвращает ошибку, т.к. думаю
     * узел с атрибутом родительский узел Event не поддерживает HistoryRead */
    Q_UNUSED(moreDataAvailable); Q_UNUSED(callbackContext);
    OPCUAClient *self = static_cast<OPCUAClient*>(UA_Client_getContext(client)); //Эмитируем сигнал через QMetaObject
    if (!self) return UA_FALSE;

    QString qnodeId = self->toQString(*nodeId);

    //Проверяем, что данные декодированы, Проверяем тип данных - должен быть HistoryData
    if (!data || data->encoding != UA_EXTENSIONOBJECT_DECODED || data->content.decoded.type != &UA_TYPES[UA_TYPES_HISTORYDATA]) {
        emit self->status(QString("[Error] %1 nodeId: %2 Data is not decoded or Unexpected data type").arg(Q_FUNC_INFO, qnodeId));
        return UA_FALSE;
    }

    UA_HistoryData *historyData = static_cast<UA_HistoryData*>(data->content.decoded.data);
    if (!historyData) return UA_FALSE;

    for (size_t i = 0; i < historyData->dataValuesSize; i++) {
        UA_DataValue *value = &historyData->dataValues[i];
        if (!value) continue;

        QDataRW qataRW;
        qataRW.nodeId = qnodeId;
        qataRW.serverTimestamp = self->toQDateTime(value->serverTimestamp);
        qataRW.status = UA_StatusCode_name(value->status);
        if (UA_StatusCode_isGood(value->status) && value->hasValue && !UA_Variant_isEmpty(&value->value)) {
            qataRW.variant = self->toQVariant(value->value);
            qataRW.sourceTimestamp = self->toQDateTime(value->sourceTimestamp);
        }
        emit self->historicalDataCompleted(qataRW);
        qDebug() << Q_FUNC_INFO << qataRW.nodeId << qataRW.status << qataRW.sourceTimestamp << qataRW.variant;
    }
    return UA_TRUE;
}

QDataEvent OPCUAClient::parsingEvent(const UA_Variant *eventFields, const size_t &nEventFields, const QList<QString> &eventFieldList)
{
    if (!eventFields || eventFieldList.isEmpty() || eventFieldList.size() > nEventFields) return QDataEvent{};

    QDataEvent qdataEvent;
    for (size_t i = 0; i < nEventFields; ++i) {
        //EventId - ByteString
        if (UA_Variant_hasScalarType(&eventFields[i], &UA_TYPES[UA_TYPES_BYTESTRING]) && QString(eventFieldList[i]) == "EventId") {
            qdataEvent.eventId = toQVariant(eventFields[i]).toString();
            continue;
        }
        //SourceNode - NodeId
        if (UA_Variant_hasScalarType(&eventFields[i], &UA_TYPES[UA_TYPES_NODEID]) && QString(eventFieldList[i]) == "SourceNode") {
            qdataEvent.sourceNode = toQVariant(eventFields[i]).toString();
            continue;
        }
        //SourceName - String
        if (UA_Variant_hasScalarType(&eventFields[i], &UA_TYPES[UA_TYPES_STRING]) && QString(eventFieldList[i]) == "SourceName") {
            qdataEvent.sourceName = toQVariant(eventFields[i]).toString();
            continue;
        }
        //Message - LocalizedText
        if (UA_Variant_hasScalarType(&eventFields[i], &UA_TYPES[UA_TYPES_LOCALIZEDTEXT]) && QString(eventFieldList[i]) == "Message") {
            qdataEvent.message = toQVariant(eventFields[i]).toString();
            continue;
        }
        //Severity - UInt16
        if (UA_Variant_hasScalarType(&eventFields[i], &UA_TYPES[UA_TYPES_UINT16]) && QString(eventFieldList[i]) == "Severity") {
            qdataEvent.severity = toQVariant(eventFields[i]).toUInt();
            continue;
        }
        //Time - DateTime
        if (UA_Variant_hasScalarType(&eventFields[i], &UA_TYPES[UA_TYPES_DATETIME]) && QString(eventFieldList[i]) == "Time") {
            qdataEvent.time = toQDateTime(*static_cast<UA_DateTime*>(eventFields[i].data));
            continue;
        }
        //ActiveState - LocalizedText
        if (UA_Variant_hasScalarType(&eventFields[i], &UA_TYPES[UA_TYPES_LOCALIZEDTEXT]) && QString(eventFieldList[i]) == "ActiveState") {
            qdataEvent.activeState = toQVariant(eventFields[i]).toString();
            // if (qdataEvent.activeState.indexOf("Active") != -1) qdataEvent.bactiveState = true; //если нашли "Active"
            // if (qdataEvent.activeState.indexOf("Inactive") != -1) qdataEvent.bactiveState = false;
            continue;
        }
        //ActiveState/Id - bool
        if (UA_Variant_hasScalarType(&eventFields[i], &UA_TYPES[UA_TYPES_BOOLEAN]) && QString(eventFieldList[i]) == "ActiveState/Id") {
            qdataEvent.bactiveState = toQVariant(eventFields[i]).toBool();
            continue;
        }
        if (UA_Variant_hasScalarType(&eventFields[i], &UA_TYPES[UA_TYPES_NODEID]) && QString(eventFieldList[i]) == "LimitState/CurrentState/Id") {
            UA_NodeId *nodeId = static_cast<UA_NodeId*>(eventFields[i].data);
            if(!nodeId) continue;
            qdataEvent.currentState = "Unknown";
            static const UA_NodeId NODE_HIGHHIGH = UA_NODEID_NUMERIC(0, UA_NS0ID_EXCLUSIVELIMITSTATEMACHINETYPE_HIGHHIGH);
            static const UA_NodeId NODE_HIGH = UA_NODEID_NUMERIC(0, UA_NS0ID_EXCLUSIVELIMITSTATEMACHINETYPE_HIGH);
            static const UA_NodeId NODE_LOW = UA_NODEID_NUMERIC(0, UA_NS0ID_EXCLUSIVELIMITSTATEMACHINETYPE_LOW);
            static const UA_NodeId NODE_LOWLOW = UA_NODEID_NUMERIC(0, UA_NS0ID_EXCLUSIVELIMITSTATEMACHINETYPE_LOWLOW);
            if (UA_NodeId_equal(nodeId, &NODE_HIGHHIGH)) qdataEvent.currentState = "HighHigh";
            if (UA_NodeId_equal(nodeId, &NODE_HIGH)) qdataEvent.currentState = "High";
            if (UA_NodeId_equal(nodeId, &NODE_LOW)) qdataEvent.currentState = "Low";
            if (UA_NodeId_equal(nodeId, &NODE_LOWLOW)) qdataEvent.currentState = "LowLow";
            continue;
        }
    }
    return qdataEvent;
}

void OPCUAClient::disconnectClient()
{
    if (!pclient) return;

    ptimer->stop();
    preadTimer->stop();

    //чистим хэши
    qDeleteAll(hSubscribeGroupEvent);
    hSubscribeGroupEvent.clear();
    qDeleteAll(hSubscribeGroupNodes);
    hSubscribeGroupNodes.clear();
    subscriptionIds.clear();

    UA_Client_disconnect(pclient);
    UA_ExtensionObject_clear(&UA_Client_getConfig(pclient)->userIdentityToken);
    QString endpointUrl = toQString(UA_Client_getConfig(pclient)->endpointUrl);
    UA_Client_delete(pclient);
    pclient = nullptr;

    freadBusy = false;
    fdisconnectQueued = false;
    emit status(QString("[Info] Disconnect: %1").arg(endpointUrl));
}

void OPCUAClient::processTimer()
{
    if (!pclient) return;
    //обработка асинхронных операций
    UA_Client_run_iterate(pclient, 0);
}

void OPCUAClient::processReadNodeAsync()
{
    if (freadBusy || !pclient || !isConnected(pclient) || !readRequest.nodesToRead) return;

    UA_UInt32 requestId;
    UA_StatusCode retval = UA_Client_sendAsyncReadRequest(pclient, &readRequest, readCallback, &readCtx, &requestId);
    if (retval == UA_STATUSCODE_GOOD) freadBusy = true;
    if (retval != UA_STATUSCODE_GOOD) {
        emit status(QString("[Error] %1 status: %2, requestId: %3").arg(Q_FUNC_INFO, UA_StatusCode_name(retval), QString::number(requestId)));
        return;
    }
}

bool OPCUAClient::getEndpoints(const QString &url, QList<Endpoint> &storeEndpoint)
{
    if (!pclient) pclient = UA_Client_new();
    if (isConnected(pclient)) return true;

    UA_ClientConfig *config = UA_Client_getConfig(pclient);
    UA_ClientConfig_setDefault(config);

    config->timeout = 2000; //время ожидания ответа сервера мс
    config->stateCallback = stateCallback;
    //передаем this в статичную функцию stateCallback(), через контекст (чтобы можно из статичной функции вызывать не статичные функции данного класса)
    config->clientContext = this;

    //Настраиваем логгер
    config->logging->log = loggingCallback;
    config->logging->context = this;

    UA_EndpointDescription *endpoints = nullptr;
    size_t endpointCount = 0;

    UA_StatusCode statusEndpoints = UA_Client_getEndpoints(pclient, url.toUtf8().constData(), &endpointCount, &endpoints);
    if (statusEndpoints != UA_STATUSCODE_GOOD) {
        emit status(QString("[Error] %1 Failed to get endpoints, status: %2").arg(Q_FUNC_INFO, UA_StatusCode_name(statusEndpoints)));
        UA_Client_delete(pclient);
        pclient = nullptr;
        return false;
    }

    //Выведем информацию о всех endpoint'ах для отладки
    Endpoint endpoint;
    for (size_t i = 0; i < endpointCount; i++) {
        storeEndpoint.append(endpoint);
        storeEndpoint.last().endpointUrl = toQString(endpoints[i].endpointUrl);
        storeEndpoint.last().securityPolicy = toQString(endpoints[i].securityPolicyUri);
        storeEndpoint.last().securityMode = endpoints[i].securityMode;
    }

    //проверка поддержки шифрования через условную компиляцию
#ifdef UA_ENABLE_ENCRYPTION
    qDebug() << "✓ Library compiled with encryption support";
#else
    qDebug() << "✗ Library compiled WITHOUT encryption support";
#endif

#ifdef UA_ENABLE_ENCRYPTION_OPENSSL
    qDebug() << "✓ OpenSSL backend enabled";
#elif defined(UA_ENABLE_ENCRYPTION_MBEDTLS)
    qDebug() << "✓ MbedTLS backend enabled";
#else
    qDebug() << "✗ No crypto backend enabled";
#endif

    // Очистка
    UA_Array_delete(endpoints, endpointCount, &UA_TYPES[UA_TYPES_ENDPOINTDESCRIPTION]);
    UA_Client_delete(pclient);
    pclient = nullptr;
    return true;
}

bool OPCUAClient::connectToServerAnonymous(const QString &url)
{
    if (pclient) {
        disconnectFromServer();  // корректно всё чистит
    }
    pclient = UA_Client_new();
    if (isConnected(pclient)) return true;

    UA_ClientConfig *config = UA_Client_getConfig(pclient);
    UA_ClientConfig_setDefault(config);
    config->timeout = 2500; //время ожидания ответа сервера мс
    //передаем this в статичную функцию stateCallback(), через контекст (чтобы можно из статичной функции вызывать не статичные функции данного класса)
    config->stateCallback = stateCallback;
    config->clientContext = this;

    //Настраиваем логгер
    config->logging->log = loggingCallback;
    config->logging->context = this;

    UA_StatusCode statusConnect = UA_Client_connect(pclient, url.toUtf8().constData());
    if (statusConnect != UA_STATUSCODE_GOOD) {
        disconnectFromServer();
        return false;
    }
    emit status(QString("[Info] Connecting to: %1").arg(toQString(UA_Client_getConfig(pclient)->endpointUrl)));
    ptimer->start(); //таймер для обработки асинхронных операций
    preadTimer->start();
    return true;
}

bool OPCUAClient::connectToServerUsername(const QString &url, const Endpoint &endpoint, const QString &name, const QString &pass)
{
    if (pclient) {
        disconnectFromServer();  // корректно всё чистит
    }
    pclient = UA_Client_new();
    if (isConnected(pclient)) return true;

    //Получаем конфиг и настраиваем шифрование
    UA_ClientConfig *config = UA_Client_getConfig(pclient);
    UA_ClientConfig_setDefault(config);
    config->timeout = 2500;
    //Передаем this в статичную функцию stateCallback(), через контекст (чтобы можно из статичной функции вызывать не статичные функции данного класса)
    config->stateCallback = stateCallback;
    config->clientContext = this;

    //Настраиваем логгер
    config->logging->log = loggingCallback;
    config->logging->context = this;

    //Устанавливаем Application URI (ВАЖНО!!! URI настроенного клиента и сертификата клиента должен совпадать!!!)
    UA_String_clear(&config->clientDescription.applicationUri);
    config->clientDescription.applicationUri = UA_STRING_ALLOC(applicationUri.toUtf8().constData());
    config->clientDescription.applicationName.text = UA_STRING_ALLOC(applicationUri.toUtf8().constData());

    //Настраиваем securityMode and securityPolicyUri
    config->securityMode = endpoint.securityMode;
    UA_ByteString_clear(&config->securityPolicyUri);
    config->securityPolicyUri = UA_STRING_ALLOC(endpoint.securityPolicy.toUtf8().constData());

    //Загружаем постоянный клиентский сертификат
    UA_ByteString clientCertificate = loadCertificate("client_cert.der");
    UA_ByteString clientPrivateKey = loadCertificate("client_key.der");
    if  (UA_ByteString_equal(&clientCertificate, &UA_BYTESTRING_NULL) || UA_ByteString_equal(&clientPrivateKey, &UA_BYTESTRING_NULL)) {
        UA_ByteString_clear(&clientCertificate);
        UA_ByteString_clear(&clientPrivateKey);
        emit status(QString("[Error] %1, Failed loadCertificate").arg(Q_FUNC_INFO));
        return false;
    }
    //ВАЖНО: Устанавливаем шифрование ДО подключения
    config->certificateVerification.clear(&config->certificateVerification);
    UA_CertificateVerification_AcceptAll(&config->certificateVerification);
    UA_StatusCode statusConfig = UA_ClientConfig_setDefaultEncryption(config, clientCertificate, clientPrivateKey, NULL, 0, NULL, 0);

    if (statusConfig != UA_STATUSCODE_GOOD) {
        UA_ByteString_clear(&clientCertificate);
        UA_ByteString_clear(&clientPrivateKey);
        emit status(QString("[Error] %1, Failed to configure encryption: %2").arg(Q_FUNC_INFO).arg(UA_StatusCode_name(statusConfig)));
        disconnectFromServer();
        return false;
    }
    //Подключаемся с использованием имени пользователя/пароля
    UA_StatusCode statusConnect = UA_Client_connectUsername(pclient, url.toUtf8().constData(), name.toUtf8().constData(), pass.toUtf8().constData());
    //Очищаем ресурсы
    UA_ByteString_clear(&clientCertificate);
    UA_ByteString_clear(&clientPrivateKey);

    if (statusConnect != UA_STATUSCODE_GOOD) {
        disconnectFromServer();
        return false;
    }
    ptimer->start(); //таймер для обработки асинхронных операций
    preadTimer->start();
    return true;
}

void OPCUAClient::disconnectFromServer()
{
    /* удаляем действующие подписки с коллбэком в лямбда функции, ждем коллбэк чтобы перейти к следующей итерации
     * и отключить клиента, если отключать клиента сразу, то подписка не успевает удалиться на сервере т.к. вызов асинхронный */
    deleteSubscriptionAsync(subscriptionIds, [this](bool succesCallback){
        //этот коллбэк выполнится в потоке Qt (внутри processTimer → run_iterate)!!!
        qDebug() << Q_FUNC_INFO << "succesCallback_deleteSubscription" << succesCallback;

        /* Qt::QueuedConnection гарантирует, что disconnectClient() выполнится после того, как управление вернётся из run_iterate в event loop
         * отключать сразу нельзя, только в очереди, иначе crash */
        QMetaObject::invokeMethod(this, [this]() {
            disconnectClient();
        }, Qt::QueuedConnection);
    });
}

bool OPCUAClient::browseNode(const QString &nodeId)
{
    if (!pclient) return false;
    if (!isConnected(pclient)) return false;

    /* Browse some objects */
    UA_BrowseRequest bReq;
    UA_BrowseRequest_init(&bReq);
    bReq.requestedMaxReferencesPerNode = 0; // 0 = без ограничений
    bReq.nodesToBrowseSize = 1;
    bReq.nodesToBrowse = bReq.nodesToBrowse =  static_cast<UA_BrowseDescription*>(UA_Array_new(bReq.nodesToBrowseSize, &UA_TYPES[UA_TYPES_BROWSEDESCRIPTION]));
    if (!bReq.nodesToBrowse) return false;
    bReq.nodesToBrowse[0].referenceTypeId = UA_NODEID_NUMERIC(0, UA_NS0ID_HIERARCHICALREFERENCES);
    bReq.nodesToBrowse[0].includeSubtypes = UA_TRUE; // Включает HasComponent, HasProperty и др.
    bReq.nodesToBrowse[0].resultMask = UA_BROWSERESULTMASK_ALL; /* return everything */

    UA_NodeId nodeId_copy = toNodeId(nodeId);
    UA_NodeId_copy(&nodeId_copy, &bReq.nodesToBrowse[0].nodeId);
    UA_NodeId_clear(&nodeId_copy);

    //Берем nodeId, если он не корректный, открываем ROOTFOLDER
    if (UA_NodeId_equal(&bReq.nodesToBrowse[0].nodeId, &UA_NODEID_NULL)) {
        bReq.nodesToBrowse[0].nodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_ROOTFOLDER); /* browse root folder */
    }

    UA_BrowseResponse bResp = UA_Client_Service_browse(pclient, bReq);
    if (bResp.responseHeader.serviceResult != UA_STATUSCODE_GOOD) {
        UA_BrowseRequest_clear(&bReq);
        UA_BrowseResponse_clear(&bResp);
        return false;
    }

    QList<NodeOPC> childNodes;
    NodeOPC nodeOPC;

    for (size_t i = 0; i < bResp.resultsSize; ++i) {
        UA_BrowseResult result = bResp.results[i];
        for (size_t j = 0; j < result.referencesSize; ++j) {
            UA_ReferenceDescription *ref = &result.references[j];
            nodeOPC.browseName = toQString(ref->browseName.name);
            nodeOPC.nodeClass = ref->nodeClass;

            //Получаем NodeId, referenceTypeId в читаемом виде QString
            nodeOPC.nodeIdStr = toQString(ref->nodeId.nodeId);
            nodeOPC.referenceTypeId = toQString(ref->referenceTypeId);

            UA_ExpandedNodeId_copy(&ref->nodeId, &nodeOPC.extendedNodeId);
            nodeOPC.displayName = toQString(ref->displayName.text);
            childNodes.append(nodeOPC);
        }
    }

    // emit status(AppLib::LogType::Info, QString("%1, Status:%2, %3").arg(Q_FUNC_INFO, UA_StatusCode_name(bResp.responseHeader.serviceResult), nodeId));
    UA_BrowseRequest_clear(&bReq);
    UA_BrowseResponse_clear(&bResp);
    emit browseCompleted(nodeId, childNodes);
    return true;
}

bool OPCUAClient::readNodeAttributes(const QString &nodeId)
{
    if (!pclient) return false;
    if (!isConnected(pclient)) return false;

    //Создаем запрос для чтения нескольких атрибутов
    UA_ReadRequest rReq;
    UA_ReadRequest_init(&rReq);
    rReq.nodesToReadSize = 13;
    rReq.nodesToRead =  static_cast<UA_ReadValueId*>(UA_Array_new(rReq.nodesToReadSize, &UA_TYPES[UA_TYPES_READVALUEID]));
    if (!rReq.nodesToRead) return false;
    rReq.timestampsToReturn = UA_TIMESTAMPSTORETURN_BOTH; //возвращаем временные метки и сервера и клиента

    //Настраиваем атрибуты для чтения (порядок слудобвания атрибутов в ответе сервера соответствует порядку подготовленного массива)
    UA_AttributeId attributeId[] = {
        UA_ATTRIBUTEID_NODEID,
        UA_ATTRIBUTEID_NODECLASS,
        UA_ATTRIBUTEID_BROWSENAME,
        UA_ATTRIBUTEID_DISPLAYNAME,
        UA_ATTRIBUTEID_DESCRIPTION,     //Описание
        UA_ATTRIBUTEID_DATATYPE,
        UA_ATTRIBUTEID_VALUE,
        UA_ATTRIBUTEID_WRITEMASK,
        UA_ATTRIBUTEID_USERWRITEMASK,
        UA_ATTRIBUTEID_ARRAYDIMENSIONS, //Размерность массива
        UA_ATTRIBUTEID_ACCESSLEVEL,     //Уровень доступа
        UA_ATTRIBUTEID_USERACCESSLEVEL, //Пользовательский уровень доступа
        UA_ATTRIBUTEID_EVENTNOTIFIER    //Поддержка узлом подписки
    };

    UA_NodeId nodeId_copy = toNodeId(nodeId);
    for(size_t i = 0; i < rReq.nodesToReadSize; i++) {
        UA_NodeId_copy(&nodeId_copy, &rReq.nodesToRead[i].nodeId); //nodeId копируем только так (т.к. вся стуктура)!! а не nodeId = nodeId
        rReq.nodesToRead[i].attributeId = attributeId[i];
    }
    UA_NodeId_clear(&nodeId_copy);

    //Формируем запрос на чтение группы параметров
    UA_ReadResponse rResp = UA_Client_Service_read(pclient, rReq);
    if(!UA_StatusCode_isGood(rResp.responseHeader.serviceResult)) {
        UA_ReadRequest_clear(&rReq);
        UA_ReadResponse_clear(&rResp);
        emit status(QString("[Error] %1, status: %2, %3").arg(Q_FUNC_INFO, UA_StatusCode_name(rResp.responseHeader.serviceResult), nodeId));
        return false;
    }

    QList<NodeAttribute> attributes;
    NodeAttribute nodeAttribute;

    for(size_t i = 0; i < rResp.resultsSize; i++) {
        if(UA_StatusCode_isGood(rResp.results[i].status) && rResp.results[i].hasValue) {
            attributes.append(nodeAttribute);
            //чередование атрибутов в запросе соответствует чередованию ответа согласно атрибута
            attributes.last().attributeId = attributeId[i];
            UA_DataValue_copy(&rResp.results[i], &attributes.last().results);
        }
    }

    UA_ReadRequest_clear(&rReq);
    UA_ReadResponse_clear(&rResp);
    emit attributeCompleted(nodeId, attributes); //модель заполняем в цикле
    return true;
}

bool OPCUAClient::createSubscriptionToEvents()
{
    /* Оформляем подписку на сервер UA_NS0ID_SERVER для получения всех сообщений, формируемых сервером
     * метод запросса синхронный, для оформления групповой подписки на конкретные узлы надо использовать
     * асинхронный метод UA_Client_MonitoredItems_createEvents_async с групповыми коллбэками как в SubscriptionGroup */

    if (!pclient || !isConnected(pclient)) return false;

    //Создаем подписку
    UA_CreateSubscriptionRequest subRequest = UA_CreateSubscriptionRequest_default();
    subRequest.requestedPublishingInterval = 1000.0;    //1000 ms
    subRequest.requestedLifetimeCount = 800;            //если клиент 800 раз подряд не подтверждает получение данных → подписка автоматически удаляется
    subRequest.requestedMaxKeepAliveCount = 10;         //количество циклов публикации между keep-alive уведомлениями (если нет данных для отправки сервер каждые 10 циклов отправляет пустое уведомление)
    subRequest.maxNotificationsPerPublish = 850;        //максимальное количество уведомлений в одном пакете публикации
    subRequest.publishingEnabled = true;                //включениа отправка данных
    subRequest.priority = 0;                            //приоритет подписки (0-255) 0 норм приоритет
    UA_CreateSubscriptionResponse subResponse = UA_Client_Subscriptions_create(pclient, subRequest, NULL, NULL, NULL);
    if(subResponse.responseHeader.serviceResult != UA_STATUSCODE_GOOD) {
        UA_CreateSubscriptionResponse_clear(&subResponse);
        return false;
    }

    //Select clauses - choose which event fields to receive
    UA_SimpleAttributeOperand *selectClauses = static_cast<UA_SimpleAttributeOperand*>(UA_Array_new(eventFields.size(), &UA_TYPES[UA_TYPES_SIMPLEATTRIBUTEOPERAND]));
    for(auto i = 0; i < eventFields.size(); i++) {
        UA_SimpleAttributeOperand_init(&selectClauses[i]); //инициализация структуры

        //Парсим путь (вложенные структуры) - разделяем по "/"
        QStringList browsePathList = QString(eventFields[i]).split('/');
        //Создаем browsePath
        selectClauses[i].browsePathSize = browsePathList.size();
        selectClauses[i].browsePath = static_cast<UA_QualifiedName*>(UA_Array_new(browsePathList.size(), &UA_TYPES[UA_TYPES_QUALIFIEDNAME]));
        for(auto j = 0; j < browsePathList.size(); j++) {
            selectClauses[i].browsePath[j] = UA_QUALIFIEDNAME_ALLOC(0, browsePathList[j].toUtf8().constData());
        }
        selectClauses[i].typeDefinitionId = UA_NODEID_NUMERIC(0, UA_NS0ID_BASEEVENTTYPE);
        selectClauses[i].attributeId = UA_ATTRIBUTEID_VALUE;
    }

    //Создаем фильтр событий
    UA_EventFilter *eventFilter = UA_EventFilter_new();
    UA_EventFilter_init(eventFilter);
    eventFilter->selectClauses = selectClauses;
    eventFilter->selectClausesSize = eventFields.size();
    //No where clause - receive all events
    eventFilter->whereClause.elements = nullptr;
    eventFilter->whereClause.elementsSize = 0;

    //Подготавливаем запрос
    UA_MonitoredItemCreateRequest monRequest = UA_MonitoredItemCreateRequest_default(UA_NODEID_NUMERIC(0, UA_NS0ID_SERVER)); //UA_NS0ID_SERVER Server получаем все события сервера
    monRequest.itemToMonitor.attributeId = UA_ATTRIBUTEID_EVENTNOTIFIER;
    monRequest.monitoringMode = UA_MONITORINGMODE_REPORTING;
    monRequest.requestedParameters.samplingInterval = 10.0; //интервал опроса/проверки событий (сервером - источника)
    monRequest.requestedParameters.queueSize = 850;         //размер очереди уведомлений (MAX кол-во, которые ждут отправки)
    monRequest.requestedParameters.discardOldest = true;    //при переполнении очереди удаляем самые старые события
    //Правильная установка фильтра в ExtensionObject
    monRequest.requestedParameters.filter.encoding = UA_EXTENSIONOBJECT_DECODED;
    monRequest.requestedParameters.filter.content.decoded.type = &UA_TYPES[UA_TYPES_EVENTFILTER];
    monRequest.requestedParameters.filter.content.decoded.data = eventFilter;

    UA_MonitoredItemCreateResult monResult = UA_Client_MonitoredItems_createEvent(pclient, subResponse.subscriptionId, UA_TIMESTAMPSTORETURN_BOTH,
                                                                                  monRequest, this, subscribeEventCallback, nullptr);

    //Cleanup
    UA_MonitoredItemCreateRequest_clear(&monRequest); //eventFilter очистится внутри monRequest

    emit status(QString("[Info] %1, status: %2, subscriptionId: %3").arg(Q_FUNC_INFO, UA_StatusCode_name(monResult.statusCode), QString::number(subResponse.subscriptionId)));
    if (monResult.statusCode != UA_STATUSCODE_GOOD) {
        UA_CreateSubscriptionResponse_clear(&subResponse);
        return false;
    }

    //Добавляем подписку в лист с активными подписками
    subscriptionIds.append(subResponse.subscriptionId);
    UA_CreateSubscriptionResponse_clear(&subResponse);
    return true;
}

bool OPCUAClient::createSubscriptionToEventsAsync(const QList<QString> &nodeIds, const UA_Double &publishingInterval)
{
    if (!pclient || !isConnected(pclient) || nodeIds.isEmpty()) return false;

    //Создаем подписку
    UA_CreateSubscriptionRequest subRequest = UA_CreateSubscriptionRequest_default();
    subRequest.requestedPublishingInterval = publishingInterval; //интервал отправления данных клиенту даже если в пакете 0 изменений
    subRequest.requestedLifetimeCount = 1000;           //если клиент 800 раз подряд не подтверждает получение данных → подписка автоматически удаляется
    subRequest.requestedMaxKeepAliveCount = 10;         //количество циклов публикации между keep-alive уведомлениями (если нет данных для отправки сервер каждые 10 циклов отправляет пустое уведомление)
    subRequest.maxNotificationsPerPublish = 850;        //максимальное количество уведомлений в одном пакете публикации
    subRequest.publishingEnabled = true;                //включениа отправка данных
    subRequest.priority = 0;                            //приоритет подписки (0-255) 0 норм приоритет
    UA_CreateSubscriptionResponse subResponse = UA_Client_Subscriptions_create(pclient, subRequest, NULL, NULL, NULL);
    if (subResponse.responseHeader.serviceResult != UA_STATUSCODE_GOOD) {
        UA_CreateSubscriptionResponse_clear(&subResponse);
        return false;
    }
    UA_UInt32 subscriptionId = subResponse.subscriptionId;
    UA_CreateSubscriptionResponse_clear(&subResponse);

    //Select clauses - choose which event fields to receive
    UA_SimpleAttributeOperand *selectClauses = static_cast<UA_SimpleAttributeOperand*>(UA_Array_new(eventFields.size(), &UA_TYPES[UA_TYPES_SIMPLEATTRIBUTEOPERAND]));
    for(auto i = 0; i < eventFields.size(); i++) {
        UA_SimpleAttributeOperand_init(&selectClauses[i]); //инициализация структуры

        //Парсим путь (вложенные структуры) - разделяем по "/"
        QStringList browsePathList = QString(eventFields[i]).split('/');
        //Создаем browsePath
        selectClauses[i].browsePathSize = browsePathList.size();
        selectClauses[i].browsePath = static_cast<UA_QualifiedName*>(UA_Array_new(browsePathList.size(), &UA_TYPES[UA_TYPES_QUALIFIEDNAME]));
        for(auto j = 0; j < browsePathList.size(); j++) {
            selectClauses[i].browsePath[j] = UA_QUALIFIEDNAME_ALLOC(0, browsePathList[j].toUtf8().constData());
        }
        selectClauses[i].typeDefinitionId = UA_NODEID_NUMERIC(0, UA_NS0ID_BASEEVENTTYPE);
        selectClauses[i].attributeId = UA_ATTRIBUTEID_VALUE;
    }

    //Создаем фильтр событий
    UA_EventFilter *filter = UA_EventFilter_new();
    UA_EventFilter_init(filter);
    filter->selectClauses = selectClauses;
    filter->selectClausesSize = eventFields.size();
    //No where clause - receive all events
    filter->whereClause.elements = nullptr;
    filter->whereClause.elementsSize = 0;

    //Cоздаем контекст c массивом полей для выборки для передачи в callback
    SubscribeGroupEvent *subscribeGroup = new SubscribeGroupEvent;
    subscribeGroup->eventFields = eventFields;
    subscribeGroup->nodeIds = nodeIds;
    subscribeGroup->subscriptionId = subscriptionId;
    subscribeGroup->monitoredIdsSize = nodeIds.size();
    if (nodeIds.size() > 0) {
        subscribeGroup->monitoredIds = static_cast<UA_UInt32*>(UA_calloc(nodeIds.size(), sizeof(UA_UInt32))); //UA_calloc - выделяем инициализированную 0 память
        subscribeGroup->contexts =  static_cast<void**>(UA_calloc(nodeIds.size(), sizeof(void*)));
        subscribeGroup->eventCallbacks = static_cast<UA_Client_EventNotificationCallback*>(UA_calloc(nodeIds.size(), sizeof(UA_Client_EventNotificationCallback)));
        subscribeGroup->deleteCallbacks = static_cast<UA_Client_DeleteMonitoredItemCallback*>(UA_calloc(nodeIds.size(), sizeof(UA_Client_DeleteMonitoredItemCallback)));
    }

    //Заполняем запросы
    UA_MonitoredItemCreateRequest *monRequests = static_cast<UA_MonitoredItemCreateRequest*>(UA_Array_new(nodeIds.size(), &UA_TYPES[UA_TYPES_MONITOREDITEMCREATEREQUEST]));
    for (size_t i = 0; i < nodeIds.size(); i++) {
        UA_MonitoredItemCreateRequest_init(&monRequests[i]);
        monRequests[i] = UA_MonitoredItemCreateRequest_default(toNodeId(nodeIds[i]));
        monRequests[i].itemToMonitor.attributeId = UA_ATTRIBUTEID_EVENTNOTIFIER;
        monRequests[i].monitoringMode = UA_MONITORINGMODE_REPORTING;
        monRequests[i].requestedParameters.samplingInterval = (publishingInterval >= 200) ? publishingInterval - 100 : 100; //интервал опроса/проверки событий (сервером - источника)
        monRequests[i].requestedParameters.queueSize = 850;          //размер очереди уведомлений (MAX кол-во, которые ждут отправки)
        monRequests[i].requestedParameters.discardOldest = true;     //при переполнении очереди удаляем самые старые события

        //Копируем фильтр для каждого запроса
        UA_EventFilter *filterCopy = UA_EventFilter_new();
        UA_EventFilter_copy(filter, filterCopy);

        monRequests[i].requestedParameters.filter.encoding = UA_EXTENSIONOBJECT_DECODED;
        monRequests[i].requestedParameters.filter.content.decoded.type = &UA_TYPES[UA_TYPES_EVENTFILTER];
        monRequests[i].requestedParameters.filter.content.decoded.data = filterCopy;

        //Передаем индекс как контекст, чтобы знать какой элемент обновлять
        ContextEvent *ctx = new ContextEvent{subscribeGroup, nodeIds[i], i};

        //Устанавливаем указатель на контекст для каждого monitored item
        if (!subscribeGroup->contexts || !subscribeGroup->eventCallbacks || !subscribeGroup->deleteCallbacks) {
            delete ctx;
            ctx = nullptr;
            return false;
        };
        subscribeGroup->contexts[i] = ctx;
        subscribeGroup->eventCallbacks[i] = subscribeEventCallback; //yстанавливаем callback для каждого monitored item
        subscribeGroup->deleteCallbacks[i] = deleteNodesCallback;
    }

    //Создаем структуру запроса
    UA_CreateMonitoredItemsRequest createRequest;
    UA_CreateMonitoredItemsRequest_init(&createRequest);
    createRequest.subscriptionId = subscriptionId;
    createRequest.timestampsToReturn = UA_TIMESTAMPSTORETURN_BOTH;
    createRequest.itemsToCreateSize = nodeIds.size();
    createRequest.itemsToCreate = monRequests;

    subscribeGroup->filter = filter;
    subscribeGroup->monRequests = monRequests;
    filter = nullptr;  //после того как создаем, обнулям т.к. объектом владеет уже ctx (во избежании висячих указателей)
    monRequests = nullptr;

    /* callback-функция выполняется при завершении асинхронного создания UA_Client_MonitoredItems_createEvents_async */
    static auto createCallback = [](UA_Client *client, void *userdata, UA_UInt32 requestId, void *response) {
        Q_UNUSED(requestId);
        SubscribeGroupEvent *subscribeGroup = static_cast<SubscribeGroupEvent*>(userdata); //получаем доступ к subGroup, переданному через void *userdata
        OPCUAClient *self = static_cast<OPCUAClient*>(UA_Client_getContext(client));
        UA_CreateMonitoredItemsResponse *resp =  static_cast<UA_CreateMonitoredItemsResponse*>(response);
        if (!subscribeGroup || !self || !resp) {
            UA_CreateMonitoredItemsResponse_clear(resp);
            return;
        };
        subscribeGroup->monitoredIdsSize = resp->resultsSize;
        for (auto i = 0; i < subscribeGroup->monitoredIdsSize; i++) {
            subscribeGroup->monitoredIds[i] = resp->results[i].monitoredItemId; //записываем вектор monitoredItemId для подписанных узлов
            if (resp->results[i].statusCode != UA_STATUSCODE_GOOD) { //если хотя бы один monitoredId не создался
                emit self->status(QString("[Error] %1 status: %2, monitoredItemId: %3, nodeId: %4").arg(Q_FUNC_INFO,
                                                                                                        UA_StatusCode_name(resp->results[i].statusCode),
                                                                                                        QString::number(subscribeGroup->monitoredIds[i]),
                                                                                                        subscribeGroup->nodeIds[i]));
            };
        }
        if(resp->responseHeader.serviceResult == UA_STATUSCODE_GOOD) {
            //Эмитируем сигнал через QMetaObject, так как это статическая функция
            //Подписка создана, добавляем ее в листк активных подписок и в хэш
            self->subscriptionIds.append(subscribeGroup->subscriptionId);
            self->hSubscribeGroupEvent.insert(subscribeGroup->subscriptionId, subscribeGroup);
            emit self->status(QString("[Success] %1 subscriptionId: %2, items: %3").arg(Q_FUNC_INFO,
                                                                                        QString::number(subscribeGroup->subscriptionId),
                                                                                        QString::number(resp->resultsSize)));
        } else {
            //1.удаляем ContextNodes объекты
            if (subscribeGroup->contexts) {
                for (auto i = 0; i < subscribeGroup->monitoredIdsSize; i++) {
                    if (subscribeGroup->contexts[i]) {
                        ContextEvent *ctx = static_cast<ContextEvent*>(subscribeGroup->contexts[i]);
                        delete ctx;
                        ctx = nullptr;
                    }
                }
            }
            //2.удаляем саму подписку
            delete subscribeGroup; //все ресурсы очищаем в деструкторе ~subscribeGroup()
            subscribeGroup = nullptr;
            emit self->status(QString("[Error] %1 status: %2, size: %3").arg(Q_FUNC_INFO,
                                                                             UA_StatusCode_name(resp->responseHeader.serviceResult),
                                                                             QString::number(resp->resultsSize)));
        }
        if (subscribeGroup && subscribeGroup->filter && subscribeGroup->monRequests) {
            self->cleanupResourcesSubscribeAsync(subscribeGroup->filter, subscribeGroup->monRequests, subscribeGroup->monitoredIdsSize);
        }
        UA_CreateMonitoredItemsResponse_clear(resp);
    };

    UA_UInt32 requestId = 0;
    UA_StatusCode retval = UA_Client_MonitoredItems_createEvents_async(pclient, createRequest, subscribeGroup->contexts, subscribeGroup->eventCallbacks,
                                                                       subscribeGroup->deleteCallbacks, createCallback, subscribeGroup, &requestId);

    if (retval != UA_STATUSCODE_GOOD) {  //обработка ошибок в loggingCallback
        //1.удаляем ContextNodes объекты
        if (subscribeGroup->contexts) {
            for (auto i = 0; i < subscribeGroup->monitoredIdsSize; i++) {
                if (subscribeGroup->contexts[i]) {
                    ContextEvent *ctx = static_cast<ContextEvent*>(subscribeGroup->contexts[i]);
                    delete ctx;
                    ctx = nullptr;
                }
            }
        }
        //2.удаляем саму подписку
        delete subscribeGroup; //все ресурсы очищаем в деструкторе ~subscribeGroup()
        subscribeGroup = nullptr;
        return false;
    }
    return true;
}

bool OPCUAClient::createSubscriptionToNodesAsync(const QList<QString> &nodeIds, const UA_Double &publishingInterval)
{
    if (!pclient || !isConnected(pclient) || nodeIds.isEmpty()) return false;

    //Создаем подписку
    UA_CreateSubscriptionRequest subRequest = UA_CreateSubscriptionRequest_default();
    subRequest.requestedPublishingInterval = publishingInterval; //интервал отправления данных клиенту даже если в пакете 0 изменений
    subRequest.requestedLifetimeCount = 1000;           //если клиент 800 раз подряд не подтверждает получение данных → подписка автоматически удаляется
    subRequest.requestedMaxKeepAliveCount = 10;         //количество циклов публикации между keep-alive уведомлениями (если нет данных для отправки сервер каждые 10 циклов отправляет пустое уведомление)
    subRequest.maxNotificationsPerPublish = 850;        //максимальное количество уведомлений в одном пакете публикации
    subRequest.publishingEnabled = true;                //включениа отправка данных
    subRequest.priority = 0;                            //приоритет подписки (0-255) 0 норм приоритет
    UA_CreateSubscriptionResponse subResponse = UA_Client_Subscriptions_create(pclient, subRequest, NULL, NULL, NULL);
    if(subResponse.responseHeader.serviceResult != UA_STATUSCODE_GOOD) {
        UA_CreateSubscriptionResponse_clear(&subResponse);
        return false;
    }
    UA_UInt32 subscriptionId = subResponse.subscriptionId;
    UA_CreateSubscriptionResponse_clear(&subResponse);

    //Настраиваем фильтр по изменению
    UA_DataChangeFilter *filter = UA_DataChangeFilter_new();
    UA_DataChangeFilter_init(filter);
    filter->trigger = UA_DATACHANGETRIGGER_STATUSVALUE;
    filter->deadbandType = UA_DEADBANDTYPE_NONE;
    filter->deadbandValue = 0.0; //% соотношение изменения мертвой зоны для UA_DEADBANDTYPE_PERCENT

    //Cоздаем контекст c массивом полей для передачи в callback
    SubscribeGroupNodes *subscribeGroup = new SubscribeGroupNodes;
    subscribeGroup->nodeIds = nodeIds;
    subscribeGroup->subscriptionId = subscriptionId;
    subscribeGroup->monitoredIdsSize = nodeIds.size();
    if (nodeIds.size() > 0) {
        subscribeGroup->monitoredIds = static_cast<UA_UInt32*>(UA_calloc(nodeIds.size(), sizeof(UA_UInt32))); //UA_calloc - выделяем инициализированную 0 память
        subscribeGroup->contexts =  static_cast<void**>(UA_calloc(nodeIds.size(), sizeof(void*)));
        subscribeGroup->dataChangeCallbacks = static_cast<UA_Client_DataChangeNotificationCallback*>(UA_calloc(nodeIds.size(), sizeof(UA_Client_DataChangeNotificationCallback)));
        subscribeGroup->deleteCallbacks = static_cast<UA_Client_DeleteMonitoredItemCallback*>(UA_calloc(nodeIds.size(), sizeof(UA_Client_DeleteMonitoredItemCallback)));
    }

    //Заполняем запросы
    UA_MonitoredItemCreateRequest *monRequests = static_cast<UA_MonitoredItemCreateRequest*>(UA_Array_new(nodeIds.size(), &UA_TYPES[UA_TYPES_MONITOREDITEMCREATEREQUEST]));
    for (size_t i = 0; i < nodeIds.size(); i++) {
        UA_MonitoredItemCreateRequest_init(&monRequests[i]);
        monRequests[i] = UA_MonitoredItemCreateRequest_default(toNodeId(nodeIds[i]));
        monRequests[i].itemToMonitor.attributeId = UA_ATTRIBUTEID_VALUE;
        monRequests[i].monitoringMode = UA_MONITORINGMODE_REPORTING;
        monRequests[i].requestedParameters.samplingInterval = (publishingInterval >= 200) ? publishingInterval - 100 : 100;   //интервал опроса/проверки событий (сервером - источника)
        monRequests[i].requestedParameters.queueSize = 10;           //размер очереди уведомлений (MAX кол-во, которые ждут отправки)
        monRequests[i].requestedParameters.discardOldest = true;     //при переполнении очереди удаляем самые старые события

        //Копируем фильтр для каждого запроса
        UA_DataChangeFilter *filterCopy = UA_DataChangeFilter_new();
        UA_DataChangeFilter_copy(filter, filterCopy);

        monRequests[i].requestedParameters.filter.encoding = UA_EXTENSIONOBJECT_DECODED;
        monRequests[i].requestedParameters.filter.content.decoded.type = &UA_TYPES[UA_TYPES_DATACHANGEFILTER];
        monRequests[i].requestedParameters.filter.content.decoded.data = filterCopy;

        //Передаем индекс как контекст, чтобы знать какой элемент обновлять
        ContextNodes *ctx = new ContextNodes{subscribeGroup, nodeIds[i], i};

        //Устанавливаем указатель на контекст для каждого monitored item
        if (!subscribeGroup->contexts || !subscribeGroup->dataChangeCallbacks || !subscribeGroup->deleteCallbacks) {
            delete ctx;
            ctx = nullptr;
            return false;
        };
        subscribeGroup->contexts[i] = ctx;
        subscribeGroup->dataChangeCallbacks[i] = subscribeNodesCallback; //yстанавливаем callback для каждого monitored item
        subscribeGroup->deleteCallbacks[i] = deleteNodesCallback;
    }

    //Создаем структуру запроса
    UA_CreateMonitoredItemsRequest createRequest;
    UA_CreateMonitoredItemsRequest_init(&createRequest);
    createRequest.subscriptionId = subscriptionId;
    createRequest.timestampsToReturn = UA_TIMESTAMPSTORETURN_BOTH;
    createRequest.itemsToCreateSize = nodeIds.size();
    createRequest.itemsToCreate = monRequests;

    subscribeGroup->filter = filter;
    subscribeGroup->monRequests = monRequests;
    filter = nullptr;  //после того как создаем, обнулям т.к. объектом владеет уже ctx (во избежании висячих указателей)
    monRequests = nullptr;

    /* callback-функция выполняется при завершении асинхронного создания UA_Client_MonitoredItems_createDataChanges_async */
    static auto createCallback = [](UA_Client *client, void *userdata, UA_UInt32 requestId, void *response) {
        Q_UNUSED(requestId);
        SubscribeGroupNodes *subscribeGroup = static_cast<SubscribeGroupNodes*>(userdata); //получаем доступ к subGroup, переданному через void *userdata
        OPCUAClient *self = static_cast<OPCUAClient*>(UA_Client_getContext(client));
        UA_CreateMonitoredItemsResponse *resp =  static_cast<UA_CreateMonitoredItemsResponse*>(response);
        if (!subscribeGroup || !self || !resp) {
            UA_CreateMonitoredItemsResponse_clear(resp);
            return;
        };
        subscribeGroup->monitoredIdsSize = resp->resultsSize;
        for (auto i = 0; i < subscribeGroup->monitoredIdsSize; i++) {
            subscribeGroup->monitoredIds[i] = resp->results[i].monitoredItemId; //записываем вектор monitoredItemId для подписанных узлов
            if (resp->results[i].statusCode != UA_STATUSCODE_GOOD) { //если хотя бы один monitoredId не создался
                emit self->status(QString("[Error] %1 status: %2, monitoredItemId: %3, nodeId: %4").arg(Q_FUNC_INFO,
                                                                                                        UA_StatusCode_name(resp->results[i].statusCode),
                                                                                                        QString::number(subscribeGroup->monitoredIds[i]),
                                                                                                        subscribeGroup->nodeIds[i]));
            };
        }
        if(resp->responseHeader.serviceResult == UA_STATUSCODE_GOOD) {
            //Эмитируем сигнал через QMetaObject, так как это статическая функция
            //Подписка создана, добавляем ее в листк активных подписок и в хэш
            self->subscriptionIds.append(subscribeGroup->subscriptionId);
            self->hSubscribeGroupNodes.insert(subscribeGroup->subscriptionId, subscribeGroup);
            emit self->status(QString("[Success] %1 subscriptionId: %2, items: %3").arg(Q_FUNC_INFO,
                                                                                        QString::number(subscribeGroup->subscriptionId),
                                                                                        QString::number(resp->resultsSize)));
        } else {
            //1.удаляем ContextNodes объекты
            if (subscribeGroup->contexts) {
                for (auto i = 0; i < subscribeGroup->monitoredIdsSize; i++) {
                    if (subscribeGroup->contexts[i]) {
                        ContextNodes *ctx = static_cast<ContextNodes*>(subscribeGroup->contexts[i]);
                        delete ctx;
                        ctx = nullptr;
                    }
                }
            }
            //2.удаляем саму подписку
            delete subscribeGroup; //все ресурсы очищаем в деструкторе ~subscribeGroup()
            subscribeGroup = nullptr;
            emit self->status(QString("[Error] %1 status:%2, size: %3").arg(Q_FUNC_INFO,
                                                                            UA_StatusCode_name(resp->responseHeader.serviceResult),
                                                                            QString::number(resp->resultsSize)));
        }
        if (subscribeGroup && subscribeGroup->monRequests) {
            self->cleanupResourcesSubscribeAsync(subscribeGroup->filter, subscribeGroup->monRequests, subscribeGroup->monitoredIdsSize);
        }
        UA_CreateMonitoredItemsResponse_clear(resp);
    };

    UA_UInt32 requestId = 0;
    UA_StatusCode retval = UA_Client_MonitoredItems_createDataChanges_async(pclient, createRequest, subscribeGroup->contexts,
                                                                            subscribeGroup->dataChangeCallbacks,
                                                                            subscribeGroup->deleteCallbacks,
                                                                            createCallback, subscribeGroup, &requestId);

    if (retval != UA_STATUSCODE_GOOD) {  //обработка ошибок в loggingCallback
        //1.удаляем ContextNodes объекты
        if (subscribeGroup->contexts) {
            for (auto i = 0; i < subscribeGroup->monitoredIdsSize; i++) {
                if (subscribeGroup->contexts[i]) {
                    ContextNodes *ctx = static_cast<ContextNodes*>(subscribeGroup->contexts[i]);
                    delete ctx;
                    ctx = nullptr;
                }
            }
        }
        //2.удаляем саму подписку
        delete subscribeGroup; //все ресурсы очищаем в деструкторе ~subscribeGroup()
        subscribeGroup = nullptr;
        return false;
    }
    return true;
}

bool OPCUAClient::deleteSubscriptionItemAsync(SubscribeGroupNodes *subscribeGroup)
{
    if (!pclient || !isConnected(pclient) || !subscribeGroup) return false;
    if (subscribeGroup->subscriptionId == 0 || subscribeGroup->monitoredIdsSize == 0) return true;

    //Подготавливаем запрос на удаление monitored items
    UA_DeleteMonitoredItemsRequest delMonReq;
    UA_DeleteMonitoredItemsRequest_init(&delMonReq);
    delMonReq.subscriptionId = subscribeGroup->subscriptionId;
    delMonReq.monitoredItemIdsSize = subscribeGroup->monitoredIdsSize;
    delMonReq.monitoredItemIds =  static_cast<UA_UInt32*>(UA_Array_new(delMonReq.monitoredItemIdsSize, &UA_TYPES[UA_TYPES_UINT32]));
    if (!delMonReq.monitoredItemIds) return false;
    UA_UInt32_copy(subscribeGroup->monitoredIds, delMonReq.monitoredItemIds);

    /* callback-функция выполняется при завершении асинхронного удаления UA_Client_MonitoredItems_delete_async */
    static auto deleteMonItemsCallback = [](UA_Client *client, void *userdata, UA_UInt32 requestId, void *response) {
        SubscribeGroupNodes *subscribeGroup = static_cast<SubscribeGroupNodes*>(userdata); //получаем доступ к subGroup, переданному через void *userdata
        OPCUAClient *self = static_cast<OPCUAClient*>(UA_Client_getContext(client));  //Эмитируем сигнал через QMetaObject
        UA_DeleteMonitoredItemsResponse *resp = (UA_DeleteMonitoredItemsResponse*)response;
        if (!subscribeGroup || !self || !resp) return;

        // Проверяем статусы удаления каждого monitored item
        for (size_t i = 0; i < resp->resultsSize; i++) {
            if (resp->results[i] != UA_STATUSCODE_GOOD) {
                break;
            }
        }
        if (resp->responseHeader.serviceResult == UA_STATUSCODE_GOOD) {
            //Очищаем monitored items данные
            subscribeGroup->monitoredIdsSize = 0;
            emit self->status(QString("[Success] Monitored items deleted: %1, subscriptionId: %2, requestId: %3").arg(resp->resultsSize).arg(subscribeGroup->subscriptionId).arg(requestId));
            //Теперь удаляем саму подписку
            // self->deleteSubscriptionAsync(ctx->subGroup, ctx->callback);
        }
        UA_free(userdata);
        UA_DeleteMonitoredItemsResponse_clear(resp);
    };

    UA_UInt32 requestId;
    UA_StatusCode retval = UA_Client_MonitoredItems_delete_async(pclient, delMonReq, deleteMonItemsCallback, subscribeGroup, &requestId);

    UA_Array_delete(delMonReq.monitoredItemIds, delMonReq.monitoredItemIdsSize, &UA_TYPES[UA_TYPES_UINT32]);
    if (retval != UA_STATUSCODE_GOOD) {
        return false;
    }
    return true;
}

bool OPCUAClient::deleteSubscriptionAsync(QList<UA_UInt32> &subscriptionIds, std::function<void (bool)> succesCallback) {
    if (!pclient || !isConnected(pclient) || subscriptionIds.isEmpty()) return false;

    UA_DeleteSubscriptionsRequest delSubReq;
    UA_DeleteSubscriptionsRequest_init(&delSubReq);
    delSubReq.subscriptionIdsSize = subscriptionIds.size();
    delSubReq.subscriptionIds =  static_cast<UA_UInt32*>(UA_Array_new(delSubReq.subscriptionIdsSize, &UA_TYPES[UA_TYPES_UINT32]));
    if (!delSubReq.subscriptionIds) return false;
    for (auto i = 0; i < delSubReq.subscriptionIdsSize; i++) {
        delSubReq.subscriptionIds[i] = subscriptionIds[i];
    }

    //Выделяем память для UserData, который будет передан в callback
    struct UserData {
        QList<UA_UInt32> subscriptionIds;
        std::function<void(bool)> succesCallback;
    };
    UserData *userData = new UserData{subscriptionIds, succesCallback};

    /* callback-функция выполняется при завершении асинхронного создания UA_Client_Subscriptions_delete_async */
    static auto deleteSubscriptionCallback = [](UA_Client *client, void *userdata, UA_UInt32 requestId, void *response) {
        Q_UNUSED(requestId);
        UserData *userData = static_cast<UserData*>(userdata);
        OPCUAClient *self = static_cast<OPCUAClient*>(UA_Client_getContext(client));
        UA_DeleteSubscriptionsResponse *resp = static_cast<UA_DeleteSubscriptionsResponse*>(response);
        if (!userData || !resp|| !self) {
            UA_DeleteSubscriptionsResponse_clear(resp);
            return;
        };

        if (resp->responseHeader.serviceResult == UA_STATUSCODE_GOOD) {
            for (auto i = 0; i < resp->resultsSize; i++) {
                /* автозаполнение userData->subscriptionIds[i] "->" тут не работает, если в структуре есть функция std::function<void(bool)> succes */
                self->cleanupHashContexts(userData->subscriptionIds[i]); //удаляем подписку из хэша
                emit self->status(QString("[Info] %1, subscriptionId: %2, status: %3, i: %4").arg(Q_FUNC_INFO,
                                                                                                  QString::number(userData->subscriptionIds[i]),
                                                                                                  UA_StatusCode_name(resp->results[i]),
                                                                                                  QString::number(i)));
            }
            self->subscriptionIds.clear(); //очищаем лист подписок на удаление
            self->subscriptionIds.squeeze();
        }
        userData->succesCallback(resp->responseHeader.serviceResult == UA_STATUSCODE_GOOD); //коллбэк метода deleteSubscriptionAsync
        //Очищаем память
        delete userData;
        userData = nullptr;
        UA_DeleteSubscriptionsResponse_clear(resp);
    };

    UA_UInt32 requestId;
    UA_StatusCode retval = UA_Client_Subscriptions_delete_async(pclient, delSubReq, deleteSubscriptionCallback,
                                                                userData, &requestId);
    //Очищаем память
    UA_Array_delete(delSubReq.subscriptionIds, 1, &UA_TYPES[UA_TYPES_UINT32]);
    if (retval != UA_STATUSCODE_GOOD) {
        delete userData;
        userData = nullptr;
        succesCallback(false);
        return false;
    }
    return true;
}

bool OPCUAClient::createReadRequest(const QList<QString> &nodeIds)
{
    if (nodeIds.isEmpty()) {
        UA_ReadRequest_clear(&readRequest);
        emit status(QString("[Warning] %1, Fault nodeIds.isEmpty()").arg(Q_FUNC_INFO));
        return false;
    }

    //создаем контект для передачи через userdate в callback функции чтения
    readCtx.qdataRWs.clear();

    //Если запрос на чтение уже подготовлен, вначале очищаем старый
    UA_ReadRequest_clear(&readRequest);
    readRequest.nodesToReadSize = nodeIds.size();
    readRequest.nodesToRead =  static_cast<UA_ReadValueId*>(UA_Array_new(readRequest.nodesToReadSize, &UA_TYPES[UA_TYPES_READVALUEID]));
    if (!readRequest.nodesToRead) return false;
    readRequest.timestampsToReturn = UA_TIMESTAMPSTORETURN_BOTH; //возвращаем временные метки и сервера и клиента

    //Заполняем массив узлов для чтения
    for (size_t i = 0; i < nodeIds.size(); i++) {
        UA_ReadValueId_init(&readRequest.nodesToRead[i]);
        UA_NodeId nodeId_copy = toNodeId(nodeIds[i]);
        UA_NodeId_copy(&nodeId_copy, &readRequest.nodesToRead[i].nodeId);
        UA_NodeId_clear(&nodeId_copy);
        readRequest.nodesToRead[i].attributeId = UA_ATTRIBUTEID_VALUE;

        //Копируем NodeIds для контекста
        QDataRW qdataRW;
        qdataRW.nodeId = nodeIds[i];
        readCtx.qdataRWs.push_back(qdataRW);
    }
    emit status(QString("[Info] %1, nodeIds: %2").arg(Q_FUNC_INFO, QString::number(readRequest.nodesToReadSize)));
    return true;
}

bool OPCUAClient::writeNodeAsync(const QList<QDataRW> &qdataRWs)
{
    if (!pclient || !isConnected(pclient)) return false;
    if (qdataRWs.empty()) return false;

    //Подготавливаем запрос на запись
    UA_WriteRequest request;
    UA_WriteRequest_init(&request);
    request.nodesToWriteSize = qdataRWs.size();
    request.nodesToWrite =  static_cast<UA_WriteValue*>(UA_Array_new(request.nodesToWriteSize, &UA_TYPES[UA_TYPES_WRITEVALUE]));
    if (!request.nodesToWrite) return false;

    //Создаем контекст для передачи в callback
    ContextRW *context = new ContextRW;

    //Заполняем массив узлов для записи
    for (size_t i = 0; i < qdataRWs.size(); i++) {
        UA_WriteValue_init(&request.nodesToWrite[i]);
        UA_NodeId nodeId_copy = toNodeId(qdataRWs[i].nodeId);
        UA_NodeId_copy(&nodeId_copy, &request.nodesToWrite[i].nodeId);
        UA_NodeId_clear(&nodeId_copy);
        request.nodesToWrite[i].attributeId = UA_ATTRIBUTEID_VALUE;

        UA_Variant var_copy = toUAVariant(qdataRWs[i].variant);
        UA_Variant_copy(&var_copy, &request.nodesToWrite[i].value.value);
        UA_Variant_clear(&var_copy);
        request.nodesToWrite[i].value.hasValue = !UA_Variant_isEmpty(&request.nodesToWrite[i].value.value);  //true говорим что есть значение для записи

        //Копируем NodeIds и значения для контекста
        QDataRW qdataRW;
        qdataRW.nodeId = qdataRWs[i].nodeId;
        qdataRW.variant = qdataRWs[i].variant;
        context->qdataRWs.push_back(qdataRW);
    }

    UA_UInt32 requestId;
    UA_StatusCode retval = UA_Client_sendAsyncWriteRequest(pclient, &request, writeCallback, context, &requestId);

    //Очищаем временные ресурсы запроса (1.освобождаем внутренние ресурсы каждого элемента, 2.освобождаем память самого массива)
    for (size_t i = 0; i < request.nodesToWriteSize; i++) {
        UA_WriteValue_clear(&request.nodesToWrite[i]);
    }
    UA_Array_delete(request.nodesToWrite, request.nodesToWriteSize, &UA_TYPES[UA_TYPES_WRITEVALUE]);
    if (retval != UA_STATUSCODE_GOOD) {
        emit status(QString("[Error] %1, status: %2").arg(Q_FUNC_INFO, UA_StatusCode_name(retval)));
        delete context; //context - элементы массива очистятся в деструкторе
        context = nullptr;
        return false;
    }
    return true;
}

bool OPCUAClient::readHistoryEvents(const QString &nodeId, const QDateTime &startTime, const QDateTime &endTime, const UA_UInt32 &maxValues)
{
    /* поддерживает чтение исторических данных узла, если узел поддерживает HistoryRead */
    if (!pclient || !isConnected(pclient)) return false;
    UA_NodeId uanodeId = toNodeId(nodeId);

    //Select clauses - choose which event fields to receive
    UA_SimpleAttributeOperand *selectClauses = static_cast<UA_SimpleAttributeOperand*>(UA_Array_new(eventFields.size(), &UA_TYPES[UA_TYPES_SIMPLEATTRIBUTEOPERAND]));
    for(auto i = 0; i < eventFields.size(); i++) {
        UA_SimpleAttributeOperand_init(&selectClauses[i]); //инициализация структуры

        //Парсим путь (вложенные структуры) - разделяем по "/"
        QStringList browsePathList = QString(eventFields[i]).split('/');
        //Создаем browsePath
        selectClauses[i].browsePathSize = browsePathList.size();
        selectClauses[i].browsePath = static_cast<UA_QualifiedName*>(UA_Array_new(browsePathList.size(), &UA_TYPES[UA_TYPES_QUALIFIEDNAME]));
        for(auto j = 0; j < browsePathList.size(); j++) {
            selectClauses[i].browsePath[j] = UA_QUALIFIEDNAME_ALLOC(0, browsePathList[j].toUtf8().constData());
        }
        selectClauses[i].typeDefinitionId = UA_NODEID_NUMERIC(0, UA_NS0ID_BASEEVENTTYPE);
        selectClauses[i].attributeId = UA_ATTRIBUTEID_VALUE;
    }

    //Создаем фильтр событий
    UA_EventFilter filter;
    UA_EventFilter_init(&filter);
    filter.selectClauses = selectClauses;
    filter.selectClausesSize = eventFields.size();
    //No where clause - receive all events
    filter.whereClause.elements = nullptr;
    filter.whereClause.elementsSize = 0;

    UA_StatusCode retval = UA_Client_HistoryRead_events(pclient, &uanodeId, historicalEventCallback, toUADateTime(startTime), toUADateTime(endTime),
                                                        UA_STRING_NULL, //все элементы
                                                        filter, maxValues, UA_TIMESTAMPSTORETURN_BOTH, NULL);
    UA_EventFilter_clear(&filter);
    if(retval != UA_STATUSCODE_GOOD) {
        emit status(QString("[Error] %1, status:%2 nodeId: %3").arg(Q_FUNC_INFO, UA_StatusCode_name(retval), nodeId));
        return false;
    }
    return true;
}

bool OPCUAClient::readHistoryData(const QString &nodeId, const QDateTime &startTime, const QDateTime &endTime, const UA_UInt32 &maxValues)
{
    /* поддерживает чтение исторических данных узла, если узел поддерживает HistoryRead */
    if (!pclient || !isConnected(pclient)) return false;
    UA_NodeId uanodeId = toNodeId(nodeId);

    UA_StatusCode retval = UA_Client_HistoryRead_raw(pclient, &uanodeId, historicalDataCallback, toUADateTime(startTime), toUADateTime(endTime),
                                                     UA_STRING_NULL,//все элементы
                                                     UA_TRUE,       //граничащие элементы так же передаем
                                                     maxValues, UA_TIMESTAMPSTORETURN_BOTH, NULL);

    if(retval != UA_STATUSCODE_GOOD) {
        emit status(QString("[Error] %1, status:%2 nodeId: %3").arg(Q_FUNC_INFO, UA_StatusCode_name(retval), nodeId));
        return false;
    }
    return true;
}

bool OPCUAClient::saveCertificatesToFile()
{
    //Генерируем сертификат клиента
    UA_ByteString clientCertificate = UA_STRING_NULL;
    UA_ByteString clientPrivateKey = UA_STRING_NULL;

    if (generateSelfSignedCertificate(clientCertificate, clientPrivateKey)) {
        saveCertificate("client_cert.der", reinterpret_cast<const char*>(clientCertificate.data), reinterpret_cast<const size_t>(clientCertificate.length));
        saveCertificate("client_key.der", reinterpret_cast<const char*>(clientPrivateKey.data), reinterpret_cast<const size_t>(clientPrivateKey.length));
        return true;
    }
    return false;
}

const QString OPCUAClient::getCertDir() const
{
    return QDir::currentPath() + "/cert/";
}

const QString OPCUAClient::getVersion() const
{
    return (QString("open62541: %1.%2.%3")
                .arg(UA_OPEN62541_VER_MAJOR)
                .arg(UA_OPEN62541_VER_MINOR)
                .arg(UA_OPEN62541_VER_PATCH));
}

const QList<UA_UInt32> &OPCUAClient::getSubscriptionIds() const
{
    return subscriptionIds;
}

QList<UA_UInt32> &OPCUAClient::getSubscriptionIds()
{
    return subscriptionIds;
}

QString OPCUAClient::toQString(const UA_String &uaStr)
{
    if (uaStr.data == nullptr) return QString();
    //Преобразуем байты в QString используя fromUtf8
    QByteArrayView byteView(reinterpret_cast<const char*>(uaStr.data), uaStr.length);
    return QString::fromUtf8(byteView);
}

QString OPCUAClient::toQString(const UA_NodeId &nodeId)
{
    if (UA_NodeId_isNull(&nodeId)) return QString();

    //Получаем NodeId в читаемом виде UA_String
    UA_String uaStr = UA_STRING_NULL;
    UA_NodeId_print(&nodeId, &uaStr);
    if (uaStr.data == nullptr) {
        UA_String_clear(&uaStr);
        return QString();
    }
    //Преобразуем байты в QString используя fromUtf8
    QByteArrayView byteView(reinterpret_cast<const char*>(uaStr.data), uaStr.length);
    UA_String_clear(&uaStr);
    return QString::fromUtf8(byteView);
}

UA_NodeId OPCUAClient::toNodeId(const QString &qStr)
{
    /* После использования возвращенного значения UA_NodeId не забываем удалять через clear!!! */
    UA_NodeId nodeId;
    UA_NodeId_init(&nodeId);
    UA_String uaString = UA_STRING_ALLOC(qStr.toUtf8().constData()); // ← Выделяет копию
    UA_StatusCode retval = UA_NodeId_parse(&nodeId, uaString);
    UA_String_clear(&uaString);
    if(retval != UA_STATUSCODE_GOOD) {
        UA_NodeId_clear(&nodeId);
        emit status(QString("[Error] %1, status: %2, nodeId: %3").arg(Q_FUNC_INFO, UA_StatusCode_name(retval), qStr));
        return UA_NODEID_NULL;
    }
    return nodeId;
}

QDateTime OPCUAClient::toQDateTime(const UA_DateTime &uaDt)
{
    UA_DateTimeStruct dt = UA_DateTime_toStruct(uaDt);
    QDate date(dt.year, dt.month, dt.day);
    QTime time(dt.hour, dt.min, dt.sec, dt.milliSec);
    //Создаем QDateTime в UTC и конвертируем в локальное время
    QDateTime utcDateTime(date, time, QTimeZone::UTC);
    return utcDateTime.toLocalTime();
}

UA_DateTime OPCUAClient::toUADateTime(const QDateTime &qDt)
{
    //Конвертируем локальное время в UTC
    QDateTime utcDateTime = qDt.toUTC();

    QDate date = utcDateTime.date();
    QTime time = utcDateTime.time();

    UA_DateTimeStruct dtStruct;
    dtStruct.year = date.year();
    dtStruct.month = date.month();
    dtStruct.day = date.day();
    dtStruct.hour = time.hour();
    dtStruct.min = time.minute();
    dtStruct.sec = time.second();
    dtStruct.milliSec = time.msec();
    dtStruct.microSec = 0;  // OPC UA поддерживает микросекунды
    dtStruct.nanoSec = 0;   // OPC UA поддерживает наносекунды

    return UA_DateTime_fromStruct(dtStruct);
}

QVariant OPCUAClient::toQVariant(const UA_Variant &uaVar)
{
    if (uaVar.type == nullptr || uaVar.data == nullptr) return {};
    const UA_DataType *type = uaVar.type;

    /* UA_NS0ID_* - это NodeId узлов типов в OPC UA пространстве имен (+1)
     * UA_TYPES_* - это индексы в массиве типов open62541
     * поэтому для определения типа данных type->typeKind используем UA_TYPES_, а если исользуем индекс то UA_NS0ID_ */

    switch (type->typeKind) {
    case UA_TYPES_BOOLEAN:
        return QVariant(*static_cast<UA_Boolean*>(uaVar.data));

    case UA_TYPES_SBYTE:
        return QVariant(*static_cast<UA_SByte*>(uaVar.data));

    case UA_TYPES_BYTE:
        return QVariant(*static_cast<UA_Byte*>(uaVar.data));

    case UA_TYPES_INT16:
        return QVariant(*static_cast<UA_Int16*>(uaVar.data));

    case UA_TYPES_UINT16:
        return QVariant(*static_cast<UA_UInt16*>(uaVar.data));

    case UA_TYPES_INT32:
        return QVariant(*static_cast<UA_Int32*>(uaVar.data));

    case UA_TYPES_UINT32:
        return QVariant(*static_cast<UA_UInt32*>(uaVar.data));

    case UA_TYPES_INT64:
        return QVariant(*static_cast<UA_Int64*>(uaVar.data));

    case UA_TYPES_UINT64:
        return QVariant(*static_cast<UA_UInt64*>(uaVar.data));

    case UA_TYPES_FLOAT:
        return QVariant(*static_cast<UA_Float*>(uaVar.data));

    case UA_TYPES_DOUBLE:
        return QVariant(*static_cast<UA_Double*>(uaVar.data));

    case UA_TYPES_STRING: {
        UA_String *uaString = static_cast<UA_String*>(uaVar.data);
        return QVariant(QString::fromUtf8((char*)uaString->data, uaString->length));
    }

    case UA_TYPES_BYTESTRING: {
        UA_ByteString *uaByteString = static_cast<UA_ByteString*>(uaVar.data);
        QByteArray byteArray((char*)uaByteString->data, uaByteString->length);
        return QVariant(byteArray.toHex());
    }

    case UA_TYPES_LOCALIZEDTEXT: {
        UA_LocalizedText *uaLocalizedText = static_cast<UA_LocalizedText*>(uaVar.data);
        QString locale = QString::fromUtf8((char*)uaLocalizedText->locale.data, uaLocalizedText->locale.length);
        QString text = QString::fromUtf8((char*)uaLocalizedText->text.data, uaLocalizedText->text.length);
        // Формат: "locale|text" или просто текст, если локаль пустая
        if (locale.isEmpty()) {
            return QVariant(text);
        } else {
            return QVariant(locale + "|" + text);
        }
    }

    case UA_TYPES_DATETIME: {
        UA_DateTime *uaDateTime = static_cast<UA_DateTime*>(uaVar.data);
        return QVariant(toQDateTime(*uaDateTime));
    }

    case UA_TYPES_NODEID: {
        UA_NodeId *uaNodeId = static_cast<UA_NodeId*>(uaVar.data);
        return toQString(*uaNodeId);
    }

    case UA_TYPES_STATUSCODE: {
        UA_StatusCode *statusCode = static_cast<UA_StatusCode*>(uaVar.data);
        return QVariant(QString::fromUtf8(UA_StatusCode_name(*statusCode)));
    }

    default: return {};
    }
}

UA_Variant OPCUAClient::toUAVariant(const QVariant &qVar)
{
    UA_Variant uaVar;
    UA_Variant_init(&uaVar);
    if (!qVar.isValid()) return uaVar;

    switch (qVar.typeId()) {
    case QMetaType::Bool: {
        UA_Boolean value = qVar.toBool();
        UA_Variant_setScalarCopy (&uaVar, &value, &UA_TYPES[UA_TYPES_BOOLEAN]);
        break;
    }
    case QMetaType::Char:
    case QMetaType::SChar: {
        UA_SByte value = static_cast<UA_SByte>(qVar.toInt());
        UA_Variant_setScalarCopy (&uaVar, &value, &UA_TYPES[UA_TYPES_SBYTE]);
        break;
    }
    case QMetaType::UChar: {
        UA_Byte value = static_cast<UA_Byte>(qVar.toUInt());
        UA_Variant_setScalarCopy (&uaVar, &value, &UA_TYPES[UA_TYPES_BYTE]);
        break;
    }
    case QMetaType::Short: {
        UA_Int16 value = static_cast<UA_Int16>(qVar.toInt());
        UA_Variant_setScalarCopy (&uaVar, &value, &UA_TYPES[UA_TYPES_INT16]);
        break;
    }
    case QMetaType::UShort: {
        UA_UInt16 value = static_cast<UA_UInt16>(qVar.toUInt());
        UA_Variant_setScalarCopy (&uaVar, &value, &UA_TYPES[UA_TYPES_UINT16]);
        break;
    }
    case QMetaType::Int: {
        UA_Int32 value = static_cast<UA_Int32>(qVar.toInt());
        UA_Variant_setScalarCopy (&uaVar, &value, &UA_TYPES[UA_TYPES_INT32]);
        break;
    }
    case QMetaType::UInt: {
        UA_UInt32 value = static_cast<UA_UInt32>(qVar.toUInt());
        UA_Variant_setScalarCopy (&uaVar, &value, &UA_TYPES[UA_TYPES_UINT32]);
        break;
    }
    case QMetaType::LongLong: {
        UA_Int64 value = static_cast<UA_Int64>(qVar.toLongLong());
        UA_Variant_setScalarCopy (&uaVar, &value, &UA_TYPES[UA_TYPES_INT64]);
        break;
    }
    case QMetaType::ULongLong: {
        UA_UInt64 value = static_cast<UA_UInt64>(qVar.toULongLong());
        UA_Variant_setScalarCopy (&uaVar, &value, &UA_TYPES[UA_TYPES_UINT64]);
        break;
    }
    case QMetaType::Float: {
        UA_Float value = qVar.toFloat();
        UA_Variant_setScalarCopy (&uaVar, &value, &UA_TYPES[UA_TYPES_FLOAT]);
        break;
    }
    case QMetaType::Double: {
        UA_Double value = qVar.toDouble();
        UA_Variant_setScalarCopy (&uaVar, &value, &UA_TYPES[UA_TYPES_DOUBLE]);
        break;
    }
    case QMetaType::QString: {
        QString qStr = qVar.toString();
        QByteArray utf8 = qStr.toUtf8();
        UA_String uaString = UA_STRING_ALLOC(utf8.constData());
        UA_Variant_setScalarCopy (&uaVar, &uaString, &UA_TYPES[UA_TYPES_STRING]);
        break;
    }
    case QMetaType::QDateTime: {
        QDateTime qDt = qVar.toDateTime();
        if (qDt.isValid()) {
            UA_DateTime uaDt = qDt.toMSecsSinceEpoch() * UA_DATETIME_MSEC;
            UA_Variant_setScalarCopy (&uaVar, &uaDt, &UA_TYPES[UA_TYPES_DATETIME]);
        }
        break;
    }
    default:
        break;
    }
    return uaVar;
}
