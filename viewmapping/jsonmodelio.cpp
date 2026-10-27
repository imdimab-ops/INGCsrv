    #include "jsonmodelio.h"

JsonModelIO::JsonModelIO(QObject *parent)
    : QObject{parent}
{

}

JsonModelIO::~JsonModelIO()
{

}

QJsonArray JsonModelIO::saveRecursiveModel(const QStandardItemModel *model, const QStandardItem *parentItem)
{
    int rowCount = parentItem ? parentItem->rowCount() : model->rowCount();
    int columnCount = parentItem ? parentItem->columnCount() : model->columnCount();

    QJsonArray rowsArray;
    for (int row = 0; row < rowCount; ++row) {
        QJsonObject rowObject;
        QJsonArray cellsArray;

        //обрабатываем все колонки в текущей строке
        for (int column = 0; column < columnCount; ++column) {
            QStandardItem *item = parentItem ? parentItem->child(row, column) : model->item(row, column);
            if (!item || !item->data(Qt::EditRole).isValid()) continue;

            QJsonObject cellObject;
            cellObject["0_column"] = column;
            cellObject["1_text"] = item->text();
            //чекбокс
            cellObject["2_checkable"] = item->isCheckable();
            //сохраняем состояние чекбокса
            if (item->isCheckable()) {
                Qt::CheckState checkState = item->checkState();
                cellObject["2_checkState"] = static_cast<int>(checkState);
            }

            //сохраняем в столбец статуса
            if (column == 5 && item->data(ModelMapping::UserRole_Item).toInt() == ModelMapping::VariantType) cellObject["1_text"] = "...";

            //сохраняем пользовательские данные userData
            QJsonArray userDataArray;
            for (int role = Qt::UserRole; role <= Qt::UserRole + 10; ++role) {
                QVariant userData = item->data(role);
                if (userData.isValid()) {
                    QJsonObject obj;
                    obj["role"] = role;
                    obj["value"] = userData.toString();
                    userDataArray.append(obj);
                }
            }
            //сохраняем ForegroundRole (цвет текста)
            QVariant foreground = item->data(Qt::ForegroundRole);
            if (foreground.isValid() && foreground.canConvert<QBrush>()) {
                QJsonObject obj;
                obj["role"] = Qt::ForegroundRole;
                obj["value"] = foreground.value<QBrush>().color().name();
                userDataArray.append(obj);
            }
            //сохраняем DecorationRole (иконка) - сохраняем идентификатор типа для герации иконки (Boolean, Int16, UInt32, Float, и т.д.)
            QVariant decoration = item->data(Qt::DecorationRole);
            if (decoration.isValid() && decoration.canConvert<QIcon>()) {
                QJsonObject obj;
                obj["role"] = Qt::DecorationRole;
                obj["value"] = item->text();
                userDataArray.append(obj);
            }

            //сохраняем TextAlignmentRole (выравнивание)
            QVariant alignment = item->data(Qt::TextAlignmentRole);
            if (alignment.isValid() && alignment.canConvert<int>()) {
                QJsonObject obj;
                obj["role"] = Qt::TextAlignmentRole;
                obj["value"] = alignment.toInt(); // сохраняем числовое значение
                userDataArray.append(obj);
            }

            cellObject["3_userData"] = userDataArray;

            //рекурсивно обходим дочерние элементы (только для первого столбца)
            if (column == 0 && item->hasChildren()) {
                QJsonArray childrenArray = saveRecursiveModel(model, item);
                if (!childrenArray.isEmpty()) {
                    cellObject["3_children"] = childrenArray;
                }
            }
            cellsArray.append(cellObject);


        }
        rowObject["0_row"] = row;
        rowObject["1_cells"] = cellsArray;
        rowsArray.append(rowObject);
    }
    return rowsArray;
}

ModelMapping *JsonModelIO::parseModel(const QJsonObject &rootObject)
{
    //проверяем тип модели
    if (rootObject["0_modeltype"].toString() != "QStandardItemModel") {
        return nullptr;
    }

    int columnCount = rootObject["3_columnCount"].toInt();
    int rowCount = rootObject["4_rowCount"].toInt();
    Q_UNUSED(rowCount);

    //создаем модель
    ModelMapping *model = ModelMapping::getInstance();
    model->clear();
    model->setColumnCount(columnCount);

    //восстанавливаем заголовки
    QJsonArray headersArray = rootObject["5_headers"].toArray();
    QStringList headers;
    for (const QJsonValue &header : std::as_const(headersArray)) {
        headers.append(header.toString());
    }
    model->setHorizontalHeaderLabels(headers);

    //восстанавливаем данные
    QJsonArray dataArray = rootObject["6_data"].toArray();
    parseRecursiveStrictData(dataArray, model, nullptr); //парсинг всех данных (произвольных решил исключить)
    // parseStrictData(dataArray, model); //парсим только строгую структуру (так проще)
    return model;
}

void JsonModelIO::parseStrictData(const QJsonArray &rowsArray, QStandardItemModel *model)
{
    /* парсим строгую структуру данных из файла для генерации модели,
     * использую, если структура данных в модели JSON строгая (создал ее сам) */

    /* std::as_const чтобы контейнер Qt не был отсоединен при итерации,
     * либо использовать классический for-loop for (int i = 0; i < someArray.size(); ++i) {] */
    //1.парсим группы
    for (const QJsonValue &rowValue : std::as_const(rowsArray)) {
        QJsonObject groupObject = rowValue.toObject();
        QList<QStandardItem*> groupRow;
        uint userRole = groupObject.value("group").toString() == "Event" ? ModelMapping::EventType : ModelMapping::VariantType; //устанавливаем тип узла (сообщение или обычный)
        groupRow.append(new QStandardItem(groupObject["group"].toString()));
        groupRow.append(new QStandardItem(groupObject["name"].toString()));

        QJsonArray paramArray = groupObject["params"].toArray();
        //2.парсим параметры
        for (const QJsonValue &paramValue : std::as_const(paramArray)) {
            QJsonObject paramObject = paramValue.toObject();
            QList<QStandardItem*> paramsRow;
            paramsRow.append(new QStandardItem(paramObject["name"].toString()));
            paramsRow.append(new QStandardItem(paramObject["description"].toString()));

            QJsonArray nodeArray = paramObject["nodes"].toArray();
            //3.парсим узлы
            for (const QJsonValue &nodeValue : std::as_const(nodeArray)) {
                QJsonObject nodeObject = nodeValue.toObject();
                QList<QStandardItem*> nodesRow;
                //DisplayName
                nodesRow.append(new QStandardItem(nodeObject["name"].toString()));
                nodesRow.last()->setData(QBrush(QColor(0x0071bd)), Qt::ForegroundRole); //цвет текста
                QString nodeId = nodeObject["nodeId"].toString();
                nodesRow.last()->setData(nodeId, ModelMapping::UserRole_NodeId); //в метаданные пишем NodeId в первый item, т.к. из него в дальнейшем читаем NodeId
                nodesRow.last()->setData(userRole, ModelMapping::UserRole_Item); //устанавливаем тип узла (сообщение или обычный)
                //NodeId
                nodesRow.append(new QStandardItem(nodeId));
                nodesRow.last()->setData(QBrush(QColor(0x0071bd)), Qt::ForegroundRole); //цвет текста
                nodesRow.last()->setData(userRole, ModelMapping::UserRole_Item); //устанавливаем тип узла (сообщение или обычный)
                //TagApp
                QString tagapp = nodeObject["tagapp"].toString();
                nodesRow.append(new QStandardItem(tagapp));
                nodesRow.last()->setData(QBrush(QColor(0x3333ff)), Qt::ForegroundRole);
                nodesRow.last()->setData(userRole, ModelMapping::UserRole_Item); //устанавливаем тип узла (сообщение или обычный)
                nodesRow.last()->setCheckState(nodeObject["checked"].toBool() ? Qt::Checked : Qt::Unchecked);
                //ValueType
                QString type = nodeObject["type"].toString();
                nodesRow.append(new QStandardItem(type));
                auto *model = ModelMapping::getInstance();
                nodesRow.last()->setData(model->createColorIcon(type), Qt::DecorationRole); //иконка
                nodesRow.last()->setData(userRole, ModelMapping::UserRole_Item); //устанавливаем тип узла (сообщение или обычный)
                //Value
                nodesRow.append(new QStandardItem("..."));
                nodesRow.last()->setTextAlignment(Qt::AlignCenter);
                nodesRow.last()->setData(userRole, ModelMapping::UserRole_Item); //устанавливаем тип узла (сообщение или обычный)
                //Status
                nodesRow.append(new QStandardItem("..."));
                nodesRow.last()->setTextAlignment(Qt::AlignCenter);
                nodesRow.last()->setData(userRole, ModelMapping::UserRole_Item); //устанавливаем тип узла (сообщение или обычный)
                //Timestamp
                nodesRow.append(new QStandardItem("..."));
                nodesRow.last()->setTextAlignment(Qt::AlignCenter);
                nodesRow.last()->setData(userRole, ModelMapping::UserRole_Item); //устанавливаем тип узла (сообщение или обычный)
                //History
                nodesRow.append(new QStandardItem("..."));
                nodesRow.last()->setTextAlignment(Qt::AlignCenter);
                nodesRow.last()->setData(userRole, ModelMapping::UserRole_Item); //устанавливаем тип узла (сообщение или обычный)
                nodesRow.last()->setData(tagapp, ModelMapping::UserRole_TagApp); //в метаданные пишем tagapp, отображение счетчика истории через хэш
                //Description
                nodesRow.append(new QStandardItem("..."));
                nodesRow.last()->setData(userRole, ModelMapping::UserRole_Item); //устанавливаем тип узла (сообщение или обычный)

                paramsRow.first()->appendRow(nodesRow);
            }
            groupRow.first()->appendRow(paramsRow);
        }
        model->appendRow(groupRow);
    }
}

void JsonModelIO::parseRecursiveStrictData(const QJsonValue &jsonValue, QStandardItemModel *model, QStandardItem *parentItem, uint userRole)
{
    /* парсим произвольную структуру c вложенными строигими данными из файла для генерации модели,
     * использую, если структура данных в модели JSON с произвольной вложенной структурой (создал ее сам)
     * @param jsonValue - текущее значение JSON (может быть объектом, массивом или примитивом)
     * @param model - указатель на модель
     * @param parentItem - родительский элемент (если nullptr, добавляем в корень модели)
     * @param userRole - роль пользователя (EventType или VariantType) */

    /* std::as_const чтобы контейнер Qt не был отсоединен при итерации,
     * либо использовать классический for-loop for (int i = 0; i < someArray.size(); ++i) {] */

    switch (jsonValue.type()) {
    case QJsonValue::Array: { //если это массив - обходим все элементы
        QJsonArray array = jsonValue.toArray();
        for (const QJsonValue &value : std::as_const(array)) {
            parseRecursiveStrictData(value, model, parentItem, userRole);
        }
        break;
    }
    case QJsonValue::Object: { //если объект - проверяем на вложенные элементы согласно ключей
        QJsonObject obj = jsonValue.toObject();
        QList<QStandardItem*> itemsRow;

        //проверяем, является ли объект узлом (содержит nodeId)
        if (obj.contains("nodeId")) { //это конечный узел - создаем строку
            uint role = obj.value("type").toString() == "Event" ? ModelMapping::EventType : ModelMapping::VariantType;
            itemsRow = createNodeRow(obj, role);
        } else { //это не узел, а группа/параметр
            if (obj.contains("group")) itemsRow.append(new QStandardItem(obj["group"].toString()));
            if (obj.contains("name")) itemsRow.append(new QStandardItem(obj["name"].toString()));
            if (obj.contains("description")) itemsRow.append(new QStandardItem(obj["description"].toString()));

            //обходим все поля объекта в поисках вложенных массивов
            for (auto it = obj.begin(); it != obj.end(); ++it) {
                QJsonValue value = it.value();
                if (value.isArray() || value.isObject()) {
                    //если нашли вложенную структуру - рекурсивно парсим
                    parseRecursiveStrictData(value, model, itemsRow.first(), userRole);
                }
            }
        }
        //добавляем группу в модель
        if (parentItem) {
            parentItem->appendRow(itemsRow);
        } else {
            model->appendRow(itemsRow);
        }
        break;
    }
    default:
        break;
    }
}

QList<QStandardItem*> JsonModelIO::createNodeRow(const QJsonObject &nodeObject, const uint &role)
{
    QList<QStandardItem*> nodesRow;

    //DisplayName
    nodesRow.append(new QStandardItem(nodeObject["name"].toString()));
    nodesRow.last()->setData(QBrush(QColor(0x0071bd)), Qt::ForegroundRole);
    QString nodeId = nodeObject["nodeId"].toString();
    nodesRow.last()->setData(nodeId, ModelMapping::UserRole_NodeId);
    nodesRow.last()->setData(role, ModelMapping::UserRole_Item);

    //NodeId
    nodesRow.append(new QStandardItem(nodeId));
    nodesRow.last()->setData(QBrush(QColor(0x0071bd)), Qt::ForegroundRole);
    nodesRow.last()->setData(role, ModelMapping::UserRole_Item);

    //TagApp
    QString tagapp = nodeObject["tagapp"].toString();
    nodesRow.append(new QStandardItem(tagapp));
    nodesRow.last()->setData(QBrush(QColor(0x3333ff)), Qt::ForegroundRole);
    nodesRow.last()->setData(role, ModelMapping::UserRole_Item);
    nodesRow.last()->setCheckState(nodeObject["checked"].toBool() ? Qt::Checked : Qt::Unchecked);

    //ValueType
    QString type = nodeObject["type"].toString();
    nodesRow.append(new QStandardItem(type));
    auto *model = ModelMapping::getInstance();
    nodesRow.last()->setData(model->createColorIcon(type), Qt::DecorationRole);
    nodesRow.last()->setData(role, ModelMapping::UserRole_Item);

    //Value
    nodesRow.append(new QStandardItem("..."));
    nodesRow.last()->setTextAlignment(Qt::AlignCenter);
    nodesRow.last()->setData(role, ModelMapping::UserRole_Item);

    //Status
    nodesRow.append(new QStandardItem("..."));
    nodesRow.last()->setTextAlignment(Qt::AlignCenter);
    nodesRow.last()->setData(role, ModelMapping::UserRole_Item);

    //Timestamp
    nodesRow.append(new QStandardItem("..."));
    nodesRow.last()->setTextAlignment(Qt::AlignCenter);
    nodesRow.last()->setData(role, ModelMapping::UserRole_Item);

    //History
    nodesRow.append(new QStandardItem("..."));
    nodesRow.last()->setTextAlignment(Qt::AlignCenter);
    nodesRow.last()->setData(role, ModelMapping::UserRole_Item);
    nodesRow.last()->setData(tagapp, ModelMapping::UserRole_TagApp);

    //Description
    nodesRow.append(new QStandardItem("..."));
    nodesRow.last()->setData(role, ModelMapping::UserRole_Item);

    return nodesRow;
}

void JsonModelIO::parseRecursiveData(const QJsonArray &rowsArray, QStandardItemModel *model, QStandardItem *parentItem)
{
    /* рекурсивно воостанавливаем модель, проходя по всем ячейкам. в проекте решил не использоовать, избыточно!
     * использую, если структура модели в JSON произвольная */

    /* std::as_const чтобы контейнер Qt не был отсоединен при итерации,
     * либо использовать классический for-loop for (int i = 0; i < someArray.size(); ++i) {] */
    for (const QJsonValue &rowValue : std::as_const(rowsArray)) {
        QJsonObject rowObject = rowValue.toObject();
        int row = rowObject["0_row"].toInt();
        Q_UNUSED(row);
        QJsonArray cellsArray = rowObject["1_cells"].toArray(); //массив item
        QList<QStandardItem*> rowItems;

        for (const QJsonValue &cellValue : std::as_const(cellsArray)) {
            QJsonObject cellObject = cellValue.toObject();
            int column = cellObject["0_column"].toInt();

            QStandardItem *item = parseCell(cellObject);
            rowItems.append(item);

            //обрабатываем дочерние элементы (только для первого столбца)
            if (column == 0 && cellObject.contains("3_children")) {
                QJsonArray childrenArray = cellObject["3_children"].toArray();
                parseRecursiveData(childrenArray, model, item);
            }
        }
        //добавляем строку в модель
        if (parentItem) {
            parentItem->appendRow(rowItems);
        } else {
            model->appendRow(rowItems);
        }
    }
}

QStandardItem *JsonModelIO::parseCell(const QJsonObject &cellObject)
{
    QStandardItem *item = new QStandardItem();

    //восстанавливаем основные свойства
    item->setText(cellObject["1_text"].toString());

    //восстанавливаем чекбокс
    item->setCheckable(cellObject["2_checkable"].toBool());
    if (cellObject.contains("2_checkState")) {
        Qt::CheckState checkState = static_cast<Qt::CheckState>(cellObject["2_checkState"].toInt());
        item->setCheckState(checkState);
    }

    //восстанавливаем пользовательские данные
    if (cellObject.contains("3_userData") && cellObject["3_userData"].isArray()) {
        QJsonArray userDataArray = cellObject["3_userData"].toArray();
        for (const QJsonValue &userDataValue : std::as_const(userDataArray)) {

            QJsonObject userDataObj = userDataValue.toObject();
            int role = userDataObj["role"].toInt();
            QVariant value = userDataObj["value"].toVariant();
            if (role == Qt::ForegroundRole) {
                //восстанавливаем цвет текста
                QColor color(value.toString());
                item->setData(QBrush(color), Qt::ForegroundRole);
            }
            else if (role == Qt::DecorationRole) {
                //восстанавливаем иконку
                auto *model = ModelMapping::getInstance();
                item->setData(model->createColorIcon(value.toString()), Qt::DecorationRole);
            }
            else if (role == Qt::TextAlignmentRole) {
                //восстанавливаем TextAlignmentRole (выравнивание)
                item->setTextAlignment(Qt::Alignment(value.toInt()));
            }
            else if (Qt::UserRole <= role && role <= Qt::UserRole + 10) {
                //восстанавливаем UserRole данные
                item->setData(value, role);
            }
        }
    }
    return item;
}

bool JsonModelIO::saveModelToJson(const QStandardItemModel *model)
{
    QJsonObject rootObject;
    rootObject["0_modeltype"] = "QStandardItemModel";
    rootObject["1_datetime"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    rootObject["2_info"] = "NodeId для создания вектора запроса OPCUA считываем из метаданных (userdata) "
                           "DisplayName роль Qt::UserRole + 1 (257)";
    rootObject["3_columnCount"] = model->columnCount();
    rootObject["4_rowCount"] = model->rowCount();

    //сохраняем заголовки столбцов
    QJsonArray headers;
    for (int col = 0; col < model->columnCount(); ++col) {
        headers.append(model->headerData(col, Qt::Horizontal).toString());
    }
    rootObject["5_headers"] = headers;

    //сохраняем данные модели
    rootObject["6_data"] = saveRecursiveModel(model, nullptr);

    //записываем в файл
    qint64 byteswrite = FileIO::saveToFile(FileIO::getConfigDir() + "mapping.json", QJsonDocument(rootObject).toJson(QJsonDocument::Indented)); //Indented, Compact - формат записи
    if (byteswrite != 0) return true;
    return false;
}

ModelMapping *JsonModelIO::loadModelFromJson()
{
    QJsonParseError parseError;
    QString filename = "mapping.json";
    QJsonDocument doc = QJsonDocument::fromJson(FileIO::loadFromFile(FileIO::getConfigDir() + filename), &parseError);

    //проверяем на ошибки парсинга
    if (parseError.error != QJsonParseError::NoError) {
        auto *modelSysLog = ModelSystemLog::getInstance();
        modelSysLog->append(Q_FUNC_INFO, QString("[Error] JSON parse error in %1 at offset %2: %3")
                                             .arg(filename, QString::number(parseError.offset), parseError.errorString()));
        return nullptr;
    }
    if (doc.isNull() || !doc.isObject()) {
        auto *modelSysLog = ModelSystemLog::getInstance();
        modelSysLog->append(Q_FUNC_INFO, QString("[Error] JSON isNull or !isObject in file: %1, %2").arg(filename));
        return nullptr;
    }
    QJsonObject rootObject = doc.object();
    return parseModel(rootObject);
}

ModelMapping *JsonModelIO::loadModelFromJson(const QString &filename)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(FileIO::loadFromFile(filename), &parseError);

    //проверяем на ошибки парсинга
    if (parseError.error != QJsonParseError::NoError) {
        auto *modelSysLog = ModelSystemLog::getInstance();
        modelSysLog->append(Q_FUNC_INFO, QString("[Error] JSON parse error in %1 at offset %2: %3")
                                             .arg(filename, QString::number(parseError.offset), parseError.errorString()));
        return nullptr;
    }
    if (doc.isNull() || !doc.isObject()) {
        auto *modelSysLog = ModelSystemLog::getInstance();
        modelSysLog->append(Q_FUNC_INFO, QString("[Error] JSON isNull or !isObject in file: %1, %2").arg(filename));
        return nullptr;
    }
    QJsonObject rootObject = doc.object();
    return parseModel(rootObject);
}
