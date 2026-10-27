#include "modelmapping.h"

ModelMapping::ModelMapping(QObject *parent)
    : QStandardItemModel{parent}
{
    this->setHorizontalHeaderLabels({"DisplayName", "NodeId", "TagApp", "ValueType", "Value",
                                     "Status", "Timestamp", "History", "Description", ""});
    connect(this, &ModelMapping::modelCompleted, this, [this]() {
    });
}

ModelMapping::~ModelMapping()
{

}

Qt::ItemFlags ModelMapping::flags(const QModelIndex &index) const
{
    Qt::ItemFlags defaultFlags = QStandardItemModel::flags(index);
    if (index.isValid()) {
        if (index.column() == 8) return defaultFlags; //разрешаем редактирование по столбцу
    }
    return defaultFlags & ~Qt::ItemIsEditable; //Всегда убираем флаг редактирования
}

ModelMapping *ModelMapping::getInstance()
{
    static ModelMapping instance;
    return &instance;
}

void ModelMapping::saveRecursivelyHash(QStandardItemModel *model, QStandardItem *parentItem, uint &depth)
{
    if (!model || depth > MAX_RECURSION_DEPTH) return;
    int rowCount = parentItem ? parentItem->rowCount() : model->rowCount();
    int columnCount = parentItem ? parentItem->columnCount() : model->columnCount();

    for (int row = 0; row < rowCount; ++row) {
        QString nodeId;
        //Обрабатываем все колонки в текущей строке
        for (int column = 0; column < columnCount; ++column) {
            QStandardItem *item = parentItem ? parentItem->child(row, column) : model->item(row, column);
            if (!item || !item->data(Qt::EditRole).isValid()) continue;
            //item все еще в модели
            QModelIndex index = model->indexFromItem(item);
            if (!index.isValid()) continue;

            //Сохраняем VariantType и EventType в хэш
            if (item->data(UserRole_Item).toInt() == VariantType || item->data(UserRole_Item).toInt() == EventType) {
                //NodeId для ключа хэша берем из метаданных ячейки (роль 1, где хранится NodeId) для простоты
                if (item->data(UserRole_NodeId).isValid()) nodeId = item->data(UserRole_NodeId).toString();
                //добавляем в хэш всю строку для обновления модели по текущему nodeId
                if (!nodeId.isEmpty()) hmodel[nodeId].append(item);
                //добавили в хэш нужный item, для обновления счетчика исторических данных
                if (item->data(UserRole_TagApp).isValid()) hitemHistory.insert(item->data(UserRole_TagApp).toString(), item);
            }
            //Рекурсивно обходим дочерние элементы ЭТОГО элемента
            if (item->hasChildren()) {
                depth++;
                saveRecursivelyHash(model, item, depth);
            }
        }
    }
}

void ModelMapping::updateHash(std::function<void (bool, uint)> callback)
{
    /* обновляем хэш с обратным вызовом callback с задержкой (иначе модель не успевает обновиться)
     * 1.очищаем хэш (если хэш пустой модель не обновляем)
     * 2.через задержку (чтобы успела выстроиться вся модель) обновляем хэш
     * 3.в callback выполняем все необходимые дальнейшие функции */
    QMutexLocker locker(&mutex); //блокируем до завершения выполнения данного метода
    hmodel.clear();
    hitemHistory.clear();

    ModelMapping *self = this;  //сохраняем указатель, т.к. к моменту вызова this может быть не валиден (гипотетически, анализатор ругается)
    QTimer::singleShot(50, this, [self, callback]() {
        QMutexLocker locker(&self->mutex);
        if (self) {
            uint depth = 0; //количество рекурсивных вызовов
            self->saveRecursivelyHash(self, nullptr, depth);
            //Вызываем callback
            if (callback) callback(!self->hmodel.isEmpty(), self->hmodel.size());
            // qDebug() << Q_FUNC_INFO << depth << self->hmodel.size() << self->hitemHistory;
        }
    });
}

QIcon ModelMapping::createColorIcon(const QString &valueType)
{
    static QHash<QString, QColor> hcolor = {
        {"Boolean", QColor(171, 239, 237)}, //бледно-синий
        {"SByte", QColor(78, 205, 196)},    //бирюзовый
        {"Byte", QColor(78, 205, 196)},     //бирюзовый
        {"Int16", QColor(139, 195, 74)},    //зеленый
        {"UInt16", QColor(139, 195, 74)},
        {"Int32", QColor(139, 195, 74)},
        {"UInt32", QColor(139, 195, 74)},
        {"Int64", QColor(139, 195, 74)},
        {"UInt64", QColor(139, 195, 74)},
        {"Float", QColor(255, 159, 67)},    //оранжевый
        {"Double", QColor(255, 159, 67)},   //оранжевый
        {"String", QColor(33, 150, 243)},   //синий
        {"DateTime", QColor(238, 90, 36)}   //коричневый
    };

    QPixmap pixmap(16, 16);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(hcolor.value(valueType, QColor(131, 149, 167))); //#8395A7 - серый по умолчанию
    painter.setPen(Qt::NoPen);
    painter.drawRect(2, 2, 12, 12);
    return QIcon(pixmap);
}

void ModelMapping::removeItem(const QModelIndex &index)
{
    if (!index.isValid()) return;
    QStandardItem *item = this->itemFromIndex(index);
    if (!item) return;

    QStandardItem *parentItem = item->parent();
    if (parentItem) {
        parentItem->removeRow(item->row());
    } else {
        this->removeRow(item->row());
    }
    //при удалении item сразу обновляем хэш, чтобы он не содержал пустых ключей (указателей на ячейки)
    updateHash([this](bool success, uint size){
        Q_UNUSED(success);
        Q_UNUSED(size);
        emit modelCompleted();
    });
}

void ModelMapping::clear()
{
    /* при очистке модели очищаем хэш */
    QMutexLocker locker(&mutex);
    if (this->rowCount() > 0) this->removeRows(0, this->rowCount());
    hmodel.clear();
}

void ModelMapping::saveMapping()
{
    //при сохранении карты обновляем хэш доступа
    updateHash([this](bool success, uint size){
        Q_UNUSED(size);
        QList<QString> nodeIds;
        QList<QString> eventNodeIds;
        QList<QString> tags;
        if (hmodel.isEmpty()) return;
        for (auto it = hmodel.constBegin(); it != hmodel.constEnd(); ++it) {
            QString key = it.key(); //ключ
            QList<QStandardItem*> value = it.value(); //значение
            //1.формируем подписку opcua
            QStandardItem *item = value.first();
            if (item) {
                if (item->data(UserRole_Item).toInt() == VariantType) nodeIds.append(key);
                if (item->data(UserRole_Item).toInt() == EventType) eventNodeIds.append(key);
            }
            //2.формируем данные для отправки клиентам по отмеченному чекбоксу
            if (value.size() >= 2) {
                QStandardItem *itemTag = value.at(2);
                if (itemTag && itemTag->data(UserRole_Item).toInt() == VariantType && itemTag->checkState() == Qt::Checked) {
                    QString tag = itemTag->data(Qt::DisplayRole).toString();
                    //сохраняем только уникальные тэги
                    if (!tags.contains(tag)) tags.append(tag);
                }
            }
        }
        if (success) emit nodeIdsCompleted(nodeIds, eventNodeIds, tags);
    });
}

void ModelMapping::updateModel(const QVariantMap &map)
{
    QMutexLocker locker(&mutex);
    /* при удаленнии ячеек из модели сразу обновляем хэш, поэтому валидность
     * QStandardItem при обновлении через хэш не проверяем */
    if (hmodel.isEmpty()) return;

    /* hmodel.value(key) - возвращают копию, для изменения данных в хэше нужна ссылка, hmodel.find(key) - возвращает итератор it, it.value() ссылка на значение */
    if (auto it = hmodel.find(map.value("nodeId").toString()); it != hmodel.end() && it.value().size() >= 7) {
        /* map с обновленными данными для формирования протокола для записи в QTcpSocket
         * делаем через контейнер QVariantMap, а не получаем данные из модели, для
         * оптиммизации с точки зрения производительности (но можно и из модели брать данные) */
        QVariantMap dataChangeMap;

        //обновляем данные для тегов
        QStandardItem *itemVariant = it.value().at(4);
        if (itemVariant && map.value("variant").isValid()) {
            itemVariant->setData(map.value("variant"), Qt::DisplayRole);  //QStandartItem
            dataChangeMap.insert("variant", map.value("variant"));
        }
        QStandardItem *itemStatus = it.value().at(5);
        if (itemStatus && map.value("status").isValid()) {
            itemStatus->setData(map.value("status"), Qt::DisplayRole);   //QStandartItem
            dataChangeMap.insert("status", map.value("status"));
            if (map["status"].toString().contains("Good", Qt::CaseInsensitive)) { //если в строке статуса нет "Good" то красным
                itemStatus->setData(QBrush(QColor(139, 195, 74)), Qt::ForegroundRole);    //зеленый
            } else {
                itemStatus->setData(QBrush(QColor(255, 36, 0)), Qt::ForegroundRole);      //красный
            }
        }
        QStandardItem *itemTimestamp = it.value().at(6);
        if (itemTimestamp && map.value("sourceTimestamp").isValid()) {
            itemTimestamp->setData(map.value("sourceTimestamp").toDateTime().toString("dd.MM.yyyy HH:mm:ss.zzz"), Qt::DisplayRole); //QStandartItem
            dataChangeMap.insert("sourceTimestamp", map.value("sourceTimestamp"));
        }

        //обновляем данные для сообщений
        if (map.value("severity").isValid()) it.value().at(4)->setData(map.value("severity"), Qt::DisplayRole);        //QStandartItem
        if (map.value("activeState").isValid()) it.value().at(5)->setData(map.value("activeState"), Qt::DisplayRole);  //QStandartItem
        if (map.value("time").isValid()) it.value().at(6)->setData(map.value("time").toDateTime().toString("dd.MM.yyyy HH:mm:ss.zzz"), Qt::DisplayRole); //QStandartItem
        if (map.value("sourceName").isValid()) it.value().at(8)->setData(map.value("sourceName"), Qt::DisplayRole);    //QStandartItem

        /* эмитируем ОДИН сигнал для всех изменений в строке из хэша
         * используем для динамического формирования контейнера QVariantMap
         * формируем контейнер только с изменениями! и записываем в QTcpSocket */
        QStandardItem *itemTag = it.value().at(2); //TagApp
        if (itemTag && itemTag->data(UserRole_Item).toInt() == VariantType) {
            dataChangeMap.insert("tagapp", itemTag->data(Qt::DisplayRole));
            emit modelDataChanged(dataChangeMap); //формируем сигнал только при изменении VariantType (сообщения делаем только в модели сообщений)
        }
    }
}

void ModelMapping::updateHistoryCounter(const QString &tagapp, const quint16 &size)
{
    QMutexLocker locker(&mutex);
    if (hitemHistory.isEmpty()) return;
    if (auto it = hitemHistory.find(tagapp); it != hitemHistory.end()) {
        QStandardItem *item = it.value();
        if (item && item->data(UserRole_Item).toInt() == VariantType) item->setData(size, Qt::DisplayRole);
    }
}

void ModelMapping::clearSomeItems()
{
    /* после изменения модели или потери связи с сервером убираем статус сигнала у переменных, значение, время */
    if (hmodel.isEmpty()) return;
    for (auto it = hmodel.constBegin(); it != hmodel.constEnd(); ++it) {
        QString key = it.key(); //ключ
        Q_UNUSED(key);
        QList<QStandardItem*> items = it.value(); //значение
        if (items.size() >= 7) {
            if (items.at(4) != nullptr) items.at(4)->setData("", Qt::DisplayRole); //variant
            if (items.at(5)) items.at(5)->setData("", Qt::DisplayRole); //status
            if (items.at(6)) items.at(6)->setData("", Qt::DisplayRole); //sourceTimestamp
        }
    }
}
