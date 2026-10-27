/* Модель данных browseNode создаем отдельным классом т.к. импортируем в два виджета */
#ifndef MODELNODES_H
#define MODELNODES_H

#include <QObject>
#include <QStandardItemModel>
#include <QPainter>
#include <QMutex>
#include "opcuaclient.h" //для видимости структуры NodeOPCUA (и структур open62541) и обновления модели

class ModelNodes : public QStandardItemModel
{
    Q_OBJECT

private:
    explicit ModelNodes(QObject *parent = nullptr);
    ~ModelNodes();

    QModelIndex currentIndex;
    mutable QMutex mutex;

    struct NodeClassStyle {
        UA_NodeClass nodeClass;
        const char *color;
        const char *icon;
        const char *label;
    };
    inline static const QList<NodeClassStyle> storeNodeClassStyle = {   //inline разрешаем инициализацию в заголовочном файле С++17
        {UA_NODECLASS_OBJECT,           "#4CAF50",  "📦",   "Object"},         //unicode символы
        {UA_NODECLASS_VARIABLE,         "#2196F3",  "📊",   "Variable"},
        {UA_NODECLASS_METHOD,           "#FF9800",  "⚙️",   "Method"},
        {UA_NODECLASS_OBJECTTYPE,       "#8BC34A",  "📋",   "ObjectType"},
        {UA_NODECLASS_VARIABLETYPE,     "#03A9F4",  "📈",   "VariableType"},
        {UA_NODECLASS_REFERENCETYPE,    "#9C27B0",  "🔗",   "ReferenceType"},
        {UA_NODECLASS_DATATYPE,         "#795548",  "📄",   "DataType"},
        {UA_NODECLASS_VIEW,             "#607D8B",  "👁️",   "View"},
        {UA_NODECLASS_UNSPECIFIED,      "#9E9E9E",  "❓",    "Unknown"}
    };
    struct IdentifierTypeStyle {
        UA_NodeIdType identifierType;
        const char *color;
        const char *label;
    };
    inline static const QList<IdentifierTypeStyle> storeIdentifierType = {
        {UA_NODEIDTYPE_NUMERIC,     "#8BC34A",  "Numeric"},
        {UA_NODEIDTYPE_STRING,      "#2196F3",  "String"},
        {UA_NODEIDTYPE_GUID,        "#795548",  "Guid"},
        {UA_NODEIDTYPE_BYTESTRING,  "#607D8B",  "ByteString"}
    };

    OPCUAClient *opcClient = OPCUAClient::getInstance();

    QIcon createColorIcon(const QColor &color) const;
    const NodeClassStyle *getNodeClassStyle(const UA_NodeClass &nodeClass);
    const IdentifierTypeStyle *getIdentifierTypeStyle(const UA_NodeIdType &identifierType);
    bool checkRepeatData(const QList<NodeOPC> &data); //проверяем, если эти данные уже есть то игнорим
    QModelIndex seachQModelIndex(const QString &nodeId);
    void addItems(const QString &nodeId, const QList<NodeOPC> &data);

public:
    static ModelNodes *getInstance();  //создаем один экземпляр класса (паттерн Singleton)

    //Кастомные роли
    enum NodeRoles {
        browseName = Qt::UserRole + 0,
        nodeId = Qt::UserRole + 1,
        referenceTypeId = Qt::UserRole + 2,
    };

public slots:
    void removeItem(const QModelIndex &index);
    void clear();
    bool checkChildren(const QModelIndex &index); //проверка на дочерние элементы

};

#endif // MODELNODES_H
