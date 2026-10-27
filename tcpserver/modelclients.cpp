#include "modelclients.h"

ModelClients::ModelClients(QObject *parent)
    : QStandardItemModel{parent}
{
    this->setHorizontalHeaderLabels({"№", "DateTime", "Name", "OS", "AppVer", "Uptime", "Peer", "Tx", "Rx", "Disconn"});
    connect(tcpsrv, &TcpThread::clientsUpdated, this, &ModelClients::update);
}

ModelClients::~ModelClients()
{

}

void ModelClients::update(const QList<ClientSnapshot> &clients)
{
    //обновляем только нужные ячейки
    if (clients.size() == this->rowCount()) {
        changedRow(clients);
        return;
    }
    //обновляем всю таблицу (создаем заново)
    if (clients.size() != this->rowCount()) {
        clear();
        quint16 i = 1;
        for (const ClientSnapshot &client : std::as_const(clients)) { //получаем доступ по ссылке
            QStringList list;
            list.append(QString::number(i++));
            list.append(client.connectDateTime.toDateTime().toString("dd.MM.yyyy hh:mm:ss"));
            list.append(client.objectName.toString());
            list.append(client.osver.toString());
            list.append(client.appver.toString());
            list.append(client.uptime.toTime().toString("hh:mm:ss"));
            list.append(QString("%1:%2").arg(client.peer_ip.toString(), client.peer_port.toString()));
            list.append(client.Tx.toString());
            list.append(client.Rx.toString());
            list.append("button");
            createNewRow(list);
        }
    }
}

void ModelClients::changedRow(const QList<ClientSnapshot> &clients)
{
    //делаем в одном цикле т.к. чередование идет по порядку + дополнительная проверка по условию
    quint16 i = 0;
    for (const ClientSnapshot &client : std::as_const(clients)) { //получаем доступ по ссылке
        if (i < this->rowCount() && this->item(i, 2)->text() == client.objectName) {
            this->item(i, 5)->setData(client.uptime.toTime().toString("hh:mm:ss"), Qt::DisplayRole);  //uptime
            this->item(i, 7)->setData(client.Tx, Qt::DisplayRole);  //TxKb
            this->item(i, 8)->setData(client.Rx, Qt::DisplayRole);  //RxKb
        }
        i++;
    }
}

void ModelClients::createNewRow(const QStringList &list)
{
    QList<QStandardItem*> row;
    for(auto i = 0; i < list.size(); i++) {
        QStandardItem *item = new QStandardItem(list.at(i));
        Qt::Alignment alignment;
        switch(i) {
        case 2:  //socket->objectName()
            alignment = Qt::AlignLeft | Qt::AlignVCenter;
            break;
        case 3:  //clientInfo.OSver
            alignment = Qt::AlignLeft | Qt::AlignVCenter;
            break;
        case 4:  //clientInfo.appver
            alignment = Qt::AlignLeft | Qt::AlignVCenter;
            break;
        default:
            alignment = Qt::AlignCenter;
            break;
        }
        item->setTextAlignment(alignment);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable); //Устанавливаем только флаги для отображения и выбора
        row << item;
    }
    row.last()->setData(list.at(2), Qt::UserRole + 1);  //socket->objectName() добавляем для возможности удаления
    this->appendRow(row);
}

void ModelClients::clear()
{
    if (this->rowCount() > 0) this->removeRows(0, this->rowCount());
}

void ModelClients::closeClient(const QModelIndex &index)
{
    if (!index.isValid()) return;
    if (this->data(index, Qt::UserRole + 1).isValid()) {
        QString client = this->data(index, Qt::UserRole + 1).toString();
        dialog->setWindowTitle("Disconnect");
        dialog->setText("Disconnect client: " + client);
        if(dialog->exec() == QDialog::Accepted) {   //вызываем диалог для подтверждения
            emit tcpsrv->disconnectClient(client);
        }
    }
}
