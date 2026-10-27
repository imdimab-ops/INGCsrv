#include "fileio.h"

FileIO::FileIO(QObject *parent)
    : QObject{parent}
{

}

FileIO::~FileIO()
{

}

QJsonObject FileIO::loadConfigFromJson(const QString &filename)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(FileIO::loadFromFile(FileIO::getConfigDir() + filename), &parseError);

    //проверяем на ошибки парсинга
    if (parseError.error != QJsonParseError::NoError) {
        auto *modelSysLog = ModelSystemLog::getInstance();
        modelSysLog->append(Q_FUNC_INFO, QString("[Error] JSON parse error in %1 at offset %2: %3")
                                             .arg(filename, QString::number(parseError.offset), parseError.errorString()));
        return QJsonObject();
    }

    if (doc.isNull() || !doc.isObject()) {
        auto *modelSysLog = ModelSystemLog::getInstance();
        modelSysLog->append(Q_FUNC_INFO, QString("[Error] JSON isNull or !isObject in file: %1, %2").arg(filename));
        return QJsonObject();
    }
    return doc.object();
}

qint64 FileIO::saveToFile(const QString &filepath, const QByteArray &barray)
{
    QFileInfo fileInfo(filepath);
    QString directoryPath = fileInfo.absolutePath(); //берем абсолютный путь из дирректории

    QDir dir;
    dir.mkdir(directoryPath); //создаем дирректорию
    QFile file(filepath);
    if (!file.open(QIODevice::WriteOnly)) {
        auto *modelSysLog = ModelSystemLog::getInstance();
        modelSysLog->append(Q_FUNC_INFO, QString("[Error] Open file: %1, %2").arg(filepath, file.errorString()));
        return 0;
    }
    qint64 byteswrite = file.write(barray);
    file.close();
    return byteswrite;
}

QByteArray FileIO::loadFromFile(const QString &filepath)
{
    QFile file(filepath);
    if (!file.open(QIODevice::ReadOnly)) {
        auto *modelSysLog = ModelSystemLog::getInstance();
        modelSysLog->append(Q_FUNC_INFO, QString("[Error] Open file: %1, %2").arg(filepath, file.errorString()));
        return QByteArray();
    }
    QByteArray barray = file.readAll();
    file.close();
    return barray;
}

bool FileIO::saveConfigToJson(const QVariantMap &map)
{
    /* сохраняем конфигурацию:
     * 1.загружаем текущую конфигурацию
     * 2.вносим изменения
     * 3.сохраняем !*/
    QJsonObject rootObject = loadConfigFromJson("config.json");
    rootObject["0_datatype"] = "Config";
    rootObject["1_datetime"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    if (map.value("opcua").isValid()) {
        QJsonObject obj;
        obj["url"] = map.value("opcua").toMap().value("url").toString();
        obj["login"] = map.value("opcua").toMap().value("login").toString();
        obj["pass"] = map.value("opcua").toMap().value("pass").toString();
        rootObject["2_opcua"] = obj;
    }
    if (map.value("tcp").isValid()) {
        QJsonObject obj;
        obj["ip"] = map.value("tcp").toMap().value("ip").toString();
        obj["port"] = map.value("tcp").toMap().value("port").toString();
        rootObject["3_tcp"] = obj;
    }
    rootObject["4_data"] = "data";
    qint64 byteswrite = saveToFile(getConfigDir() + "config.json", QJsonDocument(rootObject).toJson(QJsonDocument::Indented)); //Indented, Compact - формат записи
    if (byteswrite != 0) return true;
    return false;
}

QVariantMap FileIO::loadConfigFromJson()
{
    QVariantMap map;
    const QJsonObject rootObject = loadConfigFromJson("config.json");
    if (auto it = rootObject.find("2_opcua"); it != rootObject.end()) { //C++17 init-statement, it живет только внутри if
        const QJsonObject obj = it->toObject();
        map.insert("opcua", QVariantMap{
                                {"url",   obj.value("url").toVariant()},
                                {"login", obj.value("login").toVariant()},
                                {"pass",  obj.value("pass").toVariant()}
                            });
    }
    if (auto it = rootObject.find("3_tcp"); it != rootObject.end()) {
        const QJsonObject obj = it->toObject();
        map.insert("tcp", QVariantMap{
                              {"ip",   obj.value("ip").toVariant()},
                              {"port", obj.value("port").toVariant()}
                          });
    }
    return map;
}

QString FileIO::getConfigDir()
{
    return QDir::currentPath() + "/config/";
}

QString FileIO::getLogDir()
{
    return QDir::currentPath() + "/log/";
}

