#ifndef MODELSYSTEMLOG_H
#define MODELSYSTEMLOG_H

#include <QAbstractListModel>
#include <QDateTime>
#include <QColor>
#include <QVariant>

class ModelSystemLog : public QAbstractListModel
{
    Q_OBJECT

private:
    explicit ModelSystemLog(QObject *parent = nullptr);
    ~ModelSystemLog();
    QStringList stringList;

    const uint16_t MAX_COUNT = 5000; //количество записей
    uint16_t count = 0;

    //обязательные методы QAbstractListModel
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    //методы для редактирования данных
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    //методы для добавления/удаления данных
    bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;

    void addString(const QString &string);

public:
    static ModelSystemLog *getInstance();

    void clear();
    void append(const QString &callerInfo, const QString &message);
    const QStringList &getStringList() const;
};

#endif // MODELSYSTEMLOG_H
