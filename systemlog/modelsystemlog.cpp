#include "modelsystemlog.h"

ModelSystemLog::ModelSystemLog(QObject *parent)
    : QAbstractListModel{parent}
{

}

ModelSystemLog::~ModelSystemLog()
{

}

ModelSystemLog *ModelSystemLog::getInstance()
{
    static ModelSystemLog instance;
    return &instance;
}

int ModelSystemLog::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return stringList.size();
}

QVariant ModelSystemLog::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= stringList.size()) return QVariant{};

    QString text = stringList.at(index.row());

    //Qt::BackgroundRole - цвет фона
    //Qt::TextAlignmentRole - выравнивание текста

    if (role == Qt::DisplayRole) {
        return text;
    }
    //Определяем цвет по содержимому текста
    if (role == Qt::ForegroundRole) {
        if (text.contains("[Error]")) return QColor(255, 0, 0);       // красный
        if (text.contains("[Warning]")) return QColor(255, 167, 0);   // оранжевый
        if (text.contains("[Info]")) return QColor(51, 51, 255);      // синий
        if (text.contains("[Success]")) return QColor(0, 128, 0);     // зеленый
        return QColor(10, 9, 9); // черный
    }
    return QVariant{};
}

bool ModelSystemLog::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || role != Qt::EditRole) return false;

    if (index.row() >= 0 && index.row() < stringList.size()) {
        stringList[index.row()] = value.toString();
        emit dataChanged(index, index, {role});
        return true;
    }
    return false;
}

Qt::ItemFlags ModelSystemLog::flags(const QModelIndex &index) const
{
    if (!index.isValid()) return Qt::ItemIsEnabled;
    // return QAbstractListModel::flags(index) | Qt::ItemIsEditable;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable; //Убираем флаг редактирования, оставляем только выбор
}

bool ModelSystemLog::insertRows(int row, int count, const QModelIndex &parent)
{
    if (count < 1 || row < 0 || row > stringList.size()) return false;

    this->beginInsertRows(parent, row, row + count - 1);
    for (int i = 0; i < count; ++i) {
        stringList.insert(row, QString());
    }
    this->endInsertRows();
    return true;
}

bool ModelSystemLog::removeRows(int row, int count, const QModelIndex &parent)
{
    if (count < 1 || row < 0 || (row + count) > stringList.size()) return false;

    this->beginRemoveRows(parent, row, row + count - 1);
    for (int i = 0; i < count; ++i) {
        stringList.removeAt(row);
    }
    this->endRemoveRows();
    return true;
}

void ModelSystemLog::addString(const QString &string)
{
    //Удаляем старые стоки
    if (stringList.size() > MAX_COUNT) stringList.removeFirst();

    const int row = stringList.size();
    this->beginInsertRows(QModelIndex(), row, row);
    stringList.append(string);
    this->endInsertRows();
}

void ModelSystemLog::clear()
{
    if (stringList.isEmpty()) return;
    this->beginRemoveRows(QModelIndex(), 0, stringList.size() - 1);
    stringList.clear();
    this->endRemoveRows();
}

void ModelSystemLog::append(const QString &callerInfo, const QString &message)
{
    QString str(QString::number(count).rightJustified(4, '0') + "; " + QDateTime::currentDateTime().toString("dd.MM.yyyy hh:mm:ss.zzz") + "; " +
                callerInfo + "; " + message);
    addString(str);
    count++;
    if (count > 6 * MAX_COUNT) count = 0; //9000 count
}

const QStringList &ModelSystemLog::getStringList() const
{
    return stringList;
}
