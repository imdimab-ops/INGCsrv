/* ParsingData класс, получаем и разбираем данные */
#ifndef PARSINGDATA_H
#define PARSINGDATA_H

#include <QObject>
#include <QTcpSocket>
#include <QVariantMap>
#include <QDateTime>
#include <QMutex>

/* перечисление типа данных, которые подготовлены для отправки клиенту */
enum SendType {
    CURRENT_DATA, //текущие данные, отправляемые по таймеру
    RESPONCE_DATA, //обработка ответов для клиентов
    PING
};

class ParsingData : public QObject
{
    Q_OBJECT

private:
    explicit ParsingData(QObject *parent = nullptr);
    ~ParsingData();

public:
    static ParsingData *getInstance();

private:
    struct DataValue {
        DataValue() {}
        QString status;
        QDateTime timestamp;
        QVariant variant;
    };
    QMutex m_mutex;

    /* server info */
    struct SrvInfo {
        bool opcconn = false; //флаг подключения к серверу OPC
        QString appVersion = "";
        quint16 client_size = 0;
        QVariant uptime;
    };
    SrvInfo m_srvInfo;

    /* хеши для обновленных данных для передачи клиенту */
    QHash<QString, DataValue> m_hData;     //текущие данные (key (QString) - имя тега TagApp)
    QHash<QString, DataValue> m_hDataMem;  //данные с предыдущей итерации

    /* вектора сообщений для передачи клиенту */
    static const quint16 MAX_HISTORY_EVENTS = 1500; //max количество исторических сообщений
    QList<QVariant>m_currentEvents; //вектор текущих сообщений (т.к. за раз может прийти группа сообщений)
    QList<QVariant>m_historyEvents; //вектор исторических сообщений (для вновь подключенных клиентов)

    /* вектора исторических данных и хэш доступа к нему */
    static const quint16 MAX_HISTORY_DATA = 25000; //25000 * 200ms = 5000sec (~1h:25min)(update 200ms)
    static const quint16 MAX_TIME_DATA = 18000;    //sec (5hour)
    QHash<QString, QList<DataValue>> m_hHistoryDatas; //(key (QString) - имя тега TagApp)

    void createSendData(QVariantMap &updatedMap, QVariantMap &allMap);
    void createHistoryData(QTcpSocket *client, const QVariantMap &map);

public:
    void engineRequest(QTcpSocket *client, const QVariantMap &map); //запрос клиента для обработки

    /* setters */
    void setServerInfo(const QVariantMap &map);
    void setListTags(const QList<QString> &tags);
    void setDataChanged(const QVariantMap &map);
    void setEventsChanged(const QVariantMap &map);

    /* getters */
    void getSendMap(QVariantMap &updatedMap, QVariantMap &allMap);

signals:
    void sendData(enum SendType, QTcpSocket *client, const QVariantMap &map, const QVariantMap &allMap); //2 QVariantMap, т.к. в connect надо 2
    void updateHistoryCounter(const QString &tagapp, const quint16 &size); //обновляем на UI счетчики исторических данных
    void status(const QString &);

};

#endif // PARSINGDATA_H
