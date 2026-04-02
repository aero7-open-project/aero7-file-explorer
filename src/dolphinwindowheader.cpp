#include "dolphinwindowheader.h"

DolphinWindowHeader::DolphinWindowHeader(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::DolphinWindowHeader)
{
    ui->setupUi(this);

    ui->searchBar->addAction(ui->actSearchOpts, QLineEdit::TrailingPosition);
}

DolphinWindowHeader::~DolphinWindowHeader()
{
    delete ui;
}
