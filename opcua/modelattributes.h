/* Модель данных Attributes Node */
#ifndef MODELATTRIBUTES_H
#define MODELATTRIBUTES_H

#include <QObject>
#include <QStandardItemModel>
#include <QMutex>
#include "opcuaclient.h" //для видимости структур open62541

class ModelAttributes : public QStandardItemModel
{
    Q_OBJECT

private:
    explicit ModelAttributes(QObject *parent = nullptr);
    ~ModelAttributes();

    mutable QMutex mutex;

    //структура хранения данных для передачи Drag&Drop
    struct Attributes {
        Attributes() : dataTypeId(0) {} //конструктор, для обнуления структуры
        QString nodeId;
        QString displayName;
        QString valueType;
        int dataTypeId = 0;
        QString description;
    };
    Attributes attributes;

    OPCUAClient *opcClient = OPCUAClient::getInstance();

    void addItem(const QString &nodeId, const QList<NodeAttribute> &data);
    void attributedNodeId(QList<QStandardItem*> &parentRow, const UA_AttributeId &attributeId, const UA_DataValue &results);    //аттрибуты nodeId
    void attributedValue(QList<QStandardItem*> &parentRow, const UA_AttributeId &attributeId, const UA_DataValue &results);     //аттрибуты Value
    void attributedArrayDimensions(QList<QStandardItem*> &parentRow, const UA_AttributeId &attributeId, const UA_DataValue &results);

    QString parseDataType(const UA_UInt32 &numeric);
    QString parseNodeClass(const UA_Int32 &nodeClass);
    QString parseWriteMask(const UA_UInt32 &writeMask);
    QString parseAccessRights(const UA_Byte &level); //парсим битовую маску
    QString variantToString(const QVariant &v);

public:
    static ModelAttributes *getInstance();  //создаем один экземпляр класса (паттерн Singleton)
    QVariantMap getAttribbutes();

public slots:
    void removeItem(const QModelIndex &index);
    void clear();

signals:
    void modelCompleted();
};

#endif // MODELATTRIBUTES_H
