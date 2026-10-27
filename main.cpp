#include "mainwindow.h"

#include <QApplication>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
    qRegisterMetaType<Endpoint>("Endpoint");
    qRegisterMetaType<NodeOPC>("NodeOPC");
    qRegisterMetaType<NodeAttribute>("NodeAttribute");

    QApplication a(argc, argv);
    a.setStyle(QStyleFactory::create("Fusion"));
    a.setWindowIcon(QIcon(":/img/logo.png"));
    MainWindow w;
    QApplication::setQuitOnLastWindowClosed(false); //отключаем автоматический выход при закрытии окон
    // QScreen *screen = QGuiApplication::primaryScreen();
    // w.move(screen->geometry().width() - w.width() - 200, screen->geometry().height()/2 - w.height()/2);
    w.show();
    return a.exec();
}
