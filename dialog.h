#ifndef DIALOG_H
#define DIALOG_H

#include <QObject>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QPointer>
#include <QApplication>
#include <QScreen>

class Dialog : public QDialog
{
    Q_OBJECT

    explicit Dialog(QWidget *parent = nullptr);
    ~Dialog();

    QLabel *label = nullptr;
    QPointer<QWidget> m_caller;  // ← автоматически становится nullptr при удалении объекта

public:
    static Dialog *getInstance();
    void setCaller(QWidget *caller); //устанавливаем вызывающее окно (для отображения по середине родителя)
    void setText(const QString &str); // + указатель на вызывающий класс
    void showEvent(QShowEvent *event);
};

#endif // DIALOG_H
