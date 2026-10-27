#include "dialog.h"

Dialog::Dialog(QWidget *parent)
    : QDialog(parent)
{
    this->setWindowTitle("Dialog");
    this->setModal(true);
    QDialogButtonBox *btn_box = new QDialogButtonBox(this);
    btn_box->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    btn_box->setCenterButtons(true);

    connect(btn_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btn_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QFormLayout *layout = new QFormLayout(this);
    label = new QLabel(this);
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet("QLabel{font-size: 16pt; margin: 10}");
    label->setText("test_test_test");
    layout->addWidget(label);
    layout->addWidget(btn_box);
    this->setLayout(layout);
}

Dialog::~Dialog()
{

}

Dialog *Dialog::getInstance()
{
    // static Dialog instance;
    // return &instance;

    /* т.к. мы унаследовались от QDialog а не от QObject (от QObject стандартный паттерн сигнлтон работает без замечаний)
     * создаем синглон так, т.к. при удалении QtWidgets (где находится QDialog) вероятно выгружается раньше,
     * чем разрушается статический объект, поэтому приложение завершается с ошибкой.
     * qAddPostRoutine гарантирует что синглтон будет корректно удален до того как Qt освободит свои внутренние ресурсы,
     * что предотвращает ошибки сегментации и проблемы с RTTI (Run-Time Type Information)
     * QApplication::exec()  →  qAddPostRoutine функции  →  статические деструкторы  →  выгрузка библиотек Qt */

    static Dialog *instance = nullptr;
    if (!instance) {
        instance = new Dialog();
        qAddPostRoutine( []() {
            if (instance) {
                delete instance;
                instance = nullptr;
            }
        });
    }
    return instance;
}

void Dialog::setCaller(QWidget *caller)
{
    if (caller) m_caller = caller;
}

void Dialog::setText(const QString &str)
{
    if (label) label->setText(str);
}

void Dialog::showEvent(QShowEvent *event)
{
    //при отображении диалогового окна отображаем по центру родителя
    if (m_caller) {
        //получаем экран родителя
        QScreen *screen = QGuiApplication::screenAt(m_caller.get()->mapToGlobal(QPoint(0, 0)));
        if (!screen) screen = QGuiApplication::primaryScreen();

        //центрируем относительно родителя
        QPoint parentCenter = m_caller.get()->mapToGlobal(m_caller.get()->rect().center());
        QPoint dialogCenter(parentCenter.x() - this->width()/2, parentCenter.y() - this->height()/2);

        //ограничиваем положение экраном
        QRect screenGeometry = screen->availableGeometry();
        QRect dialogGeometry(dialogCenter.x(), dialogCenter.y(), this->width(), this->height());

        //корректируем позицию
        if (dialogGeometry.left() < screenGeometry.left()) {
            dialogGeometry.moveLeft(screenGeometry.left());
        }
        if (dialogGeometry.right() > screenGeometry.right()) {
            dialogGeometry.moveRight(screenGeometry.right());
        }
        if (dialogGeometry.top() < screenGeometry.top()) {
            dialogGeometry.moveTop(screenGeometry.top());
        }
        if (dialogGeometry.bottom() > screenGeometry.bottom()) {
            dialogGeometry.moveBottom(screenGeometry.bottom());
        }
        this->move(dialogGeometry.topLeft());
    };
    /* показываем 1 раз и удаляем, чтобы при следующеи вызове без установки звонящего
     * виджет открывался по середине экрана монитора */
    m_caller.clear();
}

