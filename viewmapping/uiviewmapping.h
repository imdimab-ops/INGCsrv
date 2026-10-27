/* Просмотр карты привязки адресов сервера */
#ifndef UIVIEWMAPPING_H
#define UIVIEWMAPPING_H

#include <QWidget>
#include <QCloseEvent>
#include <QMenu>
#include <QDesktopServices>
#include <QFileDialog>
#include "modelmapping.h"
#include "modelevent.h"
#include "jsonmodelio.h"
#include "../dialog.h"
#include "../applib.h"
#include "../systemlog/modelsystemlog.h"
#include "../opcua/opcuaclient.h"

namespace Ui {
class UiViewMapping;
}

class UiViewMapping : public QWidget
{
    Q_OBJECT

public:
    explicit UiViewMapping(QWidget *parent = nullptr);
    ~UiViewMapping();

private:
    Ui::UiViewMapping *ui;

    ModelMapping *modelMapping = ModelMapping::getInstance();
    ModelEvent *modelEvent = ModelEvent::getInstance();
    Dialog *dialog = Dialog::getInstance();
    AppLib *appLib = AppLib::getInstance();
    ModelSystemLog *modelSysLog = ModelSystemLog::getInstance();

    void setupUI();
    void setupConnections();
    void loadMapping();
    void closeEvent(QCloseEvent *event) override; //переопределяем closeEvent()
    void moveEvent(QMoveEvent *event) override;

private slots:

};

#endif // UIVIEWMAPPING_H
