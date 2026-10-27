#include "parsingdata.h"

ParsingData::ParsingData(QObject *parent)
    : QObject{parent}
{

}

ParsingData::~ParsingData()
{

}

ParsingData *ParsingData::getInstance()
{
    static ParsingData instance;
    return &instance;
}

void ParsingData::createSendData(QVariantMap &updatedMap, QVariantMap &allMap)
{
    /* читаем данные из модели и пишем в QTcpSocket для клиентов. Пишем ТОЛЬКО! изменившиеся данные (вызов по таймеру):
     * 1.через сигнал ModelMapping::modelDataChanged получаем из модели ТОЛЬКО измененные данные!
     * 2.записываем полученные данные в хэш текущих данных m_hData
     * 3.сравниваем с хэшем предыдущей итерации
     * 4.если по тегам есть изменения, готовим контейнер QVariantMap ТОЛЬКО!!! с измененными данными для записи в QTcpSocket
     * 5.после отправки данных, сохраняем хэш обновленных данных для следующей итерации */

    /* К каждому тегу из поля "TagApp" создаем 3 переменные дополнительных данных:
     * 1.TagApp.STATUS
     * 2.TagApp.TIMESTAMP
     * 3.TagApp.VALUE! */
    QMutexLocker locker(&m_mutex);
    //блокируем m_mutex
    if (m_hData.isEmpty() || m_hDataMem.isEmpty()) return;

    allMap.clear();     //map со всеми данными
    updatedMap.clear(); //map только c обновленными данными
    for (auto it = m_hData.constBegin(); it != m_hData.constEnd(); ++it) {
        QString key = it.key();
        const QString tagKey = QStringLiteral("TAG.") % key; //QStringLiteral и % выполняется быстрее чем QString().arg
        const DataValue &value = it.value();
        //готовим контейнер со всеми данными
        QVariantMap tagMap;     //map с параметрами тэга
        tagMap.insert("STATUS", value.status);          //статус
        tagMap.insert("TIMESTAMP", value.timestamp);    //метка времени
        tagMap.insert("VALUE", value.variant);          //значение
        allMap.insert(tagKey, tagMap);

        //готовим контейнер только с измененными данными, сравниванием с хэшем из предыдущей интерации
        if (auto itMem = m_hDataMem.constFind(key); itMem != m_hDataMem.constEnd()) {
            const DataValue &valueMem = itMem.value();
            if (value.variant != valueMem.variant || value.timestamp != valueMem.timestamp) {
                updatedMap.insert(tagKey, tagMap);
            }
        }
    }
    allMap.insert("SERVER.RESPONSE", "ALL_DATA");
    updatedMap.insert("SERVER.RESPONSE", "CURRENT_DATA");

    /* добавляем в пакет вектора технологические сообщения */
    allMap.insert("EVENTS", m_historyEvents);
    updatedMap.insert("EVENTS", m_currentEvents);
    m_currentEvents.clear();

    /* добавляем в пакет сервисную информацию */
    allMap.insert("SERVER.VERSION", m_srvInfo.appVersion);
    allMap.insert("SERVER.OPCCONNECT", m_srvInfo.opcconn);
    allMap.insert("SERVER.TIMESTAMP", QDateTime::currentDateTime());

    updatedMap.insert("SERVER.VERSION", m_srvInfo.appVersion);
    updatedMap.insert("SERVER.OPCCONNECT", m_srvInfo.opcconn);
    updatedMap.insert("SERVER.TIMESTAMP", QDateTime::currentDateTime());
    updatedMap.insert("SERVER.CLIENTSIZE", m_srvInfo.client_size);
    updatedMap.insert("SERVER.UPTIME", m_srvInfo.uptime);
    m_hDataMem = m_hData;
    // qDebug() << Q_FUNC_INFO << updatedMap;
    //разблокируем m_mutex
}

void ParsingData::createHistoryData(QTcpSocket *client, const QVariantMap &map)
{
    //быстрые проверки БЕЗ мьютекса
    if (!client || map.isEmpty()) return;

    QVariant tag = map.value("TAG.HISTORY");
    if (!tag.isValid()) return;
    QString tagStr = tag.toString();
    QVariantMap responseMap;
    QVariantList historyList;
    //работа с разделяемыми данными ПОД МЬЮТЕКСОМ
    {
        QMutexLocker locker(&m_mutex);
        auto it = m_hHistoryDatas.constFind(tagStr);
        if (it == m_hHistoryDatas.constEnd() || it.value().isEmpty()) return;
        const QList<DataValue> &datas = it.value();
        historyList.reserve(datas.size());
        for (const auto &data : datas) { //std::as_const(datas) - только для не константных ссылок (datas - константная ссылка)
            QVariantMap point;
            point.insert("STATUS", data.status);
            point.insert("TIMESTAMP", data.timestamp);
            point.insert("VALUE", data.variant);
            historyList.append(point);
        }
    } //разблокируем Мьютекс
    //Операции с client и отправка сигналов ПОСЛЕ мьютекса
    if (!historyList.isEmpty()) {
        client->setProperty("tags.history", tagStr);
        responseMap.insert("SERVER.RESPONSE", "HISTORY_DATA");
        responseMap.insert("TAG.HISTORY", tagStr);
        responseMap.insert("DATA", historyList);
        emit status(QString("[Info] %1, %2, TAG.HISTORY: %3, size: %4").arg(Q_FUNC_INFO, client->objectName(), tagStr, QString::number(historyList.size())));
        emit sendData(SendType::RESPONCE_DATA, client, responseMap, responseMap);
    }
}

void ParsingData::engineRequest(QTcpSocket *client, const QVariantMap &map)
{
    /* обрабатываем запросы клиента */
    if (!client || map.isEmpty()) return;
    if (map.value("CLIENT.REQUEST").toString() == QString("HISTORY_DATA")) createHistoryData(client, map);
}

void ParsingData::setServerInfo(const QVariantMap &map)
{
    if (map.value("connectOPC").isValid()) m_srvInfo.opcconn = map.value("connectOPC").toBool();
    if (map.value("appVersion").isValid()) m_srvInfo.appVersion = map.value("appVersion").toString();
    if (map.value("clientSize").isValid()) m_srvInfo.client_size = map.value("clientSize").toUInt();
    if (map.value("uptime").isValid()) m_srvInfo.uptime = map.value("uptime");
}

void ParsingData::setListTags(const QList<QString> &tags)
{
    /* заполняем хэши с тегами для передачи клиентам */
    QMutexLocker locker(&m_mutex);

    m_hData.clear();
    m_hDataMem.clear();
    m_hHistoryDatas.clear();
    for (const auto &tag : tags) { //tag - ссылка на элемент константная
        m_hData.insert(tag, DataValue());
        m_hDataMem.insert(tag, DataValue());
        m_hHistoryDatas.insert(tag, QList<DataValue>()); //в хэше храним все данные, но берем только те, по которым был запрос со стороны клиента
    }
    // qDebug() << Q_FUNC_INFO << tags;
}

void ParsingData::setDataChanged(const QVariantMap &map)
{
    /* обновляем хэши при изменении данных */
    QMutexLocker locker(&m_mutex);
    if (m_hData.isEmpty() || m_hHistoryDatas.isEmpty()) return; //заполняем m_hData при создании векторов тегов для опроса

    QString tagApp = map.value("tagapp").toString();
    QString status = map.value("status").toString();
    QDateTime timestamp = map.value("sourceTimestamp").toDateTime();
    QVariant variant = map.value("variant");

    /* m_hData.value(key) - возвращают копию, для изменения данных в хэше нужна ссылка, m_hData.find(key) - возвращает итератор it, it.value() ссылка на значение */
    //обновляем текущие данные
    if (auto it = m_hData.find(tagApp); it != m_hData.end()) {
        DataValue &data = it.value();
        data.status = status;
        data.timestamp = timestamp;
        data.variant = variant;
    }
    //обновляем историю
    if (auto it = m_hHistoryDatas.find(tagApp); it != m_hHistoryDatas.end()) {
        QList<DataValue> &datas = it.value();
        //добавляем, если пусто ИЛИ последняя точка отличается
        if (datas.isEmpty() || datas.last().timestamp != timestamp || datas.last().variant != variant) { //при выполнении первого условия остальные не проверяем
            DataValue &data_last = datas.emplaceBack(DataValue());   //C++17 конструирование на месте
            data_last.status = status;
            data_last.timestamp = timestamp;
            data_last.variant = variant;
        }
        //очистка по количеству
        while (datas.size() >= MAX_HISTORY_DATA) {
            datas.removeFirst();
        }
        //очистка по времени (приводим к одному формату времени, количество секунд от 1970UTC)
        const qint64 nowSec  = QDateTime::currentSecsSinceEpoch();
        const auto isTimeTrim = [nowSec](const QList<DataValue> &list) {
            if (list.isEmpty()) return false;
            const QDateTime& t1 = list.first().timestamp;
            if (!t1.isValid()) return true;   //невалидный — удаляем
            const qint64 ageSec = nowSec - t1.toSecsSinceEpoch();
            return ageSec >= static_cast<qint64>(MAX_TIME_DATA);
        };
        while (isTimeTrim(datas)) {
            datas.removeFirst();
        }
        emit updateHistoryCounter(tagApp, datas.size());
    }
}

void ParsingData::setEventsChanged(const QVariantMap &map)
{
    QMutexLocker locker(&m_mutex); //NOLINTNEXTLINE(clang-analyzer-core.CallAndMessage) //отключили Warning clang-analyzer-core
    while (m_historyEvents.size() > MAX_HISTORY_EVENTS) m_historyEvents.removeFirst();    //очередь исторических сообщений для новых клиентов - 500 сообщений;
    m_historyEvents.append(map);
    while (m_currentEvents.size() > 50) m_currentEvents.removeFirst();     //очередь текущих сообщений перед отправкой (чистим вектор после отправки);
    m_currentEvents.append(map);
}

void ParsingData::getSendMap(QVariantMap &updatedMap, QVariantMap &allMap)
{
    createSendData(updatedMap, allMap);
}
