#include "uiopcua.h"
#include "ui_uiopcua.h"

UiOPCUA::UiOPCUA(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::UiOPCUA)
    , model(nullptr)
    , modelTree(nullptr)
{
    ui->setupUi(this);
    this->setWindowTitle("OPCUA Connect");
    this->setFixedSize(this->width(), this->height());
    setupUI();
    setupConnections();
}

UiOPCUA::~UiOPCUA()
{
    delete ui;
}

void UiOPCUA::setupUI()
{
    ui->pb_close->setIcon(QIcon(":img/icon/mainmenu/close.svg"));

    ui->le_serverURL->setText(QString("opc.tcp://127.0.0.1:53530"));
    ui->label_info->setStyleSheet("color: #ffa700;");

    ui->label_logo->setPixmap(QPixmap(":/img/open62541.png").scaled(130, 130, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    ui->pb_connect->setDisabled(true);

    ui->le_pass->setEchoMode(QLineEdit::Password);
    ui->rb_connect->setAttribute(Qt::WA_TransparentForMouseEvents, true);     //отключаем событие мыши
    ui->rb_connect->setAutoExclusive(false);
    ui->rb_connect->setStyleSheet(applib->qss.rb_whiteGreen);

    //Создаем группу кнопок
    QButtonGroup *buttonGroup = new QButtonGroup(this);
    buttonGroup->setObjectName("authButtonGroup");   // <-- задаём уникальное имя
    buttonGroup->addButton(ui->rb_anonymous, 1);
    buttonGroup->addButton(ui->rb_username, 2);
    connect(buttonGroup, &QButtonGroup::buttonClicked, this, [this, buttonGroup] (QAbstractButton *button) {
        switch(buttonGroup->id(button)) {
        case 1:
            ui->le_username->setEnabled(false);
            ui->le_pass->setEnabled(false);
            break;
        case 2:
            ui->le_username->setEnabled(true);
            ui->le_pass->setEnabled(true);
            break;
        }
        ui->pb_connect->setEnabled((ui->rb_anonymous->isChecked() || ui->rb_username->isChecked()) && ui->cb_endpoint->currentIndex() != -1);
    });

    //treeView
    ui->treeView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->treeView->header()->setDefaultAlignment(Qt::AlignCenter);
    ui->treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->treeView->setFocusPolicy(Qt::StrongFocus);
    ui->treeView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->treeView->setEditTriggers(QAbstractItemView::DoubleClicked);
    ui->treeView->setModel(modelNodes);

    //Включаем чекбоксы
    ui->chBox_subscribeToData->setChecked(true);
    ui->chBox_subscribeToEvents->setChecked(true);
}

void UiOPCUA::setupConnections()
{
    connect(opcClient, &OPCUAClient::status, this, [this] (const QString &status) { modelSysLog->append("OPCUA", status); });

    connect(ui->pb_getEndpoints, &QPushButton::clicked, this, &UiOPCUA::getEndpoint);

    connect(ui->cb_endpoint, &QComboBox::activated, this, [this] () {
        ui->pb_connect->setEnabled((ui->rb_anonymous->isChecked() || ui->rb_username->isChecked()) && ui->cb_endpoint->currentIndex() != -1);
    });

    connect(ui->pb_connect, &QPushButton::clicked, this, [this] () {
        QModelIndex index = ui->cb_endpoint->model()->index(ui->cb_endpoint->currentIndex(), 0);
        Endpoint endpoint;
        endpoint.securityPolicy = ui->cb_endpoint->model()->data(index, SecurityRoles::securityPolicy).toString();
        endpoint.securityMode = static_cast<UA_MessageSecurityMode>(ui->cb_endpoint->model()->data(index, SecurityRoles::securityMode).toUInt());
        // qDebug() << "Endpoint" << index << endpoint.securityPolicy << endpoint.securityMode;
        /* подключаемся и создаем запросы в одном лямбда функии друг за другом, т.к. при подключении используем синхронные запросы */
        if (ui->rb_anonymous->isChecked()) opcClient->connectToServerAnonymous(ui->le_serverURL->text()); //прописываем IP:port, если endpointURL иногда долго думает
        if (ui->rb_username->isChecked()) opcClient->connectToServerUsername(ui->le_serverURL->text(), endpoint, ui->le_username->text(), ui->le_pass->text());

        /* флаг создания запроса на подписку (обрабатываем его в Engine), разделили, т.к. отдельно обрабатываем флаг,
         * отдельно обрабатываем сам процесс создания запроса на подписку, лист nodeIds может быть прочитан заранее
         * и может быть откорректирован на лету! */
        engine->setSubscribeEnable(ui->chBox_subscribeToData->isChecked(), ui->chBox_subscribeToEvents->isChecked()); //флаги подписок
        engine->createSubscribeOPC();
        //сохраняем url, login, pass
        FileIO::saveConfigToJson(QVariantMap{{"opcua", QVariantMap{
                                 {"url", ui->le_serverURL->text()},
                                 {"login", ui->le_username->text()},
                                 {"pass", ui->le_pass->text().toUtf8().toHex()}} //из Qstring в 16сс
        }});
    });

    connect(ui->pb_disconnect, &QPushButton::clicked, this, [this] () { opcClient->disconnectFromServer(); });

    connect(ui->pb_close, &QPushButton::clicked, this, [this] () { this->close(); });

    QButtonGroup *buttonGroup = this->findChild<QButtonGroup*>("authButtonGroup"); //поиск по имени т.к. buttonGroup - не член класса
    connect(opcClient, &OPCUAClient::connected, this, [this, buttonGroup] (const bool &b) {
        if (b) {
            ui->rb_connect->setIcon(QIcon(":img/icon/plugconnected.svg"));
        } else {
            ui->rb_connect->setIcon(QIcon(":img/icon/plugdisconnected.svg"));
            ui->le_endpointURL->clear();
            ui->cb_endpoint->clear();

            //сбрасываем состояние радиокнопок
            if (buttonGroup && buttonGroup->checkedButton()) {
                buttonGroup->setExclusive(false);           // Временно отключаем эксклюзивность
                buttonGroup->checkedButton()->setChecked(false); // Снимаем выделение с текущей выбранной кнопки
                buttonGroup->setExclusive(true);            // Включаем эксклюзивность обратно
            }
        }
        ui->pb_connect->setDisabled(true); //включаем кнопку только при переключении QRadioButton в QButtonGroup
        ui->rb_connect->setChecked(b);
        ui->le_serverURL->setDisabled(b);
        ui->le_endpointURL->setDisabled(b);
        ui->le_username->setDisabled(b);
        ui->le_pass->setDisabled(b);
        ui->cb_endpoint->setDisabled(b);
        ui->pb_getEndpoints->setDisabled(b);
        ui->chBox_subscribeToData->setDisabled(b);
        ui->chBox_subscribeToEvents->setDisabled(b);
        QTimer::singleShot(500, this, [this]() { opcClient->browseNode(nullptr); }); //вызываем с задержкой иначе получается рекурсивный коннект хз
    });

    //treeView (запрашиваем новые данные BrowseNode, как они готовы по сигналу OPCUAClient::browseCompleted добавляем на QTreeView в классе ItemModel
    connect(ui->treeView, &QTreeView::clicked, this, [this] (const QModelIndex &index) {
        if (modelNodes->checkChildren(index)) return; //Если есть дочерний элемент исключаем повторный запрос
        if (ui->treeView->model()->data(index, ModelNodes::NodeRoles::nodeId).isValid()) {
            opcClient->browseNode(ui->treeView->model()->data(index, ModelNodes::NodeRoles::nodeId).toString());
        }
    });
}

void UiOPCUA::getEndpoint()
{
    ui->le_endpointURL->clear();
    ui->cb_endpoint->clear();

    QList<Endpoint> storeEndpoint;
    if (!opcClient->getEndpoints(ui->le_serverURL->text(), storeEndpoint) || storeEndpoint.isEmpty()) return;
    ui->le_endpointURL->setText(storeEndpoint.first().endpointUrl);

    if (!model) {
        model = new QStandardItemModel(this);
        ui->cb_endpoint->setModel(model);
    }
    model->clear();

    for (auto i = 0; i < storeEndpoint.size(); i++) {
        QString securityModeStr;
        switch(storeEndpoint[i].securityMode) {
        case UA_MESSAGESECURITYMODE_INVALID: securityModeStr = "Invalid"; break;                   //0
        case UA_MESSAGESECURITYMODE_NONE: securityModeStr = "None"; break;                         //1
        case UA_MESSAGESECURITYMODE_SIGN: securityModeStr = "Sign"; break;                         //2
        case UA_MESSAGESECURITYMODE_SIGNANDENCRYPT: securityModeStr = "SignAndEncrypt"; break;     //3
        default: securityModeStr = "Unknown"; break;
        }

        QStandardItem *item = new QStandardItem;
        item->setText(QString("%1 & %2").arg(securityModeStr, storeEndpoint[i].securityPolicy));
        item->setData(storeEndpoint[i].securityPolicy, SecurityRoles::securityPolicy); //Qt::UserRole - используется для хранения пользовательских данных
        item->setData(QVariant::fromValue(static_cast<uint>(storeEndpoint[i].securityMode)), SecurityRoles::securityMode);
        model->appendRow(item);
    }
    ui->pb_connect->setEnabled((ui->rb_anonymous->isChecked() || ui->rb_username->isChecked()) && ui->cb_endpoint->currentIndex() != -1);
}

void UiOPCUA::restore(const QString &url, const QString &login, const QString &pass)
{
    ui->le_serverURL->setText(url);
    ui->le_username->setText(login);
    ui->le_pass->setText(QString::fromUtf8(QByteArray::fromHex(pass.toLatin1()))); //из 16сс в Qstring
}
