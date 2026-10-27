#ifndef MODELCLIENTS_H
#define MODELCLIENTS_H

#include <QStandardItemModel>
#include <QPushButton>
#include "tcpthread.h"
#include "../dialog.h"

class ModelClients : public QStandardItemModel
{
    Q_OBJECT
public:
    explicit ModelClients(QObject *parent = nullptr);
    ~ModelClients();

private:
    TcpThread *tcpsrv = TcpThread::getInstance();
    Dialog *dialog = Dialog::getInstance();

    void update (const QList<ClientSnapshot> &clients);
    void changedRow(const QList<ClientSnapshot> &clients);
    void createNewRow(const QStringList &list);
    void clear();

public:
    void closeClient(const QModelIndex &index);
};

#endif // MODELCLIENTS_H
