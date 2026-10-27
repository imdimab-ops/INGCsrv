#include "uitcp.h"
#include "./ui_uitcp.h"

UiTcp::UiTcp(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::UiTcp)
{
    ui->setupUi(this);
    this->setWindowTitle("TCP Server");
    this->setFixedSize(this->width(), this->height());
    setupUI();
    setupConnections();
}

UiTcp::~UiTcp()
{
    delete ui;
}

void UiTcp::setupUI()
{
    ui->le_Port->setText("5577");
    ui->pb_start->setIcon(QIcon(":img/icon/play.svg"));
    ui->pb_stop->setIcon(QIcon(":img/icon/stop.svg"));
    ui->pb_close->setIcon(QIcon(":img/icon/mainmenu/close.svg"));

    ui->rb_Run->setAttribute(Qt::WA_TransparentForMouseEvents, true);     //отключаем событие мыши
    ui->rb_Run->setAutoExclusive(false);
    ui->rb_Run->setStyleSheet(applib->qss.rb_whiteGreen);



    //Тут подгружаем настройки из файла настроек и прописываем
}

void UiTcp::setupConnections()
{
    connect(tcpsrv, &TcpThread::status, this, [this] (const QString &status) { modelSysLog->append("TCP", status); });
    connect(tcpsrv, &TcpThread::serverUpdated, this, [this] (const STWsrv &stw) {
        ui->rb_Run->setChecked(stw.start);
        ui->cb_IP->setDisabled(stw.start);
        ui->le_Port->setDisabled(stw.start);
        if (stw.start) {
            FileIO::saveConfigToJson(QVariantMap{{"tcp", QVariantMap{
                                   {"ip", stw.host},
                                   {"port", stw.port}}
            }}); //сохраняем url
        }
    });
    connect(ui->pb_start, &QPushButton::clicked, this, [this]() { emit tcpsrv->startServer(ui->cb_IP->currentText(), ui->le_Port->text()); });
    connect(ui->pb_stop, &QPushButton::clicked, this, [this]() { emit tcpsrv->stopServer(); });
    connect(ui->pb_close, &QPushButton::clicked, this, [this]() { this->close(); });
}

void UiTcp::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    ui->cb_IP->clear();
    QStringList list = tcpsrv->getIPList();
    ui->cb_IP->addItems(list);

    int index = ui->cb_IP->findText(m_currentIP); //индекс элемента по тексту
    if (index != -1) ui->cb_IP->setCurrentIndex(index); //если нашли, устанавливаем как текущий
}

void UiTcp::restore(const QString &ip, const QString &port)
{
    m_currentIP = ip;
    ui->le_Port->setText(port);
}
