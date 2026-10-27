#include "uigencertificate.h"
#include "ui_uigencertificate.h"

UiGenCertificate::UiGenCertificate(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::UiGenCertificate)
{
    ui->setupUi(this);
    this->setWindowTitle("Generate Certificate");
    settingsUI();
}

UiGenCertificate::~UiGenCertificate()
{
    delete ui;
}

void UiGenCertificate::settingsUI()
{
    //Запрещаем изменение размеров окна
    setFixedSize(this->size());

    connect(ui->btn_saveCert, &QPushButton::clicked, this, [this] () {
        ui->label_status->setText("...");
        bool res = opcClient->saveCertificatesToFile();
        if (res) {
            ui->label_status->setText("✓");
            modelSysLog->append(Q_FUNC_INFO, QString("[Success] ✓ Generate Certificate"));
        }
    });
    connect(ui->btn_openDir, &QPushButton::clicked, this, [this] () {
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(opcClient->getCertDir()))) {
            modelSysLog->append(Q_FUNC_INFO, QString("[Error] filed open dir:%1").arg(opcClient->getCertDir()));
        };
    });
    connect(ui->buttonBox, &QDialogButtonBox::clicked, this, &UiGenCertificate::close);
}

void UiGenCertificate::closeEvent(QCloseEvent *event)
{
    ui->label_status->setText("...");
}
