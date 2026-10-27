/* Класс со статическими методами для сохранения и чтения модели в JSON, сохраняем полную структуру модели */
#ifndef JSONMODELIO_H
#define JSONMODELIO_H

#include <QObject>
#include <QStandardItemModel>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "jsonmodelio.h"
#include "modelmapping.h"
#include "../systemlog/modelsystemlog.h"
#include "../fileio.h"


class JsonModelIO : public QObject
{
    Q_OBJECT
public:
    explicit JsonModelIO(QObject *parent = nullptr);
    ~JsonModelIO();


private:
    static QJsonArray saveRecursiveModel(const QStandardItemModel *model, const QStandardItem *parentItem);
    static ModelMapping *parseModel(const QJsonObject &rootObject);
    static void parseStrictData(const QJsonArray &rowsArray, QStandardItemModel *model);     //если структура строгая
    static void parseRecursiveStrictData(const QJsonValue &jsonValue, QStandardItemModel *model, QStandardItem *parentItem, uint userRole = 0);  //если вложенная структура произвольная
    static QList<QStandardItem*> createNodeRow(const QJsonObject &nodeObject, const uint &role);
    static void parseRecursiveData(const QJsonArray &rowsArray, QStandardItemModel *model, QStandardItem *parentItem);  //если структура произвольная
    static QStandardItem *parseCell(const QJsonObject &cellObject); //парсим items

public:
    /* статические методы, можем не создавать экземпляр класса для его использования
     * аналог синглтону, если не требуется сохранять состояние между вызовами класса */
    static bool saveModelToJson(const QStandardItemModel *model);
    static ModelMapping *loadModelFromJson();
    static ModelMapping *loadModelFromJson(const QString &filename);

signals:

};

#endif // JSONMODELIO_H
