//AppLib бибилиотека приложения
//описываем UDT - пользовательские типы данных

#ifndef APPLIB_H
#define APPLIB_H

#include <QObject>
#include <QDateTime>
#include <QElapsedTimer>
#include <QOperatingSystemVersion>
#include <QWidget>
#include <QScreen>
#include <QSysInfo>
#include <QDateTime>
#include <openssl/aes.h>
#include "version/version.h"

class AppLib : public QObject
{
    Q_OBJECT

private:
    explicit AppLib(QObject *parent = nullptr);
    ~AppLib();

    struct QSS {
        //стили QRadioButton
        QString rb_redGreen;
        QString rb_whiteGreen;
        QString rb_whiteYellow;
        QString rb_whiteRed;
        QString treeView;
        QString treeView_height25;
        QString pb_dragdrop;
    };

public:
    //enum Типов сообщений
    enum LogType {
        Info = 0,
        Warning = 1,
        Error = 2,
        Success = 4
    };

    QSS qss;
    static AppLib *getInstance();  //создаем один экземпляр класса (паттерн Singleton)
    void moveWindowRelative(QWidget* parent, QWidget* child, int margin = 5); //ф-я центровки открытого окна
    /* getterts */
    QString getOSVersionString();
    QString getOpenSSLVersionString();
    QString getHostName() const { return QSysInfo::machineHostName(); }
    QString getAppVersion() const { return PROJECT_VERSION_FULL; }
    QDateTime getUptime();


signals:
};

#endif // APPLIB_H
