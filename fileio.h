/* Класс со статическими методами для сохранения и чтения конфигурации в JSON */
#ifndef FILEIO_H
#define FILEIO_H

#include <QObject>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "systemlog/modelsystemlog.h"

class FileIO : public QObject
{
    Q_OBJECT
public:
    explicit FileIO(QObject *parent = nullptr);
    ~FileIO();

private:
    static QJsonObject loadConfigFromJson(const QString &filename);

    const uint16_t MAX_COUNT = 1500;       //количество записей
    uint16_t count = 0;

public:
    static qint64 saveToFile(const QString &filepath, const QByteArray &barray);
    static QByteArray loadFromFile(const QString &filepath);
    static bool saveConfigToJson(const QVariantMap &map);
    static QVariantMap loadConfigFromJson();
    static QString getConfigDir();
    static QString getLogDir();

signals:

};

#endif // FILEIO_H
