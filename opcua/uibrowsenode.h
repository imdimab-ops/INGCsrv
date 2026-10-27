#ifndef UIBROWSENODE_H
#define UIBROWSENODE_H

#include <QWidget>
#include <QMouseEvent>
#include <QDrag>
#include <QMimeData>
#include <QJsonObject>
#include "opcuaclient.h"
#include "modelnodes.h"
#include "modelattributes.h"
#include "../applib.h"

namespace Ui {
class UiBrowseNode;
}

class UiBrowseNode : public QWidget
{
    Q_OBJECT

public:
    explicit UiBrowseNode(QWidget *parent = nullptr);
    ~UiBrowseNode();

private:
    Ui::UiBrowseNode *ui;

    QString nodeId_current;

    OPCUAClient *opcClient = OPCUAClient::getInstance();
    ModelNodes *modelNodes = ModelNodes::getInstance();
    ModelAttributes *modelAttributes = ModelAttributes::getInstance();
    AppLib *appLib = AppLib::getInstance();
    void setupUI();
    void setupConnections();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    QPixmap createDragPixmap(const QString &name = "...");

private slots:


};

#endif // UIBROWSENODE_H
