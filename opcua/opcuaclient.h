/* С++ обертка Клиента OPCUA для работы с библиотекой open62541 */
#ifndef OPCUACLIENT_H
#define OPCUACLIENT_H

#include <QObject>
#include <QTimer>
#include <QMap>
#include <QDir>
#include <QDebug>
#include "open62541.h"

/* Порядок работы:
 1.Создаем временного клиента
 2.Получаем список Endpoints (без установления полноценного соединения)
 3.Выбираем подходящий Endpoint с поддержкой нужной аутентификации
 4.Подключаемся к выбранному Endpoint с настройками безопасности
 5.Выполняем аутентификацию */

//----регестрируем глобальные типы данных (UDT - пользовательские типы данных)---//
//Endpoint (структура для передачи точек подключения к серверу)
struct SEndpoint {
    QString endpointUrl;
    QString securityPolicy;
    UA_MessageSecurityMode securityMode;
};
typedef SEndpoint Endpoint;
Q_DECLARE_METATYPE(Endpoint)

//Browse Node (структура для области данных узлов сервера OPCUA)
struct NodeOPC {
    QString browseName;
    UA_NodeClass nodeClass;
    QString nodeIdStr;
    UA_ExpandedNodeId extendedNodeId;
    QString referenceTypeId; //i=35 - Organizes, i=36 - HasEventSource (для событий), i=40 - HasTypeDefinition, i=47 - обратная ссылка
    QString displayName;
};
Q_DECLARE_METATYPE(NodeOPC)

//Node Attributes (структура данных атрибутов узла)
struct NodeAttribute {
    UA_AttributeId attributeId;
    UA_DataValue results;
};
Q_DECLARE_METATYPE(NodeAttribute)

//Структура для чтения/записи данных
struct QDataRW {
    QString nodeId;
    QVariant variant;
    QString status;
    QDateTime sourceTimestamp;
    QDateTime serverTimestamp;
};
Q_DECLARE_METATYPE(QDataRW)

//Структура для событий
struct QDataEvent {
    QString nodeId;     //адрес NodeId подписки
    QString eventId;    //hex id сообщения
    QString sourceNode; //NodeId источника сообщения
    QString sourceName; //имя источника сообщения
    QString message;
    quint16 severity;   //важность 0..399 инф.сообщения, 400..699 Предупреждения, 700..899 Аварии, 900..1000 Критические события
    QDateTime time;
    QString activeState;    //Active/Inactive - строковое состояние сообщение пришло/ушло
    bool bactiveState;      //булевое соостяние сообщение пришло/ушло
    QString currentState;   //текущее состояние (распарсил сам)
};
Q_DECLARE_METATYPE(QDataEvent)

//не забываем добавить qRegisterMetaType<Endpoint>("Endpoint") в main.cpp
//-------------------------------------------------------------------------------//

class OPCUAClient : public QObject
{
    Q_OBJECT

private:
    explicit OPCUAClient(QObject *parent = nullptr);
    ~OPCUAClient();

    UA_Client *pclient = nullptr;
    QTimer *ptimer = nullptr;
    QTimer *preadTimer = nullptr;
    bool freadBusy = false; //флаг блокирующий повторные запросы на чтение, пока не пришел ответ
    bool fdisconnectQueued = false; //флаг блокирующий повторные вызовы, когда обрабатываем отключение от сервера disconnectFromServer() из очереди событий

    QString applicationUri = "urn:ingcqtapp:opcuaclient"; //Uri - должен совпадать в сертификате и настройке подключения защищенного клиента
    UA_ReadRequest readRequest; //запрос для чтения
    QList<UA_UInt32> subscriptionIds; //лист активных подписок (если работыть с подписками, надо делать методы обновления/удаления списка)

    /* cтруктура с данными подписки на узлы NodeId */
    struct SubscribeGroupEvent {
        QList<QString> eventFields; //поля для запроса
        QList<QString> nodeIds;
        UA_UInt32 subscriptionId;
        UA_UInt32 monitoredIdsSize; //размер nodeId
        UA_UInt32 *monitoredIds;    //ID подписанного узла
        void **contexts;            //Контексты для callback'ов (массив указателей на контексты)
        UA_Client_EventNotificationCallback *eventCallbacks; //сallback,и при изменении значения одного из узлов в подписке
        UA_Client_DeleteMonitoredItemCallback *deleteCallbacks; //сallback,и при удалении подписки
        /* поля для сохранения указателей и очистки ресурсов в callback */
        UA_EventFilter *filter;
        UA_MonitoredItemCreateRequest *monRequests;
        ~SubscribeGroupEvent() {
            if (monitoredIds) {
                UA_free(monitoredIds);
                monitoredIds = nullptr;
            }
            if (contexts) {
                UA_free(contexts);
                contexts = nullptr;
            }
            if (eventCallbacks) {
                UA_free(eventCallbacks);
                eventCallbacks = nullptr;
            }
            if (deleteCallbacks) {
                UA_free(deleteCallbacks);
                deleteCallbacks = nullptr;
            }
            cleanupResourcesSubscribeAsync(filter, monRequests, monitoredIdsSize); //очищаем ресурсы
            eventFields.clear();
            nodeIds.clear();
        }
        SubscribeGroupEvent()
            : subscriptionId(0),
            monitoredIdsSize(0),
            monitoredIds(nullptr),
            contexts(nullptr),
            eventCallbacks(nullptr),
            deleteCallbacks(nullptr),
            filter(nullptr),
            monRequests(nullptr) {}
    };
    /* Структура для передачи SubscribeGroupEvent в subscribeEventCallback через контекст */
    struct ContextEvent {
        SubscribeGroupEvent *subscribeGroup;
        QString nodeId;
        size_t index;
    };
    QHash<UA_UInt32, SubscribeGroupEvent*> hSubscribeGroupEvent; //хэш для доступа и удаления динамически созданных контекстов

    /* cтруктура с данными подписки на узлы NodeId */
    struct SubscribeGroupNodes {
        QList<QString> nodeIds;
        UA_UInt32 subscriptionId;
        UA_UInt32 monitoredIdsSize; //размер nodeId
        UA_UInt32 *monitoredIds;    //ID подписанного узла
        void **contexts;            //Контексты для callback'ов (массив указателей на контексты)
        UA_Client_DataChangeNotificationCallback *dataChangeCallbacks; //сallback,и при изменении значения одного из узлов в подписке
        UA_Client_DeleteMonitoredItemCallback *deleteCallbacks; //сallback,и при удалении подписки
        /* поля для сохранения указателей и очистки ресурсов в callback */
        UA_DataChangeFilter *filter;
        UA_MonitoredItemCreateRequest *monRequests;
        ~SubscribeGroupNodes() {
            if (monitoredIds) {
                UA_free(monitoredIds);
                monitoredIds = nullptr;
            }
            if (contexts) {
                UA_free(contexts);
                contexts = nullptr;
            }
            if (dataChangeCallbacks) {
                UA_free(dataChangeCallbacks);
                dataChangeCallbacks = nullptr;
            }
            if (deleteCallbacks) {
                UA_free(deleteCallbacks);
                deleteCallbacks = nullptr;
            }
            cleanupResourcesSubscribeAsync(filter, monRequests, monitoredIdsSize); //очищаем ресурсы
            nodeIds.clear();
        }
        SubscribeGroupNodes()
            : subscriptionId(0),
            monitoredIdsSize(0),
            monitoredIds(nullptr),
            contexts(nullptr),
            dataChangeCallbacks(nullptr),
            deleteCallbacks(nullptr),
            filter(nullptr),
            monRequests(nullptr) {}
    };
    /* Структура для передачи SubscribeGroupNodes в subscribeNodesCallback через контекст */
    struct ContextNodes {
        SubscribeGroupNodes *subscribeGroup;
        QString nodeId;
        size_t index;
    };
    QHash<UA_UInt32, SubscribeGroupNodes*> hSubscribeGroupNodes; //хэш для доступа и удаления динамически созданных контекстов

    /* cтруктура для передачи в readCallback, writeCallback через контекст */
    struct ContextRW {
        QList<QDataRW> qdataRWs;
    };
    ContextRW readCtx;

    /* массив полей для выборки событий (журнал сообщений) */
    QList<QString> eventFields = {"EventId", "SourceNode", "SourceName", "Message", "Severity",
        "Time",             //время события на устройстве plc
        "ActiveState",      //активно ли условие
        "ActiveState/Id",   //ID активного состояния (в данном случае true/false границы)
        "LimitState/CurrentState/Id", //текущее состояние границы (парсить надо по nodeId для конкретного сервера)
        "EnabledState"      //условие включено
    };

    void freeMemory_dll(UA_ByteString &bstr); //освобождаем память, если библиотека dll
    bool generateSelfSignedCertificate(UA_ByteString &cert, UA_ByteString &key);
    bool saveCertificate(const QString &fileName, const char *data, const qint64 &len);
    UA_ByteString loadCertificate(const QString &fileName); //очищаем UA_ByteString после загрузки
    bool isConnected(UA_Client *&client);
    void cleanupHashContexts(const UA_UInt32 &subscriptionId);
    //static - метод cleanupResourcesSubscribeToEventsAsync доступен в деструкторе структуры ContextEvent
    static void cleanupResourcesSubscribeAsync(UA_EventFilter *&filter, UA_MonitoredItemCreateRequest *&monRequests, UA_UInt32 &monitoredIdsSize);
    static void cleanupResourcesSubscribeAsync(UA_DataChangeFilter *&filter, UA_MonitoredItemCreateRequest *&monRequests, UA_UInt32 &monitoredIdsSize);

    //callback's
    static void stateCallback(UA_Client *client, UA_SecureChannelState channelState, UA_SessionState sessionState, UA_StatusCode connectStatus);
    static void loggingCallback(void *context, UA_LogLevel level, UA_LogCategory category, const char *msg, va_list args);
    static void subscribeEventCallback(UA_Client *client, UA_UInt32 subId, void *subContext, UA_UInt32 monId, void *monContext,
                                       size_t nEventFields, UA_Variant *eventFields);
    static void subscribeNodesCallback(UA_Client *client, UA_UInt32 subId, void *subContext, UA_UInt32 monId, void *monContext, UA_DataValue *value);
    static void deleteNodesCallback(UA_Client *client, UA_UInt32 subId, void *subContext, UA_UInt32 monId, void *monContext);
    static void readCallback(UA_Client *client, void *userdata, UA_UInt32 requestId, UA_ReadResponse *rr);
    static void writeCallback(UA_Client *client, void *userdata, UA_UInt32 requestId, UA_WriteResponse *wr);
    static UA_Boolean historicalEventCallback(UA_Client *client, const UA_NodeId *nodeId, UA_Boolean moreDataAvailable,
                                             const UA_ExtensionObject *data, void *callbackContext);
    static UA_Boolean historicalDataCallback(UA_Client *client, const UA_NodeId *nodeId, UA_Boolean moreDataAvailable,
                                             const UA_ExtensionObject *data, void *callbackContext);
    QDataEvent parsingEvent(const UA_Variant *eventFields, const size_t &nEventFields, const QList<QString> &eventFieldList);
    void disconnectClient();    //отключаем клиента

private slots:
    void processTimer();
    void processReadNodeAsync();

public:
    static OPCUAClient *getInstance();  //создаем один экземпляр класса (паттерн Singleton)

    bool getEndpoints(const QString &url, QList<Endpoint> &storeEndpoint);
    bool connectToServerAnonymous(const QString &url);
    bool connectToServerUsername(const QString &url, const Endpoint &endpoint, const QString &name, const QString &pass);
    void disconnectFromServer();
    /* синхронные запросы */
    bool browseNode(const QString &nodeId); //при загрузке смотрим Root
    bool readNodeAttributes(const QString &nodeId);

    /* 1. cоздаем подписку с разными items или несколько подписок с разной цикличностью
     * 2. добавляем/удаляем в уже созданную подписку items
     * 3. удаляем подписку (если удалить все items из подписки, без удаления подписки, сервер продолжит ее обслуживать */

    /* подписка на события */
    bool createSubscriptionToEvents(); //All Server Events
    bool createSubscriptionToEventsAsync(const QList<QString> &nodeIds, const UA_Double &publishingInterval = 1000.0);  //ms
    /* подписка на чтение узлов */
    bool createSubscriptionToNodesAsync(const QList<QString> &nodeIds, const UA_Double &publishingInterval = 1000.0);   //ms
    bool addSubscriptionItemAsync(const UA_UInt32 &subscriptionId);
    bool deleteSubscriptionItemAsync(SubscribeGroupNodes *subscribeGroup);
    bool deleteSubscriptionAsync(QList<UA_UInt32> &subscriptionIds, std::function<void (bool)> succesCallback);
    /* асинхронное чтение/запись */
    bool createReadRequest(const QList<QString> &nodeIds); //тут создаем запрос, читаем в методе void readNodeAsync();
    bool writeNodeAsync(const QList<QDataRW> &qdataRWs);
    /* чтение истории, синхронный запрос с callback */
    bool readHistoryEvents(const QString &nodeId, const QDateTime &startTime, const QDateTime &endTime, const UA_UInt32 &maxValues = 0);//maxValues = 0 - вернуть все значения
    bool readHistoryData(const QString &nodeId, const QDateTime &startTime, const QDateTime &endTime, const UA_UInt32 &maxValues = 0);  //maxValues = 0 - вернуть все значения

    bool saveCertificatesToFile();

    /* getters */
    const QString getCertDir() const;
    const QString getVersion() const;
    const QList<UA_UInt32> &getSubscriptionIds() const; //константная ссылка
    QList<UA_UInt32> &getSubscriptionIds();             //ссылка, можем изменять значения

    /* data convert */
    QString toQString(const UA_String &uaStr);
    QString toQString(const UA_NodeId &nodeId);
    UA_NodeId toNodeId(const QString &qStr);
    QDateTime toQDateTime(const UA_DateTime &uaDt);
    UA_DateTime toUADateTime(const QDateTime &qDt);
    QVariant toQVariant(const UA_Variant &uaVar);
    UA_Variant toUAVariant(const QVariant &qVar);

signals:
    void connected(const bool &);
    void status(const QString &); //LogType, message
    void browseCompleted(const QString &nodeId, const QList<NodeOPC> &childNodes);
    void attributeCompleted(const QString &nodeId, const QList<NodeAttribute> &attributes);
    void readCompleted(const QList<QDataRW> &qdataRWs);     //обработка в цикле
    void subscribeEventCompleted(const QDataEvent &qevent);
    void subscribeNodesCompleted(const QDataRW &qdataRW);
    void historicalEventCompleted(const QDataEvent &qevent);//обработка в цикле
    void historicalDataCompleted(const QDataRW &qdataRW);   //обработка в цикле
};

#endif // OPCUACLIENT_H
