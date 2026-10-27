/* Модель данных карты привязки адресов */
#ifndef MODELMAPPING_H
#define MODELMAPPING_H

#include <QStandardItemModel>
#include <QMimeData>
#include <QIODevice>
#include <QEventLoop>
#include <QDateTime>
#include <QPainter>
#include <QIcon>
#include <QMutex>
#include <QTimer>
#include <QApplication>
#include <QClipboard>

class ModelMapping : public QStandardItemModel
{
    Q_OBJECT

private:
    explicit ModelMapping(QObject *parent = nullptr);
    ~ModelMapping();

    //QStandardItemModel флаги редактирования на уровне модели
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    mutable QMutex mutex; //защищаем от одновременного доступа (остальные вызовы становятся в очередь) mutable - доступен в const методах
    QHash<QString, QList<QStandardItem*>> hmodel; //хэш ключей ["nodeId"] для быстрого доступа к itemsRow
    QHash<QString, QStandardItem*>  hitemHistory; //хэш ключей ["tagapp"] для быстрого дотупа к item "History" для отображения глубины архива

    QColor getColor(const QString &valueType);
    QIcon createColorIcon(const QColor &color);

    static const int MAX_RECURSION_DEPTH = 300; //максимальная глубина рекурсии
    void saveRecursivelyHash(QStandardItemModel *model, QStandardItem *parentItem, uint &depth); //перед вызовом не заабываем очистить QHash!
    void updateHash(std::function<void(bool, uint)> callback); //функция асинхронная, делаем с callback

public:
    static ModelMapping *getInstance();

    //перечисление для типов элементов (храним в пользовательской роли Qt::UserRole + 0)
    enum ItemType {
        VariantType,
        EventType
    };
    enum CustomUserRoles {
        UserRole_Item = Qt::UserRole + 0,
        UserRole_NodeId = Qt::UserRole + 1,
        UserRole_TagApp = Qt::UserRole + 2
    };
    QIcon createColorIcon(const QString &valueType); //генерируем иконку в зависимости от типа данных

public slots:
    void removeItem(const QModelIndex &index);
    void clear();

public:
    void saveMapping();
    void updateModel(const QVariantMap &map); //вызываем в цикле класса Engine()
    void updateHistoryCounter(const QString &tagapp, const quint16 &size);
    void clearSomeItems();

signals:
    void modelCompleted();
    void nodeIdsCompleted(const QList<QString> &nodeIds, const QList<QString> &eventNodeIds, const QList<QString> &tags);
    void modelDataChanged(const QVariantMap &map); //изменение в строке

};

#endif // MODELMAPPING_H
