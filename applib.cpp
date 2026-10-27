#include "applib.h"

AppLib::AppLib(QObject *parent)
    : QObject{parent}
{
    //стили QRadioButton
    qss.rb_redGreen = "QRadioButton::indicator {"
                      "width: 15px;"
                      "height: 15px;}"
                      "QRadioButton::indicator::unchecked {"
                      "border: 1px solid;"
                      "border-radius: 5px;"
                      "border-color: black;"
                      "background-color: red; }"
                      "QRadioButton::indicator::checked {"
                      "border: 1px solid;"
                      "border-radius: 5px;"
                      "border-color: black;"
                      "background-color: green; }";
    qss.rb_whiteGreen = "QRadioButton::indicator {"
                        "width: 15px;"
                        "height: 15px;}"
                        "QRadioButton::indicator::unchecked {"
                        "border: 1px solid;"
                        "border-radius: 5px;"
                        "border-color: black;"
                        "background-color: white; }"
                        "QRadioButton::indicator::checked {"
                        "border: 1px solid;"
                        "border-radius: 5px;"
                        "border-color: black;"
                        "background-color: green; }";
    qss.rb_whiteYellow = "QRadioButton::indicator {"
                         "width: 15px;"
                         "height: 15px;}"
                         "QRadioButton::indicator::unchecked {"
                         "border: 1px solid;"
                         "border-radius: 5px;"
                         "border-color: black;"
                         "background-color: white; }"
                         "QRadioButton::indicator::checked {"
                         "border: 1px solid;"
                         "border-radius: 5px;"
                         "border-color: black;"
                         "background-color: yellow; }";
    qss.rb_whiteRed = "QRadioButton::indicator {"
                      "width: 15px;"
                      "height: 15px;}"
                      "QRadioButton::indicator::unchecked {"
                      "border: 1px solid;"
                      "border-radius: 5px;"
                      "border-color: black;"
                      "background-color: white; }"
                      "QRadioButton::indicator::checked {"
                      "border: 1px solid;"
                      "border-radius: 5px;"
                      "border-color: black;"
                      "background-color: red; }";
    qss.treeView = "QTreeView {"
                   "alternate-background-color: #f8f8ff;"
                   "background-color: #fffff8;"
                   "}"
                   "QTreeView::item {"
                   "border-bottom: 1px solid #e0e0e0;"
                   "padding: 1px;"
                   "}"
                   "QTreeView::item:selected {"
                   "background-color: #fffff8;"     /* Основной цвет */
                   "color: black;"
                   "border: 1px dotted #ca3a27;"    /* Пунктирная граница вместо заливки */
                   "}"
                   "QTreeView::item:selected:alternate {"
                   "background-color: #f8f8ff;"     /* Альтернативный цвет */
                   "}";
    qss.treeView_height25 = "QTreeView {"
                            "alternate-background-color: #f8f8ff;"
                            "background-color: #fffff8;"
                            "}"
                            "QTreeView::item {"
                            "border-bottom: 1px solid #e0e0e0;"
                            "padding: 1px;"
                            "height: 25px;"
                            "}"
                            "QTreeView::item:selected {"
                            "background-color: #fffff8;"     /* Основной цвет */
                            "color: black;"
                            "border: 1px dotted #ca3a27;"    /* Пунктирная граница вместо заливки */
                            "}"
                            "QTreeView::item:selected:alternate {"
                            "background-color: #f8f8ff;"     /* Альтернативный цвет */
                            "}";
    qss.pb_dragdrop = "QPushButton {"
                      "    background-color: #4CAF50;"
                      "    color: white;"
                      "    border: none;"
                      "    padding: 8px;"
                      "    border-radius: 4px;"
                      "}"
                      "QPushButton:hover {"
                      "    background-color: #45a049;"
                      "}"
                      "QPushButton:pressed {"
                      "    background-color: #3d8b40;"
                      "}";

}

AppLib::~AppLib()
{

}

AppLib *AppLib::getInstance()
{
    static AppLib instance;
    return &instance;
}

void AppLib::moveWindowRelative(QWidget *parent, QWidget *child, int margin)
{
    child->close();
    QScreen* screen = parent->screen();
    if (!screen) return;

    QRect screenRect = screen->availableGeometry();

    int x_left = parent->x() - child->width() - margin;
    int x_right = parent->x() + parent->width() + margin;
    int y = parent->y();

    int x_pos = (x_left >= screenRect.x()) ? x_left :
                    ((x_right + child->width() <= screenRect.right()) ? x_right :
                         qMax(screenRect.x(), parent->x() + (parent->width() - child->width()) / 2));

    y = qBound(screenRect.y(), y, screenRect.bottom() - child->height());
    child->move(x_pos, y);
    child->show();
}

QString AppLib::getOSVersionString()
{
    auto OS = QOperatingSystemVersion::current();
    return QString("OS: %1 %2.%3.%4").arg(OS.name()).arg(OS.majorVersion()).arg(OS.minorVersion()).arg(OS.microVersion());
}

QString AppLib::getOpenSSLVersionString()
{
    return OPENSSL_VERSION_TEXT;
}

QDateTime AppLib::getUptime()
{
    static const QDateTime START_TIME = QDateTime::currentDateTime(); //static - инициализация только при первом вызове
    auto secondsPassed = START_TIME.secsTo(QDateTime::currentDateTime()); //сколько прошло времени
    QDateTime zeroTime = QDateTime(QDate(1, 1, 1), QTime(0, 0, 0)); //cоздаем валидный QDateTime QDate(1, 1, 1) - минимальное значение
    return zeroTime.addSecs(secondsPassed);
}
