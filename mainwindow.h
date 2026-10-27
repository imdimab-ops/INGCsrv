#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMessageBox>
#include <QCloseEvent>
#include <QSystemTrayIcon>
#include <QWidgetAction>
#include "applib.h"
#include "dialog.h"
#include "engine.h"
#include "systemlog/uisystemlog.h"
#include "systemlog/modelsystemlog.h"
#include "opcua/uiopcua.h"
#include "opcua/uigencertificate.h"
#include "opcua/opcuaclient.h"
#include "opcua/uibrowsenode.h"
#include "tcpserver/uitcp.h"
#include "tcpserver/modelclients.h"
#include "tcpserver/buttondelagate.h"
#include "viewmapping/uiviewmapping.h"
#include "fileio.h"


QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;

    AppLib *applib = AppLib::getInstance();
    OPCUAClient *opcClient = OPCUAClient::getInstance();
    TcpThread *tcpsrv = TcpThread::getInstance();
    ModelSystemLog *modelSysLog = ModelSystemLog::getInstance();
    Dialog *dialog = Dialog::getInstance();
    Engine *engine = Engine::getInstance();
    UiOPCUA uiOPCUA;
    UiGenCertificate uiGenCert;
    UiBrowseNode uiBrowseNode;
    UiSystemLog uiSystemLog;
    UiTcp uiTcp;
    UiViewMapping uiViewMapping;
    bool m_tcprun = false; //флаги работы сервиса
    bool m_opcrun = false;

    void setupUI();
    void setupConnections();
    void restoreConfig();

    void closeEvent(QCloseEvent *event) override; //переопределим closeEvent()
    void changeEvent(QEvent *event) override; // переопределяем changeEvent()
    void showCenter(QWidget &child);
    /* System Tray */
    void initTray(); //инициализируем работу с треем
    void activatedTray(QSystemTrayIcon::ActivationReason reason);
};
#endif // MAINWINDOW_H
