#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    this->setWindowTitle("INGC App Server");
    setupUI();
    setupConnections();
    initTray();
    modelSysLog->append(Q_FUNC_INFO, "[Info] INGC App start");
    QTimer::singleShot(50, this, [this]() {
        // this->hide(); //при загрузке сразу сворачиваем в трей
        restoreConfig();
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupUI()
{
    ui->actionQuit->setIcon(QIcon(":img/icon/mainmenu/close.svg"));
    ui->actionSystemLog->setIcon(QIcon(":img/icon/mainmenu/systemlog.svg"));
    ui->actionOPCUAGenerateCertificate->setIcon(QIcon(":img/icon/mainmenu/generate.svg"));
    ui->actionOPCUABrowseNode->setIcon(QIcon(":img/icon/mainmenu/browsenodes.svg"));
    ui->actionOPCUAConnect->setIcon(QIcon(":img/icon/mainmenu/connection.svg"));
    ui->actionTcpServer->setIcon(QIcon(":img/icon/mainmenu/server.svg"));
    ui->actionViewMapping->setIcon(QIcon(":img/icon/mainmenu/map.svg"));
    ui->actionAbout->setIcon(QIcon(":img/icon/mainmenu/about.svg"));
    ui->actionAboutQt->setIcon(QIcon(":img/icon/mainmenu/aboutqt.svg"));

    ui->rb_opc->setIcon(QIcon(":img/icon/plugdisconnected.svg"));
    ui->rb_opc->setAttribute(Qt::WA_TransparentForMouseEvents, true);     //отключаем событие мыши
    ui->rb_opc->setAutoExclusive(false);
    ui->rb_opc->setStyleSheet(applib->qss.rb_whiteGreen);

    ui->rb_srv->setIcon(QIcon(":img/icon/stop.svg"));
    ui->rb_srv->setAttribute(Qt::WA_TransparentForMouseEvents, true);     //отключаем событие мыши
    ui->rb_srv->setAutoExclusive(false);
    ui->rb_srv->setStyleSheet(applib->qss.rb_whiteGreen);

    ModelClients *modelClients = new ModelClients(this);
    ui->tableView->setModel(modelClients);
    ui->tableView->setSortingEnabled(true);
    ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->tableView->horizontalHeader()->setMinimumSectionSize(40); //минимальная ширина
    ui->tableView->setStyleSheet("QTableView::item {"
                                 "padding-left: 5px;"
                                 "padding-right: 5px;"
                                 "}");
    ui->tableView->horizontalHeader()->setSectionResizeMode(modelClients->columnCount() - 1, QHeaderView::Fixed);
    ui->tableView->setColumnWidth(modelClients->columnCount() - 1, 75); //фиксированный размер последней колонки

    ButtonDelegate *buttonDelegate = new ButtonDelegate(this);
    ui->tableView->setItemDelegateForColumn(modelClients->columnCount() - 1, buttonDelegate); //для столбца "Disconn"
    connect(buttonDelegate, &ButtonDelegate::buttonClicked, this, [this, modelClients] (const QModelIndex &index) {
        dialog->setCaller(this); //отображаем виджет в материнском окне (диалог вызываем в методе closeClient(index)
        modelClients->closeClient(index);
    });

    ui->splitter->setCollapsible(0, false); //первый виджет нельзя схлопнуть
    ui->splitter->setCollapsible(1, false); //второй виджет нельзя схлопнуть
    ui->splitter->setStretchFactor(0, 3);   //соотношение 3:1
    ui->splitter->setStretchFactor(1, 1);

    //настраиваем лог событый на главном окне
    ui->listView->setModel(modelSysLog);
    ui->listView->setMinimumWidth(20);
    connect(modelSysLog, &ModelSystemLog::rowsInserted, this, [this]() { ui->listView->scrollToBottom(); });
}

void MainWindow::setupConnections()
{
    connect(ui->actionQuit, &QAction::triggered, this, &MainWindow::close);
    connect(ui->actionSystemLog, &QAction::triggered, this, [this] () {
        //отображаем слева или справа главного окна
        applib->moveWindowRelative(this, &uiSystemLog);
    });
    connect(ui->actionLogDir, &QAction::triggered, this, [this]() {});
    connect(ui->actionViewMapping, &QAction::triggered, this, [this]() { showCenter(uiViewMapping); });
    connect(ui->actionOPCUAGenerateCertificate, &QAction::triggered, this, [this] () { showCenter(uiGenCert); });
    connect(ui->actionOPCUABrowseNode, &QAction::triggered, this, [this] () { showCenter(uiBrowseNode); });
    connect(ui->actionOPCUAConnect, &QAction::triggered, this, [this] () { showCenter(uiOPCUA); });

    connect(ui->actionTcpServer, &QAction::triggered, this, [this] () { showCenter(uiTcp); });

    connect(ui->actionAboutQt, &QAction::triggered, this, [this] () {
        QMessageBox::aboutQt(this, "Application");
    });
    connect(ui->actionAbout, &QAction::triggered, this, [this] () {
        QString message = QStringLiteral("ООО \"ИНГК-Промтех\" \n"
                                         "г.Москва, Научный проезд 14А, строение 3, тел.+7(495)231-46-61 \n"
                                         "E-mail: info@ingc.ru \n"
                                         "INGCsrv сервер для обновления данных удаленных клиентов"
                                         "\n \n"
                                         "%1\n"
                                         "%2\n"
                                         "%3\n"
                                         "%4\n"
                                         "Uptime: %5")
                              .arg(applib->getAppVersion(),
                                   opcClient->getVersion(),
                                   applib->getOpenSSLVersionString(),
                                   applib->getOSVersionString(),
                                   applib->getUptime().toString("dd.MM.yyyy hh:mm:ss"));
        QMessageBox::about(this, "Application", message);
    });

    connect(opcClient, &OPCUAClient::connected, this, [this] (const bool &b) {
        if (b) {
            ui->rb_opc->setIcon(QIcon(":img/icon/plugconnected.svg"));
        } else {
            ui->rb_opc->setIcon(QIcon(":img/icon/plugdisconnected.svg"));
        }
        ui->rb_opc->setChecked(b);
    });
    connect(tcpsrv, &TcpThread::serverUpdated, this, [this] (const STWsrv &stw) {
        ui->rb_srv->setChecked(stw.start);
        if (stw.start) {
            ui->rb_srv->setIcon(QIcon(":img/icon/play.svg"));
        } else {
            ui->rb_srv->setIcon(QIcon(":img/icon/stop.svg"));
        }
        ui->label_IP->setText("IP: " + stw.host + ":" + stw.port);
        ui->label_clients->setText("Clients: " + QString::number(stw.client_size));
    });
}

void MainWindow::restoreConfig()
{
    QVariantMap map = FileIO::loadConfigFromJson();
    if (map.value("opcua").isValid()) {
        uiOPCUA.restore(map.value("opcua").toMap().value("url").toString(),
                        map.value("opcua").toMap().value("login").toString(),
                        map.value("opcua").toMap().value("pass").toString());
    }
    if (map.value("tcp").isValid()) {
        uiTcp.restore(map.value("tcp").toMap().value("ip").toString(),
                      map.value("tcp").toMap().value("port").toString());
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    dialog->setCaller(this);
    dialog->setWindowTitle("Close");
    dialog->setText("Остановить и закрыть сервер?");
    if(dialog->exec() == QDialog::Rejected) {   //вызываем диалог для подтверждения
        event->ignore();
        this->showNormal();
        qDebug() << Q_FUNC_INFO << 1;
        return;
    }
    qDebug() << Q_FUNC_INFO << 2;

    emit tcpsrv->stopServer(); //для закрытия всех подключений ранее удаления системы событий Qt
    //ждем ровно один цикл обработки событий
    QEventLoop loop;
    QMetaObject::invokeMethod(&loop, "quit", Qt::QueuedConnection);
    loop.exec();
    //закрываем все дальше
    uiOPCUA.close();
    uiGenCert.close();
    uiBrowseNode.close();
    uiTcp.close();
    uiSystemLog.close();
    uiViewMapping.close();
    /* в main.cpp прописываем флаг QApplication::setQuitOnLastWindowClosed(false)-отключаем автоматический выход при закрытии окон
     * иначе при закрытии открытых окнах и свернутом в трей приложении приложение закрывалось, теперь его закрывам ручками QApplication::quit()! */
    QApplication::quit();
}

void MainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange) {
        QWindowStateChangeEvent *stateEvent = static_cast<QWindowStateChangeEvent*>(event);
        /* при сворачивании в панель задач - убираем в трей */
        if (this->windowState() == Qt::WindowMinimized) {
            this->hide();
        }
    }
}

void MainWindow::showCenter(QWidget &child)
{
    if (child.isMinimized()) {
        child.setWindowState(child.windowState() & ~Qt::WindowMinimized);
    }
    int x = this->x() + this->width()  / 2 - child.width()  / 2;
    int y = this->y() + this->height() / 2 - child.height() / 2;

    child.move(x, y);
    child.show();
    child.raise();
    child.activateWindow();
}

void MainWindow::initTray()
{
    QMenu *trayIconMenu = new QMenu(this);

    //Заголовок меню
    QWidget *headerWidget = new QWidget();
    QHBoxLayout *headerLayout = new QHBoxLayout(headerWidget);
    QLabel *titleLabel = new QLabel(this->windowTitle());
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-weight: bold; padding: 5px; color: #2c3e50;");
    headerLayout->addWidget(titleLabel);
    headerLayout->setContentsMargins(4, 4, 4, 4);

    QWidgetAction *headerAction = new QWidgetAction(trayIconMenu);
    headerAction->setDefaultWidget(headerWidget);
    trayIconMenu->addAction(headerAction);


    //Раздел статуса
    QMenu *statusMenu = trayIconMenu->addMenu("Статус");
    // statusMenu->setIcon(QIcon(":img/icon/tray/status.svg"));
    statusMenu->setIcon(QIcon(":img/icon/tray/ststop.svg"));

    //Статус OPC UA
    QAction *opcStatus = new QAction("OPC UA: ✗ Отключен", this);
    opcStatus->setIcon(QIcon(":img/icon/plugdisconnected.svg"));
    opcStatus->setEnabled(false);
    connect(opcClient, &OPCUAClient::connected, this, [this, opcStatus, statusMenu](bool connected) {
        opcStatus->setText(connected ? "OPC UA: ✓ Подключен" : "OPC UA: ✗ Отключен");
        opcStatus->setIcon(QIcon(connected ? ":img/icon/plugconnected.svg" : ":img/icon/plugdisconnected.svg"));
        m_opcrun = connected;
        statusMenu->setIcon(QIcon(m_tcprun && m_opcrun ? ":img/icon/tray/stplay.svg" : ":img/icon/tray/ststop.svg"));
    });
    statusMenu->addAction(opcStatus);

    //Статус TCP сервера
    QAction *tcpStatus = new QAction("TCP сервер: ✗ Остановлен", this);
    tcpStatus->setIcon(QIcon(":img/icon/stop.svg"));
    tcpStatus->setEnabled(false);
    connect(tcpsrv, &TcpThread::serverUpdated, this, [this, tcpStatus, statusMenu](const STWsrv &stw) {
        tcpStatus->setText(stw.start ? QString("TCP сервер: ✓ Запущен (Клиентов: %1)").arg(stw.client_size) : "TCP сервер: ✗ Остановлен");
        tcpStatus->setIcon(QIcon(stw.start ? ":img/icon/play.svg" : ":img/icon/stop.svg"));
        m_tcprun = stw.start;
        statusMenu->setIcon(QIcon(m_tcprun && m_opcrun ? ":img/icon/tray/stplay.svg" : ":img/icon/tray/ststop.svg"));
    });
    statusMenu->addAction(tcpStatus);

    //IP адрес сервера
    QAction *ipAction = new QAction("IP: Не настроен", this);
    ipAction->setEnabled(false);
    connect(tcpsrv, &TcpThread::serverUpdated, this, [this, ipAction](const STWsrv &stw) {
        ipAction->setText(stw.host.isEmpty() ? "IP: Не настроен" : QString("IP: %1:%2").arg(stw.host, stw.port));
    });
    statusMenu->addAction(ipAction);
    statusMenu->addSeparator();

    //Раздел управления сервером
    QMenu *serverMenu = trayIconMenu->addMenu("Управление сервером");
    serverMenu->setIcon(QIcon(":img/icon/mainmenu/server.svg"));
    serverMenu->addSeparator();
    serverMenu->addAction(ui->actionTcpServer);
    serverMenu->addAction(ui->actionOPCUAConnect);
    serverMenu->addAction(ui->actionViewMapping);
    serverMenu->addAction(ui->actionSystemLog);

    //Основные действия окна
    QAction *minimaze = new QAction("Свернуть все", this);
    minimaze->setIcon(QIcon(":img/icon/tray/hide.svg"));
    connect(minimaze, &QAction::triggered, this, [this]() {
        this->hide();       //сворачиваем в трей
        uiOPCUA.close();    //остальное закрываем тк нужные данные лежат в моделях
        uiGenCert.close();
        uiBrowseNode.close();
        uiTcp.close();
        uiSystemLog.close();
        uiViewMapping.close(); //закрываем, тк надо сохранять карту привязки тегов
    });

    QAction *restore = new QAction("Восстановить", this);
    restore->setIcon(QIcon(":img/icon/tray/restore.svg"));
    connect(restore, &QAction::triggered, this, [this]() {
        this->showNormal();
        this->activateWindow();
        this->raise();
    });

    QAction *quit = new QAction("Выход", this);
    quit->setIcon(QIcon(":img/icon/tray/close.svg"));
    connect(quit, &QAction::triggered, this, [this]() {
        if (this->isHidden() || this->windowState() == Qt::WindowMinimized) {
            //если окно скрыто или свернуто, показываем его нормально
            this->showNormal();
            this->activateWindow();
            this->raise();
        }
        this->close(); //вызываем закрытие - диалог появится поверх окна
    });

    trayIconMenu->addSeparator();
    trayIconMenu->addAction(minimaze);
    trayIconMenu->addAction(restore);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quit);

    //создаем иконку в трее
    QSystemTrayIcon *trayIcon = new QSystemTrayIcon(this);
    trayIcon->setContextMenu(trayIconMenu);
    connect(trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::activatedTray);
    trayIcon->setIcon(QIcon(":/img/logo.png"));
    trayIcon->show();
}

void MainWindow::activatedTray(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick) {
        if (this->isHidden() || this->windowState() == Qt::WindowMinimized) {
            this->showNormal();
            this->activateWindow();
            this->raise();
        } else {
            this->hide();
        }
    }
}
