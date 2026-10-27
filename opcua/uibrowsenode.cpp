#include "uibrowsenode.h"
#include "./ui_uibrowsenode.h"

UiBrowseNode::UiBrowseNode(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::UiBrowseNode)
{
    ui->setupUi(this);
    this->setWindowTitle("OPCUA Browse Node");
    setupUI();
    setupConnections();
}

UiBrowseNode::~UiBrowseNode()
{
    delete ui;
}

void UiBrowseNode::setupUI()
{
    //treeView_browseNode
    ui->treeView_browseNode->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->treeView_browseNode->header()->setDefaultAlignment(Qt::AlignCenter);
    ui->treeView_browseNode->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->treeView_browseNode->setFocusPolicy(Qt::StrongFocus);
    ui->treeView_browseNode->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->treeView_browseNode->setEditTriggers(QAbstractItemView::DoubleClicked);
    ui->treeView_browseNode->setModel(modelNodes);
    ui->treeView_browseNode->setColumnWidth(ui->treeView_browseNode->model()->columnCount() - 1, 0); //размер последней колонки
    ui->treeView_browseNode->setMinimumWidth(40);

    //treeView_attributes
    ui->treeView_attributes->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->treeView_attributes->header()->setDefaultAlignment(Qt::AlignCenter);
    ui->treeView_attributes->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->treeView_attributes->setFocusPolicy(Qt::StrongFocus);
    ui->treeView_attributes->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->treeView_attributes->setEditTriggers(QAbstractItemView::DoubleClicked);
    ui->treeView_attributes->setModel(modelAttributes);
    ui->treeView_attributes->setColumnWidth(ui->treeView_attributes->model()->columnCount() - 1, 0); //размер последней колонки
    ui->treeView_attributes->setAlternatingRowColors(true);
    ui->treeView_attributes->setStyleSheet(appLib->qss.treeView);
    ui->treeView_attributes->setMinimumWidth(40);

    //splitter
    ui->splitter->setStretchFactor(0, 7);   //соотношение 7:3
    ui->splitter->setStretchFactor(1, 3);

    ui->pb_update->setIcon(QIcon(":/img/icon/update.svg"));
    ui->pb_update->setToolTip("Обновить");

    ui->pb_drag->setIcon(QIcon(":/img/icon/dragclick.svg"));
    ui->pb_drag->setToolTip("Перетащите для копирования данных узла");
    ui->pb_drag->installEventFilter(this); //включаем фильтр событий
    QVariantMap metaData; //метаданные (можно использовать для валидации источника! передавать сложные Qt типы!)
    metaData["source"] = "UiBrowseNode";
    metaData["widget_type"] = "drag_button";
    ui->pb_drag->setProperty("metaData", metaData);
}

void UiBrowseNode::setupConnections()
{
    //treeView (запрашиваем новые данные BrowseNode, как они готовы по сигналу OPCUAClient::browseCompleted добавляем на QTreeView в классе ItemModel
    connect(ui->treeView_browseNode, &QTreeView::clicked, this, [this] (const QModelIndex &index) {
        if (modelNodes->checkChildren(index)) return; //Если есть дочерний элемент исключаем повторный запрос
        if (ui->treeView_browseNode->model()->data(index, ModelNodes::NodeRoles::nodeId).isValid()) {
            opcClient->browseNode(ui->treeView_browseNode->model()->data(index, ModelNodes::NodeRoles::nodeId).toString());
        }
    });
    connect(ui->treeView_browseNode, &QTreeView::clicked, this, [this] (const QModelIndex &index) {
        if (ui->treeView_browseNode->model()->data(index, ModelNodes::NodeRoles::nodeId).isValid()) {
            nodeId_current = ui->treeView_browseNode->model()->data(index, ModelNodes::NodeRoles::nodeId).toString();
            opcClient->readNodeAttributes(nodeId_current);
        }
    });
    connect(ui->pb_update, &QPushButton::clicked, this, [this] () { opcClient->readNodeAttributes(nodeId_current); });

    //по готовности модели открываем все элементы
    connect(modelAttributes, &ModelAttributes::modelCompleted, this, [this] () { ui->treeView_attributes->expandAll(); });
}

bool UiBrowseNode::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == ui->pb_drag && event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent && mouseEvent->button() == Qt::LeftButton) {

            //Готовим данные для передачи
            QVariantMap attributes = modelAttributes->getAttribbutes();
            QVariantMap dragData;
            dragData["metaData"] = ui->pb_drag->property("metaData").toMap();;  //метаданные кнопки (полученные из свойств)
            dragData["nodeId"] = attributes.value("nodeId").toString(); //value - безопасный доступ (не создает пустого значения)
            dragData["displayName"] = attributes.value("displayName").toString();
            dragData["valueType"] = attributes.value("valueType").toString();
            dragData["dataTypeId"] = attributes.value("dataTypeId").toInt(); //int
            dragData["description"] = attributes.value("description").toString();
            dragData["tagList"] = QStringList{"sensor", "analog", "temperature"};

            QByteArray baData;
            QDataStream stream(&baData, QIODevice::WriteOnly);
            stream << dragData;

            //MIME данные для Drag&Drop
            QDrag *drag = new QDrag(this);
            QMimeData *mimeData = new QMimeData; //после удаления drag удалится mimeData
            mimeData->setData("application/x-browsenode", baData); //используем ТОТ ЖЕ формат nimeType, что ожидает модель!

            //Дополнительные форматы для совместимости
            mimeData->setText(dragData["displayName"].toString());
            drag->setMimeData(mimeData);

            //Настраиваем визуал
            drag->setPixmap(createDragPixmap(dragData["displayName"].toString()));
            drag->setHotSpot(QPoint(10, 10));

            //Запускаем drag асинхронно
            Qt::DropAction result = drag->exec(Qt::MoveAction); //после выполнения drag удалится автоматически
            qDebug() << Q_FUNC_INFO << result;
            return true; //Обработали событие
        }
    }
    return QObject::eventFilter(obj, event);
}

QPixmap UiBrowseNode::createDragPixmap(const QString &name)
{
    QPixmap pixmap(80, 25);
    pixmap.fill(Qt::lightGray);

    QPainter painter(&pixmap);
    painter.setPen(Qt::black);
    painter.drawText(pixmap.rect(), Qt::AlignCenter, name);
    return pixmap;
}
