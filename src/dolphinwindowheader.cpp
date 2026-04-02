#include "dolphinwindowheader.h"

DolphinWindowHeader::DolphinWindowHeader(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::DolphinWindowHeader)
{
    ui->setupUi(this);
}

DolphinWindowHeader::~DolphinWindowHeader()
{
    delete ui;
}
