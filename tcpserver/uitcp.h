#ifndef UITCP_H
#define UITCP_H

#include <QDialog>

#include "tcpthread.h"
#include "../applib.h"
#include "../systemlog/uisystemlog.h"
#include "../fileio.h"

namespace Ui {
class UiTcp;
}

class UiTcp : public QDialog
{
    Q_OBJECT

public:
    explicit UiTcp(QWidget *parent = nullptr);
    ~UiTcp();

private:
    Ui::UiTcp *ui;

    QString m_currentIP = "";
    TcpThread *tcpsrv = TcpThread::getInstance();
    AppLib *applib = AppLib::getInstance();
    ModelSystemLog *modelSysLog = ModelSystemLog::getInstance();

    void setupUI();
    void setupConnections();
    void showEvent(QShowEvent *event) override; //переопределем QShowEvent()

public:
    void restore(const QString &ip, const QString &port);
};

#endif // UITCP_H
