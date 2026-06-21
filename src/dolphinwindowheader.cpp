#include "dolphinwindowheader.h"

DolphinWindowHeader::DolphinWindowHeader(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::DolphinWindowHeader)
{
    ui->setupUi(this);

    ui->searchBar->addAction(ui->actSearchOpts, QLineEdit::TrailingPosition);

    /* Force all three textboxes to be the same height */
    ui->primaryNavHole->setContentsMargins(QMargins(0, 0, 0, 0));
    ui->secondaryNavHole->setContentsMargins(QMargins(0, 0, 0, 0));
    ui->searchBar->setMaximumHeight(26);
}

DolphinWindowHeader::~DolphinWindowHeader()
{
    delete ui;
}
