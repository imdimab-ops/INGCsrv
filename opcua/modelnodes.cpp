#include "modelnodes.h"

ModelNodes::ModelNodes(QObject *parent)
    : QStandardItemModel{parent}
{
    this->setHorizontalHeaderLabels({"BrowseName", "NodeClass", "NodeId", "ReferenceTypeId", "IdentifierType", "DisplayName", ""});

    /* Добавляем QList<NodeOPCUA> &childNodes в модель, + через роли добавляем в модель browseName, nodeIdStr, nodeId, referenceTypeId.
     * Для создания нового запроса browseNode() и readNodeAttributes() из модели берем nodeIdStr и nodeId (тип данных UA_NodeId !!!) */
    connect(opcClient, &OPCUAClient::browseCompleted, this, [this] (const QString &nodeId, const QList<NodeOPC> &childNodes) {
        addItems(nodeId, childNodes);
    });
    connect(opcClient, &OPCUAClient::connected, this, [this] (const bool &b) { if (!b) clear(); });
}

ModelNodes::~ModelNodes()
{

}

ModelNodes *ModelNodes::getInstance()
{
    static ModelNodes instance;
    return &instance;
}

QIcon ModelNodes::createColorIcon(const QColor &color) const
{
    QPixmap pixmap(16, 16);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(color);
    painter.setPen(Qt::NoPen);
    painter.drawRect(2, 2, 12, 12);

    return QIcon(pixmap);
}

const ModelNodes::NodeClassStyle *ModelNodes::getNodeClassStyle(const UA_NodeClass &nodeClass)
{
    /* используем указатель, на тот случай если в массиве элемент не найден (хотя это не наш случай)
     * при использовании в коде необходимо проверять на существование элемента */
    auto it = std::find_if(storeNodeClassStyle.begin(), storeNodeClassStyle.end(),
                           [nodeClass](const NodeClassStyle& iter) { return iter.nodeClass == nodeClass; });
    return it != storeNodeClassStyle.end() ? &(*it) : nullptr;
}

const ModelNodes::IdentifierTypeStyle *ModelNodes::getIdentifierTypeStyle(const UA_NodeIdType &identifierType)
{
    /* используем указатель, на тот случай если в массиве элемент не найден (хотя это не наш случай)
     * при использовании в коде необходимо проверять на существование элемента */
    auto it = std::find_if(storeIdentifierType.begin(), storeIdentifierType.end(),
                           [identifierType](const IdentifierTypeStyle& iter) { return iter.identifierType == identifierType; });
    return it != storeIdentifierType.end() ? &(*it) : nullptr;
}

bool ModelNodes::checkRepeatData(const QList<NodeOPC> &data)
{
    for (auto i = 0; i < rowCount(); ++i) {
        QModelIndex index = this->index(i, 0);
        for (auto j = 0; j < data.size(); ++j) {
            if (this->data(index, NodeRoles::nodeId).toString() == data[j].nodeIdStr &&
                this->data(index, NodeRoles::browseName).toString() == data[j].browseName) {
                return true;
            }
        }
    }
    return false;
}

QModelIndex ModelNodes::seachQModelIndex(const QString &nodeId)
{
    QModelIndexList matches = this->match(
        this->index(0, 0),
        NodeRoles::nodeId,
        nodeId,
        100, // -1 найти все совпадения
        Qt::MatchExactly | Qt::MatchRecursive
        );
    if (matches.isEmpty()) return QModelIndex();
    if (currentIndex.isValid() && matches.contains(currentIndex)) return currentIndex;
    return QModelIndex();
}

void ModelNodes::addItems(const QString &nodeId, const QList<NodeOPC> &data)
{
    QStandardItem *parentItem = this->itemFromIndex(seachQModelIndex(nodeId));
    for (auto i = 0; i < data.size(); ++i) {
        QList<QStandardItem*> itemRow;

        itemRow.append(new QStandardItem());
        const NodeClassStyle *nodeClass = getNodeClassStyle(data[i].nodeClass);
        if (nodeClass != nullptr) { //проверяем не пустой ли элемент
            itemRow.last()->setData(QString("%1 %2").arg(nodeClass->icon, data[i].browseName), Qt::DisplayRole);

            //Используется для хранения пользовательских данных
            itemRow.last()->setData(data[i].browseName, NodeRoles::browseName);
            itemRow.last()->setData(data[i].nodeIdStr, NodeRoles::nodeId);
            itemRow.last()->setData(data[i].referenceTypeId, NodeRoles::referenceTypeId);

            itemRow.append(new QStandardItem());
            itemRow.last()->setData(nodeClass->label, Qt::DisplayRole); //используем для иконок отображения
        }

        itemRow.append(new QStandardItem());
        itemRow.last()->setData(data[i].nodeIdStr, Qt::DisplayRole);
        itemRow.last()->setData(QBrush(QColor(0, 113, 189)), Qt::ForegroundRole);   //цвет текста

        itemRow.append(new QStandardItem());
        itemRow.last()->setData(data[i].referenceTypeId, Qt::DisplayRole);

        itemRow.append(new QStandardItem());
        const IdentifierTypeStyle *identifierType = getIdentifierTypeStyle(data[i].extendedNodeId.nodeId.identifierType);
        if (identifierType != nullptr) { //проверяем не пустой ли элемент
            itemRow.last()->setData(identifierType->label, Qt::DisplayRole);
            itemRow.last()->setIcon(createColorIcon(QColor(identifierType->color)));
        }

        itemRow.append(new QStandardItem());
        itemRow.last()->setData(data[i].displayName, Qt::DisplayRole);

        if (parentItem) {
            parentItem->appendRow(itemRow);
        } else {
            this->appendRow(itemRow);
        }
    }
}

void ModelNodes::removeItem(const QModelIndex &index)
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

void ModelNodes::clear()
{
    QMutexLocker locker(&mutex); //блокируем на выполнение данной операции
    /* очищаем так, если через QStandardItemModel::clear(); очистит все, в том числе и заголовок */
    if (this->rowCount() > 0) this->removeRows(0, this->rowCount());
}

bool ModelNodes::checkChildren(const QModelIndex &index)
{
    if (!index.isValid()) return false;
    currentIndex = index; //запоминаем текущий индекс для поиска в seachQModelIndex();
    /* Проверка на дочерние элементы для исключения повторных запросов */
    if (index.isValid() && this->hasChildren(index)) return true;
    return false;
}
