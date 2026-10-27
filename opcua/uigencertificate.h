#ifndef UIGENCERTIFICATE_H
#define UIGENCERTIFICATE_H

#include <QDialog>
#include <QDesktopServices>
#include "opcuaclient.h"
#include "../systemlog/uisystemlog.h"

namespace Ui {
class UiGenCertificate;
}

class UiGenCertificate : public QDialog
{
    Q_OBJECT

public:
    explicit UiGenCertificate(QWidget *parent = nullptr);
    ~UiGenCertificate();

private:
    Ui::UiGenCertificate *ui;

    OPCUAClient *opcClient = OPCUAClient::getInstance();
    ModelSystemLog *modelSysLog = ModelSystemLog::getInstance();
    void settingsUI();

    void closeEvent(QCloseEvent *event) override; //переопределем closeEvent()

};

#endif // UIGENCERTIFICATE_H
