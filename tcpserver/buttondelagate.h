#ifndef BUTTONDELEGATE_H
#define BUTTONDELEGATE_H

#include <QStyledItemDelegate>
#include <QPushButton>
#include <QApplication>
#include <QMouseEvent>
#include <QAbstractItemView>

class ButtonDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit ButtonDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index) override;

signals:
    void buttonClicked(const QModelIndex &index);

private:
    mutable QModelIndex pressedIndex; // Запомнить нажатую кнопку

};

#endif // BUTTONDELEGATE_H





