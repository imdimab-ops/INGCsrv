#ifndef UIOPCUA_H
#define UIOPCUA_H

#include <QWidget>
#include <QStandardItemModel>
#include <QButtonGroup>
#include "opcuaclient.h"
#include "modelnodes.h"
#include "../systemlog/modelsystemlog.h"
#include "../applib.h"
#include "../engine.h"
#include "../fileio.h"

namespace Ui {
class UiOPCUA;
}

class UiOPCUA : public QWidget
{
    Q_OBJECT


public:
    explicit UiOPCUA(QWidget *parent = nullptr);
    ~UiOPCUA();

private:
    Ui::UiOPCUA *ui;
    QStandardItemModel *model;
    QStandardItemModel *modelTree;

    //Кастомные роли
    enum SecurityRoles {
        securityPolicy = Qt::UserRole + 0,
        securityMode = Qt::UserRole + 1,
    };

    AppLib *applib = AppLib::getInstance();
    OPCUAClient *opcClient = OPCUAClient::getInstance();
    ModelSystemLog *modelSysLog = ModelSystemLog::getInstance();
    ModelNodes *modelNodes = ModelNodes::getInstance();
    Engine *engine = Engine::getInstance();
    void setupUI();
    void setupConnections();

private slots:
    void getEndpoint();

public:
    void restore(const QString &url, const QString &login, const QString &pass);

};

#endif // UIOPCUA_H
