/* Модель для хранения журнала сообщений сервера (проекта, не сервера OPCUA) */
#ifndef MODELEVENT_H
#define MODELEVENT_H

#include <QAbstractTableModel>
#include <QDateTime>
#include <QColor>
#include <QStringList>

struct EventData {
    QString id;
    QDateTime time;
    bool bactiveState;
    quint16 severity;
    QString sourceName;
    QString message;

    EventData() : bactiveState(false) {}
};

class ModelEvent : public QAbstractTableModel
{
    Q_OBJECT

private:
    explicit ModelEvent(QObject *parent = nullptr);
    ~ModelEvent();


    QList<EventData> eventList;
    QStringList columnHeaders;
    uint count = 0;
    static const quint16 MAX_COUNT = 9999;
    static const quint16 MAX_EVENTS = 2000;

    //Переопределенные методы QAbstractTableModel
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    //Методы для управления строками
    bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;

public:
    static ModelEvent *getInstance();

    //Публичные методы
    void addEvent(const EventData &event);
    void append(const QVariantMap &map);
    void clear();

    //Дополнительные методы для удобства
    QList<EventData> getEvents() const;
    QString getEventId(int row) const;
    QString getEventMessage(int row) const;
    void updateEvent(int row, const EventData &event);

signals:
    void modelDataChanged(const QVariantMap &map); //добавили новое сообщение

};

#endif // MODELEVENT_H
