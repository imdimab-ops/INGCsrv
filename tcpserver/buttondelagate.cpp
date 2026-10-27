#include "buttondelagate.h"

ButtonDelegate::ButtonDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

void ButtonDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionButton button;

    // Увеличиваем высоту кнопки (добавляем 8 пикселей к высоте)
    QRect buttonRect = option.rect.adjusted(3, 2, -3, -2);
    button.rect = buttonRect;

    button.text = "Disconnect";
    button.state = QStyle::State_Enabled;

    // Улучшенная обработка наведения мыши
    if (option.state & QStyle::State_MouseOver) {
        QPoint mousePos = QCursor::pos();
        if (auto view = qobject_cast<QAbstractItemView*>(parent())) {
            QPoint viewportPos = view->viewport()->mapFromGlobal(mousePos);
            QModelIndex hoverIndex = view->indexAt(viewportPos);
            if (hoverIndex == index && buttonRect.contains(viewportPos)) {
                button.state |= QStyle::State_MouseOver;
            }
        }
    }

    // Добавляем состояние нажатия
    if (pressedIndex == index) {
        button.state |= QStyle::State_Sunken;
    }

    QApplication::style()->drawControl(QStyle::CE_PushButton, &button, painter);
}

bool ButtonDelegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index)
{
    // Создаем такой же rect как в paint
    QRect buttonRect = option.rect.adjusted(3, 2, -3, -2);

    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        if (buttonRect.contains(mouseEvent->pos())) {
            pressedIndex = index;
            // Запрашиваем перерисовку
            if (auto view = qobject_cast<QAbstractItemView*>(parent())) {
                view->viewport()->update();
            }
            return true;
        }
        break;
    }

    case QEvent::MouseButtonRelease: {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        if (buttonRect.contains(mouseEvent->pos()) && pressedIndex == index) {
            pressedIndex = QModelIndex(); // Сбрасываем нажатую кнопку
            // Запрашиваем перерисовку
            if (auto view = qobject_cast<QAbstractItemView*>(parent())) {
                view->viewport()->update();
            }
            emit buttonClicked(index);
            return true;
        }
        // Если release вне кнопки, сбрасываем состояние
        if (pressedIndex == index) {
            pressedIndex = QModelIndex();
            if (auto view = qobject_cast<QAbstractItemView*>(parent())) {
                view->viewport()->update();
            }
        }
        break;
    }

    case QEvent::MouseMove: {
        // Обновляем внешний вид при движении мыши только для текущего индекса
        if (auto view = qobject_cast<QAbstractItemView*>(parent())) {
            // Обновляем только текущую ячейку для производительности
            view->viewport()->update(option.rect);
        }
        break;
    }

    default:
        break;
    }

    return QStyledItemDelegate::editorEvent(event, model, option, index);
}
