#include "uisystemlog.h"
#include "ui_uisystemlog.h"

UiSystemLog::UiSystemLog(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::UiSystemLog)
{
    ui->setupUi(this);
    this->setWindowTitle("System Log");
    settingsUI();
}

UiSystemLog::~UiSystemLog()
{
    delete ui;
}

void UiSystemLog::settingsUI()
{
    ui->pb_clear->setIcon(QIcon(":img/icon/clean.svg"));
    ui->pb_close->setIcon(QIcon(":img/icon/mainmenu/close.svg"));

    ui->cb_autoscroll->setChecked(true);

    ModelSystemLog *modelSysLog = ModelSystemLog::getInstance();
    ui->listView->setModel(modelSysLog);
    connect(ui->pb_clear, &QPushButton::clicked, this, [this, modelSysLog] () { modelSysLog->clear(); });
    connect(modelSysLog, &ModelSystemLog::rowsInserted, this, [this]() { if (ui->cb_autoscroll->isChecked()) ui->listView->scrollToBottom(); });
    connect(ui->pb_close, &QPushButton::clicked, this, &UiSystemLog::close);

    connect(ui->pb_save, &QPushButton::clicked, this, [this, modelSysLog]() {
        QString filepath = QFileDialog::getSaveFileName(
            this,                               //Родительское окно
            "Сохранить лог событий",            //Заголовок окна
            QDir::homePath(),                   //Начальная директория
            "Текстовые файлы (*.log);;Все файлы (*)"  //Фильтры
            );
        if (!filepath.isEmpty()) {
            QStringList list = modelSysLog->getStringList();
            FileIO::saveToFile(filepath, list.join("\n").toUtf8());
        }
    });
}
