#include "engine.h"

Engine::Engine(QObject *parent)
    : QObject{parent}
{
    setupConnections();
}

Engine::~Engine()
{

}

Engine *Engine::getInstance()
{
    static Engine instance;
    return &instance;
}

void Engine::setupConnections()
{
    //Заполняем вектора и хэши всеми тегами, которые необходимо опрашивать
    auto *modelMapping = ModelMapping::getInstance();
    connect(modelMapping, &ModelMapping::nodeIdsCompleted, this, [this](const QList<QString> &nodeIds, const QList<QString> &eventNodeIds, const QList<QString> &tags) {
        nodeIds_mem.clear();
        nodeIds_mem = nodeIds;
        eventNodeIds_mem.clear();
        eventNodeIds_mem = eventNodeIds;
        createSubscribeOPC();
        emit tcpsrv->setListTags(tags);
    });
    //Пишем только измененные данные для тэгов
    connect(modelMapping, &ModelMapping::modelDataChanged, this, [this](const QVariantMap &map) {
        emit tcpsrv->setDataChanged(map);
        emit tcpsrv->setServerInfo(QVariantMap{{"uptime", applib->getUptime()}});
    });
    //Пишем только измененные данные для сообщений
    auto *modelEvent = ModelEvent::getInstance();
    connect(modelEvent, &ModelEvent::modelDataChanged, this, [this](const QVariantMap &map){
        emit tcpsrv->setEventsChanged(map);
    });

    //Отслуживаем подключен ОПС или нет
    connect(opcClient, &OPCUAClient::connected, this, [this, modelMapping] (const bool &b) {
        bconnectOPC = b;
        if (!b) modelMapping->clearSomeItems(); //сбрасываем определенные item, чтобы было видно что нет обмена
        emit tcpsrv->setServerInfo(QVariantMap{
            {"connectOPC", bconnectOPC},
            {"appVersion", applib->getAppVersion()}
        });
    });
    //Обновляем данные модели тэгов по сигналу готовности от OPCUA клиента
    connect(opcClient, &OPCUAClient::subscribeNodesCompleted, this, [this, modelMapping] (const QDataRW &qdataRW) {
        QVariantMap map;
        map["nodeId"] = qdataRW.nodeId;
        map["sourceTimestamp"] = qdataRW.sourceTimestamp;
        map["status"] = qdataRW.status;
        map["variant"] = qdataRW.variant;
        modelMapping->updateModel(map);
        // qDebug() << Q_FUNC_INFO << qdataRW.nodeId << qdataRW.variant << qdataRW.status;
    });
    //Обновляем счетчики исторических данных в модели ModelMapping
    connect(tcpsrv, &TcpThread::updateHistoryCounter, modelMapping, &ModelMapping::updateHistoryCounter);
    //Обновляем данные модели сообщений по сигналу готовности от OPCUA клиента
    connect(opcClient, &OPCUAClient::subscribeEventCompleted, this, [this, modelEvent, modelMapping](const QDataEvent &qdataEvent) {
        /* тут надо распарсить по типам сообщений и сделать цвет текста */
        QVariantMap map;
        map["nodeId"] = qdataEvent.nodeId;
        map["eventId"] = qdataEvent.eventId;
        map["sourceNode"] = qdataEvent.sourceNode;
        map["sourceName"] = qdataEvent.sourceName;
        map["severity"] = qdataEvent.severity;
        map["time"] = qdataEvent.time;
        map["activeState"] = qdataEvent.activeState;
        map["currentState"] = qdataEvent.currentState;
        map["bactiveState"] = qdataEvent.bactiveState;
        map["message"] = qdataEvent.message;
        modelEvent->append(map);
        modelMapping->updateModel(map); //сделал для обновление строки, где оформляем подписку
    });
}

void Engine::setSubscribeEnable(const bool &data, const bool &event)
{
    bsubscribeDataEnable = data;
    bsubscribeEventEnable = event;
}

void Engine::createSubscribeOPC()
{
    /* Порядок работы:
     * 1.если подписок нет - создаем (для удобства все через функцию таймера)
     * 2.удаляем действующие подписки
     * 3.создаем новые подписки (отложенно, через таймер) */
    if (!bconnectOPC) return;
    if (opcClient->getSubscriptionIds().size() == 0) createSubscribeOPCTimer();

    QList<UA_UInt32> &ids = opcClient->getSubscriptionIds(); //берем значение по ссылке (можем изменять)
    opcClient->deleteSubscriptionAsync(ids, [this](bool succesCallback){
        if (succesCallback) createSubscribeOPCTimer();
        qDebug() << Q_FUNC_INFO <<  "succesCallback_deleteSubscription" << succesCallback; //ждем удаление подписки и переходим к следующей итерации
    });
}

void Engine::createSubscribeOPCTimer()
{
    /* создаем новые подписки с задержкой, чтобы предыдущие успели удалиться и корректно очиститься ресурсы
     * не смотря на то, что создаем подписки после обработки удаления подписок в callback! */
    QTimer::singleShot(2000, this, [this]() {
        if (bsubscribeDataEnable) opcClient->createSubscriptionToNodesAsync(nodeIds_mem, 500);
        if (bsubscribeEventEnable) opcClient->createSubscriptionToEventsAsync(eventNodeIds_mem, 1000);
    });
}
