#ifndef DOLPHINWINDOWHEADER_H
#define DOLPHINWINDOWHEADER_H

#include <QWidget>
#include "ui_dolphinwindowheader.h"

class DolphinMainWindow;

class DolphinWindowHeader : public QWidget
{
    Q_OBJECT
    friend class DolphinMainWindow;

public:
    explicit DolphinWindowHeader(QWidget *parent = nullptr);
    ~DolphinWindowHeader();

private:
    Ui::DolphinWindowHeader *ui;
};

#endif // DOLPHINWINDOWHEADER_H
