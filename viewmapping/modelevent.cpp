#include "modelevent.h"

ModelEvent::ModelEvent(QObject *parent)
    : QAbstractTableModel{parent}
{
    // Инициализация заголовков колонок
    columnHeaders << "ID" << "Время" << "Актив" << "Важн" << "Источник" << "Сообщение";
}

ModelEvent::~ModelEvent()
{

}

ModelEvent *ModelEvent::getInstance()
{
    static ModelEvent instance;
    return &instance;
}

int ModelEvent::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return eventList.size();
}

int ModelEvent::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return columnHeaders.size();
}

QVariant ModelEvent::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() ||
        index.row() >= eventList.size() ||
        index.column() >= columnHeaders.size())
        return QVariant{};

    const EventData &event = eventList.at(index.row());

    if (role == Qt::DisplayRole) {
        switch(index.column()) {
        case 0: return event.id;
        case 1: return event.time.toString("dd.MM.yyyy hh:mm:ss.zzz");
        case 2: return event.bactiveState ? "+" : "-";
        case 3: return event.severity;
        case 4: return event.sourceName;
        case 5: return event.message;
        default: return QVariant{};
        }
    }

    //Определяем цвет текста по уровню важности (столбец 3)
    if (role == Qt::ForegroundRole) {
        if (event.message.contains("[Error]", Qt::CaseInsensitive)) return QColor(255, 0, 0);
        if (event.message.contains("[Warning]", Qt::CaseInsensitive)) return QColor(255, 167, 0);
        if (event.message.contains("[Info]", Qt::CaseInsensitive)) return QColor(51, 51, 255);
        if (event.message.contains("[Success]", Qt::CaseInsensitive)) return QColor(0, 128, 0);
        if (event.message.contains("[User]", Qt::CaseInsensitive)) return QColor(128, 128, 128);
        return QColor(10, 9, 9);
    }

    //Выравнивание текста
    if (role == Qt::TextAlignmentRole) {
        switch(index.column()) {
        case 0: return Qt::AlignCenter;  //ID по центру
        case 1: return Qt::AlignCenter;  //Time по центру
        case 2: return Qt::AlignCenter;  //Активность по центру
        case 3: return Qt::AlignCenter;  //Важность по центру
        default: return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }
    return QVariant{};
}

QVariant ModelEvent::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole) return QVariant{};

    if (orientation == Qt::Horizontal) {
        if (section >= 0 && section < columnHeaders.size()) {
            return columnHeaders.at(section);
        }
    } else if (orientation == Qt::Vertical) {
        return section + 1;  // Нумерация строк с 1
    }

    return QVariant{};
}

bool ModelEvent::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || role != Qt::EditRole) return false;

    if (index.row() >= 0 && index.row() < eventList.size() &&
        index.column() >= 0 && index.column() < columnHeaders.size()) {

        EventData &event = eventList[index.row()];
        switch(index.column()) {
        case 0: event.id = value.toString(); break;
        case 1: event.time = value.toDateTime(); break;
        case 2: event.bactiveState = value.toBool(); break;
        case 3: event.severity = value.toUInt(); break;
        case 4: event.sourceName = value.toString(); break;
        case 5: event.message = value.toString(); break;
        default: return false;
        }
        emit this->dataChanged(index, index, {role});
        return true;
    }
    return false;
}

Qt::ItemFlags ModelEvent::flags(const QModelIndex &index) const
{
    if (!index.isValid()) return Qt::ItemIsEnabled;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

bool ModelEvent::insertRows(int row, int count, const QModelIndex &parent)
{
    if (count < 1 || row < 0 || row > eventList.size()) return false;

    this->beginInsertRows(parent, row, row + count - 1);
    for (int i = 0; i < count; ++i) {
        eventList.insert(row, EventData());
    }
    this->endInsertRows();
    return true;
}

bool ModelEvent::removeRows(int row, int count, const QModelIndex &parent)
{
    if (count < 1 || row < 0 || (row + count) > eventList.size()) return false;

    this->beginRemoveRows(parent, row, row + count - 1);
    for (int i = 0; i < count; ++i) {
        eventList.removeAt(row);
    }
    this->endRemoveRows();
    return true;
}

void ModelEvent::addEvent(const EventData &event)
{
    //Проверяем лимит и удаляем самую старую запись
    if (eventList.size() >= MAX_EVENTS) {
        this->beginRemoveRows(QModelIndex(), eventList.size() - 1, eventList.size() - 1);
        eventList.removeLast();
        this->endRemoveRows();
    }

    //Добавляем новую запись в начало
    this->beginInsertRows(QModelIndex(), 0, 0);
    eventList.insert(0, event);
    this->endInsertRows();

    /* упаковываем в eventmapapp только нужные поля для дальнейшей записи в QTcpSocket, после обновления модели */
    QVariantMap map;
    map["id"] = event.id;
    map["sourceName"] = event.sourceName;
    map["severity"] = event.severity;
    map["time"] = event.time;
    map["message"] = event.message;
    emit modelDataChanged(map);
}

QList<EventData> ModelEvent::getEvents() const
{
    return eventList;
}

void ModelEvent::clear()
{
    if (eventList.isEmpty()) return;
    this->beginRemoveRows(QModelIndex(), 0, eventList.size() - 1);
    eventList.clear();
    this->endRemoveRows();
}

void ModelEvent::append(const QVariantMap &map)
{
    if (map.isEmpty()) return;

    EventData event;
    event.id = QString::number(count).rightJustified(4, '0');
    event.time = map.value("time").toDateTime();
    event.bactiveState = map.value("bactiveState").toBool();
    event.severity = map.value("severity").toUInt();
    event.sourceName = map.value("sourceName").toString();

    if (11 <= event.severity && event.severity <= 20) {
        event.message = QString("%1 %2 %3").arg("[Error]", map["message"].toString(), map.value("currentState").toString());
    }
    else if (21 <= event.severity && event.severity <= 30) {
        event.message = QString("%1 %2 %3").arg("[Warning]", map["message"].toString(), map.value("currentState").toString());
    }
    else if (31 <= event.severity && event.severity <= 40) {
        event.message = QString("%1 %2 %3").arg("[Info]", map["message"].toString(), map.value("currentState").toString());
    }
    else if (event.severity == 100) {
        event.message = QString("%1 %2 %3").arg("[User]", map["message"].toString(), map.value("currentState").toString());
    }
    else {
        event.message = QString("%1 %2").arg(map["message"].toString(), map.value("currentState").toString());
    }

    addEvent(event);
    count++;
    if (count > MAX_COUNT) {
        count = 0;
    }
}

QString ModelEvent::getEventId(int row) const
{
    if (row >= 0 && row < eventList.size())
        return eventList.at(row).id;
    return QString();
}

QString ModelEvent::getEventMessage(int row) const
{
    if (row >= 0 && row < eventList.size())
        return eventList.at(row).message;
    return QString();
}

void ModelEvent::updateEvent(int row, const EventData &event)
{
    if (row >= 0 && row < eventList.size()) {
        eventList[row] = event;
        QModelIndex topLeft = createIndex(row, 0);
        QModelIndex bottomRight = createIndex(row, columnCount() - 1);
        emit dataChanged(topLeft, bottomRight);
    }
}
