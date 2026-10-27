#ifndef UISYSTEMLOG_H
#define UISYSTEMLOG_H

#include <QWidget>
#include <QFileDialog>
#include "modelsystemlog.h"
#include "../fileio.h"

namespace Ui {
class UiSystemLog;
}

class UiSystemLog : public QWidget
{
    Q_OBJECT

public:
    explicit UiSystemLog(QWidget *parent = nullptr);
    ~UiSystemLog();


private:
    Ui::UiSystemLog *ui;
    void settingsUI();

signals:

};

#endif // UISYSTEMLOG_H
