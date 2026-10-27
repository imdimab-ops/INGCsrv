#include "modelattributes.h"

ModelAttributes::ModelAttributes(QObject *parent)
    : QStandardItemModel{parent}
{
    this->setHorizontalHeaderLabels({"Attribute", "Value"});

    /* Добавляем NodeAttribute &attribute в модель, данный сигнал вызывается в цикле обработки получения данных */
    connect(opcClient, &OPCUAClient::attributeCompleted, this, [this] (const QString &nodeId, const QList<NodeAttribute> &data) {
        addItem(nodeId, data);
    });
    connect(opcClient, &OPCUAClient::connected, this, [this] (const bool &b) { if (!b) clear(); });
}

ModelAttributes::~ModelAttributes()
{

}

ModelAttributes *ModelAttributes::getInstance()
{
    static ModelAttributes instance;
    return &instance;
}

void ModelAttributes::addItem(const QString &nodeId, const QList<NodeAttribute> &data)
{
    clear(); //перед заполнением очищаем модель

    for (auto i = 0; i < data.size(); i++) {
        // Создаем родительскую строку для Value
        QList<QStandardItem*> parentRow;
        parentRow.append(new QStandardItem()); // Аттрибут
        parentRow.append(new QStandardItem()); // Основное значение

        switch(data[i].attributeId) {
        case UA_ATTRIBUTEID_NODEID: {
            parentRow.first()->setData("NodeId", Qt::DisplayRole);
            parentRow.last()->setData("", Qt::DisplayRole);
            attributedNodeId(parentRow, data[i].attributeId, data[i].results);
            break;
        }
        case UA_ATTRIBUTEID_NODECLASS: {
            parentRow.first()->setData("NodeClass", Qt::DisplayRole);
            UA_Int32 *nodeClass = static_cast<UA_Int32*>(data[i].results.value.data);
            if (!nodeClass) break;
            parentRow.last()->setData(QString("%1 (i=%2)").arg(parseNodeClass(*nodeClass), QVariant::fromValue(*nodeClass).toString()), Qt::DisplayRole);
            break;
        }
        case UA_ATTRIBUTEID_BROWSENAME: {
            parentRow.first()->setData("BrowseName", Qt::DisplayRole);
            UA_QualifiedName *browseName = static_cast<UA_QualifiedName*>(data[i].results.value.data);
            if (!browseName || UA_String_isEmpty(&browseName->name)) break;
            parentRow.last()->setData(opcClient->toQString(browseName->name), Qt::DisplayRole);
            break;
        }
        case UA_ATTRIBUTEID_DISPLAYNAME: {
            parentRow.first()->setData("DisplayName", Qt::DisplayRole);
            UA_LocalizedText *displayName = static_cast<UA_LocalizedText*>(data[i].results.value.data);
            if (!displayName || UA_String_isEmpty(&displayName->text)) break;
            parentRow.last()->setData(opcClient->toQString(displayName->text), Qt::DisplayRole);
            attributes.displayName = parentRow.last()->text();
            break;
        }
        case UA_ATTRIBUTEID_DESCRIPTION: {
            parentRow.first()->setData("Description", Qt::DisplayRole);
            UA_LocalizedText *description = static_cast<UA_LocalizedText*>(data[i].results.value.data);
            if (!description || UA_String_isEmpty(&description->text)) break;
            parentRow.last()->setData(opcClient->toQString(description->text), Qt::DisplayRole);
            attributes.description = parentRow.last()->text();
            break;
        }
        case UA_ATTRIBUTEID_DATATYPE: {
            parentRow.first()->setData("DataType", Qt::DisplayRole);
            UA_NodeId *dataType = static_cast<UA_NodeId*>(data[i].results.value.data);
            if (!dataType || UA_NodeId_isNull(dataType)) break;
            parentRow.last()->setData(QString("%1 %2").arg(parseDataType(dataType->identifier.numeric), opcClient->toQString(*dataType)), Qt::DisplayRole);
            attributes.dataTypeId = static_cast<int>(dataType->identifier.numeric);
            break;
        }
        case UA_ATTRIBUTEID_VALUE: {
            parentRow.first()->setData("Value", Qt::DisplayRole);
            parentRow.last()->setData("", Qt::DisplayRole);
            attributedValue(parentRow, data[i].attributeId, data[i].results);
            break;
        }
        case UA_ATTRIBUTEID_WRITEMASK: {
            parentRow.first()->setData("WriteMask", Qt::DisplayRole);
            parentRow.last()->setData("", Qt::DisplayRole);
            UA_UInt32 *writeMask = (UA_UInt32*)data[i].results.value.data;
            if (!writeMask) break;

            // Дочерние элементы
            {
                QList<QStandardItem*> writeMaskRow;
                writeMaskRow.append(new QStandardItem("WriteMask"));
                writeMaskRow.append(new QStandardItem());
                writeMaskRow.last()->setData(QVariant::fromValue(*writeMask), Qt::DisplayRole);
                parentRow.first()->appendRow(writeMaskRow);

                // Декодируем битовую маску в читаемый текст
                QString writeMaskStr = parseWriteMask(*writeMask);
                if (!writeMaskStr.isEmpty()) {
                    QList<QStandardItem*> writeMaskTextRow;
                    writeMaskTextRow.append(new QStandardItem("WriteMask Text"));
                    writeMaskTextRow.append(new QStandardItem(writeMaskStr));
                    parentRow.first()->appendRow(writeMaskTextRow);
                }

                // Дополнительно: показываем отдельные биты
                QList<QStandardItem*> writeMaskBitsRow;
                writeMaskBitsRow.append(new QStandardItem("WriteMask Bits"));
                writeMaskBitsRow.append(new QStandardItem(QString("0x%1").arg(*writeMask, 8, 16, QChar('0'))));
                parentRow.first()->appendRow(writeMaskBitsRow);
            }
            break;
        }
        case UA_ATTRIBUTEID_USERWRITEMASK: {
            parentRow.first()->setData("UserWriteMask", Qt::DisplayRole);
            parentRow.last()->setData("", Qt::DisplayRole);
            UA_UInt32 *userWriteMask = (UA_UInt32*)data[i].results.value.data;
            if (!userWriteMask) break;

            QList<QStandardItem*> writeMaskRow;
            writeMaskRow.append(new QStandardItem("UserWriteMask"));
            writeMaskRow.append(new QStandardItem());
            writeMaskRow.last()->setData(QVariant::fromValue(*userWriteMask), Qt::DisplayRole);
            parentRow.first()->appendRow(writeMaskRow);

            QString writeMaskStr = parseWriteMask(*userWriteMask);
            if (!writeMaskStr.isEmpty()) {
                QList<QStandardItem*> writeMaskTextRow;
                writeMaskTextRow.append(new QStandardItem("UserWriteMask Text"));
                writeMaskTextRow.append(new QStandardItem(writeMaskStr));
                parentRow.first()->appendRow(writeMaskTextRow);
            }
            break;
        }

        case UA_ATTRIBUTEID_ARRAYDIMENSIONS: {
            parentRow.first()->setData("ArrayDimensions", Qt::DisplayRole);
            parentRow.last()->setData("", Qt::DisplayRole);
            attributedArrayDimensions(parentRow, data[i].attributeId, data[i].results);
            break;
        }
        case UA_ATTRIBUTEID_ACCESSLEVEL: {
            parentRow.first()->setData("AccessLevel", Qt::DisplayRole);
            parentRow.last()->setData("", Qt::DisplayRole);
            UA_Byte *accessLevel = (UA_Byte*)data[i].results.value.data;
            if (!accessLevel) break;
            parentRow.last()->setData(parseAccessRights(*accessLevel), Qt::DisplayRole);
            break;
        }
        case UA_ATTRIBUTEID_USERACCESSLEVEL: {
            parentRow.first()->setData("UserAccessLevel", Qt::DisplayRole);
            parentRow.last()->setData("", Qt::DisplayRole);
            UA_Byte *accessLevel = (UA_Byte*)data[i].results.value.data;
            if (!accessLevel) break;
            parentRow.last()->setData(parseAccessRights(*accessLevel), Qt::DisplayRole);
            break;
        }
        case UA_ATTRIBUTEID_EVENTNOTIFIER: {
            parentRow.first()->setData("EventNotifier", Qt::DisplayRole);
            parentRow.last()->setData("", Qt::DisplayRole);
            UA_Byte *value  = (UA_Byte*)data[i].results.value.data;
            if (!value ) break;

            QString displayText;
            switch (*value) {
            case 0x00:
                displayText = QString("(0x%1) No Events").arg(*value, 2, 16, QChar('0'));
                break;
            case 0x01:
                displayText = QString("(0x%1) Generate Only").arg(*value, 2, 16, QChar('0'));
                break;
            case 0x02:
                displayText = QString("(0x%1) Subscribe Only").arg(*value, 2, 16, QChar('0'));
                break;
            case 0x03:
                displayText = QString("(0x%1) Generate & Subscribe").arg(*value, 2, 16, QChar('0'));
                break;
            default:
                displayText = QString("(0x%1) Custom").arg(*value, 2, 16, QChar('0'));
                break;
            }
            parentRow.last()->setData(displayText, Qt::DisplayRole);
            break;
        }
        default:
            qDebug() << "Unhandled attribute:" << data[i].attributeId;
            break;
        }

        // Добавляем только если есть данные
        if (!parentRow.first()->data(Qt::DisplayRole).isNull() && !parentRow.last()->data(Qt::DisplayRole).isNull()) {
            this->appendRow(parentRow);
        } else {
            qDeleteAll(parentRow);
        }
    }
    emit modelCompleted();
}

void ModelAttributes::attributedNodeId(QList<QStandardItem *> &parentRow, const UA_AttributeId &attributeId, const UA_DataValue &results)
{
    const UA_NodeId *nodeId = static_cast<UA_NodeId*>(results.value.data);
    if (parentRow.isEmpty() || attributeId != UA_ATTRIBUTEID_NODEID || UA_NodeId_isNull(nodeId)) return;

    // Дочерние элементы
    // Основная информация о NodeId
    QList<QStandardItem*> nodeIdRow;
    nodeIdRow.append(new QStandardItem("NodeId"));
    nodeIdRow.append(new QStandardItem());

    // Преобразуем NodeId в строку
    nodeIdRow.last()->setData(opcClient->toQString(*nodeId), Qt::DisplayRole);
    nodeIdRow.last()->setData(QBrush(QColor(0, 113, 189)), Qt::ForegroundRole);   //цвет текста
    parentRow.first()->appendRow(nodeIdRow);
    attributes.nodeId = nodeIdRow.last()->text();

    // Детальная информация о NodeId
    QList<QStandardItem*> namespaceIndexRow;
    namespaceIndexRow.append(new QStandardItem("NamespaceIndex"));
    namespaceIndexRow.append(new QStandardItem(QString::number(nodeId->namespaceIndex)));
    parentRow.first()->appendRow(namespaceIndexRow);

    // В зависимости от типа идентификатора
    QList<QStandardItem*> identifierTypeRow;
    identifierTypeRow.append(new QStandardItem("IdentifierType"));

    switch (nodeId->identifierType) {
    case UA_NODEIDTYPE_NUMERIC: {
        identifierTypeRow.append(new QStandardItem("Numeric"));
        parentRow.first()->appendRow(identifierTypeRow);

        QList<QStandardItem*> numericIdRow;
        numericIdRow.append(new QStandardItem("NumericId"));
        numericIdRow.append(new QStandardItem(QString::number(nodeId->identifier.numeric)));
        parentRow.first()->appendRow(numericIdRow);
        break;
    }
    case UA_NODEIDTYPE_STRING: {
        identifierTypeRow.append(new QStandardItem("String"));
        parentRow.first()->appendRow(identifierTypeRow);

        QList<QStandardItem*> stringIdRow;
        stringIdRow.append(new QStandardItem("StringId"));
        stringIdRow.append(new QStandardItem(opcClient->toQString(nodeId->identifier.string)));
        parentRow.first()->appendRow(stringIdRow);
        break;
    }
    case UA_NODEIDTYPE_GUID: {
        identifierTypeRow.append(new QStandardItem("GUID"));
        parentRow.first()->appendRow(identifierTypeRow);

        QList<QStandardItem*> guidRow;
        guidRow.append(new QStandardItem("GUID"));
        const UA_Guid *guid = &nodeId->identifier.guid;
        QString guidStr = QString("{%1-%2-%3-%4-%5}")
                              .arg(QString::number(guid->data1, 16).rightJustified(8, '0'),
                                   QString::number(guid->data2, 16).rightJustified(4, '0'),
                                   QString::number(guid->data3, 16).rightJustified(4, '0'),
                                   QByteArray((char*)guid->data4, 2).toHex(),
                                   QByteArray((char*)guid->data4 + 2, 6).toHex());
        guidRow.append(new QStandardItem(guidStr));
        parentRow.first()->appendRow(guidRow);
        break;
    }
    case UA_NODEIDTYPE_BYTESTRING: {
        identifierTypeRow.append(new QStandardItem("ByteString"));
        parentRow.first()->appendRow(identifierTypeRow);

        QList<QStandardItem*> byteStringRow;
        byteStringRow.append(new QStandardItem("ByteString"));
        const UA_ByteString *uaByteString = &nodeId->identifier.byteString;
        QByteArray byteArray((char*)uaByteString->data, uaByteString->length);
        byteStringRow.append(new QStandardItem(byteArray.toHex()));
        parentRow.first()->appendRow(byteStringRow);
        break;
    }
    default: {
        identifierTypeRow.append(new QStandardItem("Unknown"));
        parentRow.first()->appendRow(identifierTypeRow);
        break;
    }
    }
}

void ModelAttributes::attributedValue(QList<QStandardItem*> &parentRow, const UA_AttributeId &attributeId, const UA_DataValue &results)
{
    if (parentRow.isEmpty() || attributeId != UA_ATTRIBUTEID_VALUE) return;
    if(!results.value.type || !results.value.data) return;

    // Дочерние элементы
    {
        // Value
        QList<QStandardItem*> valueRow;
        valueRow.append(new QStandardItem("Value"));
        valueRow.append(new QStandardItem(variantToString(opcClient->toQVariant(results.value))));
        parentRow.first()->appendRow(valueRow);

        // Статус
        QList<QStandardItem*> statusRow;
        statusRow.append(new QStandardItem("Status"));
        statusRow.append(new QStandardItem(QString::fromUtf8(UA_StatusCode_name(results.status))));
        parentRow.first()->appendRow(statusRow);

        // Source Timestamp
        QList<QStandardItem*> sourceTimeRow;
        sourceTimeRow.append(new QStandardItem("Source Timestamp"));
        if (results.hasSourceTimestamp) {
            QDateTime sourceTime = opcClient->toQDateTime(results.sourceTimestamp);
            sourceTimeRow.append(new QStandardItem(sourceTime.toString("dd.MM.yyyy HH:mm:ss.zzz")));
        }
        parentRow.first()->appendRow(sourceTimeRow);

        // Server Timestamp
        QList<QStandardItem*> serverTimeRow;
        serverTimeRow.append(new QStandardItem("Server Timestamp"));
        if (results.hasServerTimestamp) {
            QDateTime serverTime = opcClient->toQDateTime(results.serverTimestamp);
            serverTimeRow.append(new QStandardItem(serverTime.toString("dd.MM.yyyy HH:mm:ss.zzz")));
        }
        parentRow.first()->appendRow(serverTimeRow);

        // Дополнительная информация о значении
        QList<QStandardItem*> typeRow;
        typeRow.append(new QStandardItem("Value Type"));
        typeRow.append(new QStandardItem(QString(results.value.type->typeName)));
        parentRow.first()->appendRow(typeRow);
        attributes.valueType = typeRow.last()->text();
    }
}

void ModelAttributes::attributedArrayDimensions(QList<QStandardItem *> &parentRow, const UA_AttributeId &attributeId, const UA_DataValue &results)
{
    if (parentRow.isEmpty() || attributeId != UA_ATTRIBUTEID_ARRAYDIMENSIONS) return;

    UA_UInt32 *arrayDims = static_cast<UA_UInt32*>(results.value.data);
    if (!arrayDims || results.value.arrayLength == 0) {
        // Скалярное значение (не массив)
        QList<QStandardItem*> arrayDimRow;
        arrayDimRow.append(new QStandardItem("Array Dimensions"));
        arrayDimRow.append(new QStandardItem("Scalar"));
        parentRow.first()->appendRow(arrayDimRow);
    } else {
        // Массив - показываем размерности
        QList<QStandardItem*> arrayDimRow;
        arrayDimRow.append(new QStandardItem("Array Dimensions"));

        // Формируем строку с размерностями [dim1][dim2]...
        QString dimensions;
        for (size_t i = 0; i < results.value.arrayLength; i++) {
            if (0 < i && i < 25) dimensions += " x ";   //ограничим в 25 элементов
            dimensions += QString::number(arrayDims[i]);
        }
        dimensions = "[" + dimensions + "]";

        arrayDimRow.append(new QStandardItem(dimensions));
        parentRow.first()->appendRow(arrayDimRow);

        // Дополнительная информация
        QList<QStandardItem*> arrayLengthRow;
        arrayLengthRow.append(new QStandardItem("Array Length"));
        arrayLengthRow.append(new QStandardItem(QString::number(results.value.arrayLength)));
        parentRow.first()->appendRow(arrayLengthRow);
    }
}

QString ModelAttributes::parseDataType(const UA_UInt32 &numeric)
{
    /* UA_NS0ID_* - это NodeId узлов типов в OPC UA пространстве имен (+1)
     * UA_TYPES_* - это индексы в массиве типов open62541 */
    switch (numeric) {
    case UA_NS0ID_BOOLEAN: return "Boolean";
    case UA_NS0ID_SBYTE: return "SByte";
    case UA_NS0ID_BYTE: return "Byte";
    case UA_NS0ID_INT16: return "Int16";
    case UA_NS0ID_UINT16: return "UInt16";
    case UA_NS0ID_INT32: return "Int32";
    case UA_NS0ID_UINT32: return "UInt32";
    case UA_NS0ID_INT64: return "Int64";
    case UA_NS0ID_UINT64: return "UInt64";
    case UA_NS0ID_FLOAT: return "Float";
    case UA_NS0ID_DOUBLE: return "Double";
    case UA_NS0ID_STRING: return "String";
    case UA_NS0ID_DATETIME: return "DateTime";
    case UA_NS0ID_GUID: return "Guid";
    case UA_NS0ID_BYTESTRING: return "ByteString";
    case UA_NS0ID_XMLELEMENT: return "XmlElement";
    case UA_NS0ID_NODEID: return "NodeId";
    case UA_NS0ID_EXPANDEDNODEID: return "ExpandedNodeId";
    case UA_NS0ID_STATUSCODE: return "StatusCode";
    case UA_NS0ID_QUALIFIEDNAME: return "QualifiedName";
    case UA_NS0ID_LOCALIZEDTEXT: return "LocalizedText";
    case UA_NS0ID_STRUCTURE: return "Structure";
    case UA_NS0ID_DATAVALUE: return "DataValue";
    case UA_NS0ID_BASEDATATYPE: return "BaseDataType";
    case UA_NS0ID_DIAGNOSTICINFO: return "DiagnosticInfo";
    case UA_NS0ID_DECIMAL: return "Decimal";
    default: return QString("Unknown(%1)").arg(numeric);
    }
}

QString ModelAttributes::parseNodeClass(const UA_Int32 &nodeClass)    // Вспомогательная функция для преобразования NodeClass в текст
{
    switch(nodeClass) {
    case UA_NODECLASS_OBJECT: return "Object";
    case UA_NODECLASS_VARIABLE: return "Variable";
    case UA_NODECLASS_METHOD: return "Method";
    case UA_NODECLASS_OBJECTTYPE: return "ObjectType";
    case UA_NODECLASS_VARIABLETYPE: return "VariableType";
    case UA_NODECLASS_REFERENCETYPE: return "ReferenceType";
    case UA_NODECLASS_DATATYPE: return "DataType";
    case UA_NODECLASS_VIEW: return "View";
    default: return QString::number(nodeClass);
    }
}

QString ModelAttributes::parseWriteMask(const UA_UInt32 &writeMask)
{
    QStringList permissions;
    // Стандартные биты WriteMask из спецификации OPC UA
    if (writeMask & UA_WRITEMASK_ACCESSLEVEL) permissions << "AccessLevel";
    if (writeMask & UA_WRITEMASK_ARRRAYDIMENSIONS) permissions << "ArrayDimensions";
    if (writeMask & UA_WRITEMASK_BROWSENAME) permissions << "BrowseName";
    if (writeMask & UA_WRITEMASK_CONTAINSNOLOOPS) permissions << "ContainsNoLoops";
    if (writeMask & UA_WRITEMASK_DATATYPE) permissions << "DataType";
    if (writeMask & UA_WRITEMASK_DESCRIPTION) permissions << "Description";
    if (writeMask & UA_WRITEMASK_DISPLAYNAME) permissions << "DisplayName";
    if (writeMask & UA_WRITEMASK_EVENTNOTIFIER) permissions << "EventNotifier";
    if (writeMask & UA_WRITEMASK_EXECUTABLE) permissions << "Executable";
    if (writeMask & UA_WRITEMASK_HISTORIZING) permissions << "Historizing";
    if (writeMask & UA_WRITEMASK_INVERSENAME) permissions << "InverseName";
    if (writeMask & UA_WRITEMASK_ISABSTRACT) permissions << "IsAbstract";
    if (writeMask & UA_WRITEMASK_MINIMUMSAMPLINGINTERVAL) permissions << "MinimumSamplingInterval";
    if (writeMask & UA_WRITEMASK_NODECLASS) permissions << "NodeClass";
    if (writeMask & UA_WRITEMASK_NODEID) permissions << "NodeId";
    if (writeMask & UA_WRITEMASK_SYMMETRIC) permissions << "Symmetric";
    if (writeMask & UA_WRITEMASK_USERACCESSLEVEL) permissions << "UserAccessLevel";
    if (writeMask & UA_WRITEMASK_USEREXECUTABLE) permissions << "UserExecutable";
    if (writeMask & UA_WRITEMASK_USERWRITEMASK) permissions << "UserWriteMask";
    if (writeMask & UA_WRITEMASK_VALUEFORVARIABLETYPE) permissions << "ValueForVariableType";
    if (writeMask & UA_WRITEMASK_VALUERANK) permissions << "ValueRank";
    if (writeMask & UA_WRITEMASK_WRITEMASK) permissions << "WriteMask";

    if (permissions.isEmpty()) {
        return "None";
    }
    return permissions.join(" | ");
}

QString ModelAttributes::parseAccessRights(const UA_Byte &level)
{
    QStringList rights;
    if (level & UA_ACCESSLEVELMASK_READ) rights << "Read";
    if (level & UA_ACCESSLEVELMASK_WRITE) rights << "Write";
    if (level & UA_ACCESSLEVELMASK_HISTORYREAD) rights << "HistoryRead";
    if (level & UA_ACCESSLEVELMASK_HISTORYWRITE) rights << "HistoryWrite";
    if (level & UA_ACCESSLEVELMASK_SEMANTICCHANGE) rights << "SemanticChange";
    if (level & UA_ACCESSLEVELMASK_STATUSWRITE) rights << "StatusWrite";
    if (level & UA_ACCESSLEVELMASK_TIMESTAMPWRITE) rights << "TimestampWrite";
    return rights.isEmpty() ? "None" : rights.join(", ");
}

QString ModelAttributes::variantToString(const QVariant &v)
{
    /* Если в QVariant лежит Float 32бита, при преобразовании в строку toString()
     * метод преобразует в тип данных Double 64бита, и после преобразования получаем длинный остаток
     * потерю точности, поэтому преобразуем через данную функцию */
    if (v.typeId() == QMetaType::Float) {
        QString result = QString::number(v.toFloat(), 'f', 4); //'f'-fixed - количество знаков после запятой
        //удаляем trailing zeros
        while (result.endsWith('0')) {
            result.chop(1);
        }
        if (result.endsWith('.')) result.chop(1);
        return result;
    }
    return v.toString();
}

QVariantMap ModelAttributes::getAttribbutes()
{
    QVariantMap map;
    map["nodeId"] = attributes.nodeId;
    map["displayName"] = attributes.displayName;
    map["valueType"] = attributes.valueType;
    map["dataTypeId"] = attributes.dataTypeId; //int
    map["description"] = attributes.description;
    return map;
}

void ModelAttributes::removeItem(const QModelIndex &index)
{
    QMutexLocker locker(&mutex); //блокируем на выполнение данной операции
    if (!index.isValid()) return;
    QStandardItem *item = this->itemFromIndex(index);
    if (!item) return;
    QStandardItem *parentItem = item->parent();
    if (parentItem) {
        parentItem->removeRow(item->row());
    } else {
        this->removeRow(item->row());
    }
}

void ModelAttributes::clear()
{
    QMutexLocker locker(&mutex); //блокируем на выполнение данной операции
    /* очищаем так, если через QStandardItemModel::clear(); очистит все, в том числе и заголовок */
    if (this->rowCount() > 0) this->removeRows(0, this->rowCount());
    attributes = Attributes(); //обнуляем структуру хранения атрибутов
}

