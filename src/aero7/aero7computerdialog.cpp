/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aero7computerdialog.h"

#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include <functional>

namespace {

QString sizeText(qint64 bytes)
{
    const double gib = bytes / (1024.0 * 1024.0 * 1024.0);
    return QStringLiteral("%1 GB").arg(gib, 0, 'f', gib < 10.0 ? 1 : 0);
}

QLabel *sectionTitle(const QString &title, int count)
{
    auto *label = new QLabel(QStringLiteral("▾  %1 (%2)").arg(title).arg(count));
    label->setStyleSheet(
        "QLabel { color: #174a86; font-weight: bold; border-bottom: 1px solid #c8d7e8; "
        "padding: 3px 0 4px 0; background: transparent; }");
    return label;
}

QWidget *driveTile(const QStorageInfo &storage, const QString &displayName,
                   const QString &iconName, QObject *context,
                   const std::function<void()> &open)
{
    auto *tile = new QWidget;
    tile->setMinimumWidth(255);
    tile->setMaximumWidth(360);
    auto *row = new QHBoxLayout(tile);
    row->setContentsMargins(5, 6, 5, 7);
    row->setSpacing(10);

    auto *drive = new QPushButton;
    drive->setObjectName(QStringLiteral("computerDrive"));
    drive->setFlat(true);
    drive->setCursor(Qt::PointingHandCursor);
    drive->setIcon(QIcon::fromTheme(iconName, QIcon::fromTheme(QStringLiteral("drive-harddisk"))));
    drive->setIconSize(QSize(48, 48));
    drive->setFixedSize(58, 58);
    drive->setStyleSheet("QPushButton { border: 0; background: transparent; padding: 0; }");
    drive->setToolTip(QStringLiteral("Open %1").arg(displayName));
    QObject::connect(drive, &QPushButton::clicked, context, open);
    row->addWidget(drive, 0, Qt::AlignTop);

    auto *details = new QVBoxLayout;
    details->setContentsMargins(0, 0, 0, 0);
    details->setSpacing(3);
    auto *name = new QPushButton(displayName);
    name->setFlat(true);
    name->setCursor(Qt::PointingHandCursor);
    name->setStyleSheet(
        "QPushButton { color: #111; border: 0; background: transparent; padding: 0; text-align: left; }"
        "QPushButton:hover { color: #0645ad; text-decoration: underline; }");
    QObject::connect(name, &QPushButton::clicked, context, open);
    details->addWidget(name);

    if (storage.bytesTotal() > 0) {
        auto *capacity = new QProgressBar;
        capacity->setObjectName(QStringLiteral("driveCapacity"));
        capacity->setRange(0, 1000);
        const qint64 used = storage.bytesTotal() - storage.bytesAvailable();
        capacity->setValue(int((used * 1000) / storage.bytesTotal()));
        capacity->setTextVisible(false);
        capacity->setFixedHeight(13);
        capacity->setStyleSheet(
            "QProgressBar { border: 1px solid #9aa7b3; background: white; }"
            "QProgressBar::chunk { background: #3aa5df; border: 1px solid #1787c3; }");
        details->addWidget(capacity);

        auto *free = new QLabel(QStringLiteral("%1 free of %2")
            .arg(sizeText(storage.bytesAvailable()), sizeText(storage.bytesTotal())));
        free->setStyleSheet("color: #4a4a4a; background: transparent;");
        details->addWidget(free);
    }
    row->addLayout(details, 1);
    return tile;
}

} // namespace

Aero7ComputerView::Aero7ComputerView(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("aero7ComputerView"));
    setStyleSheet("#aero7ComputerView { background: white; }");
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto *scroll = new QScrollArea;
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    m_content = new QWidget;
    m_content->setStyleSheet("background: white;");
    scroll->setWidget(m_content);
    outer->addWidget(scroll);
    refresh();
}

bool Aero7ComputerView::isUserVisibleStorage(const QStorageInfo &storage)
{
    if (!storage.isValid() || !storage.isReady() || storage.bytesTotal() <= 0)
        return false;
    const QString root = QDir::cleanPath(storage.rootPath());
    if (root == QLatin1String("/"))
        return true;
    const QString user = qEnvironmentVariable("USER");
    return root.startsWith(QStringLiteral("/run/media/%1/").arg(user))
        || root.startsWith(QStringLiteral("/media/%1/").arg(user));
}

void Aero7ComputerView::refresh()
{
    if (QLayout *old = m_content->layout()) {
        while (QLayoutItem *item = old->takeAt(0)) {
            delete item->widget();
            delete item;
        }
        delete old;
    }
    auto *layout = new QVBoxLayout(m_content);
    layout->setContentsMargins(8, 15, 12, 14);
    layout->setSpacing(10);

    QList<QStorageInfo> hard;
    QList<QStorageInfo> removable;
    for (const QStorageInfo &storage : QStorageInfo::mountedVolumes()) {
        if (!isUserVisibleStorage(storage))
            continue;
        (QDir::cleanPath(storage.rootPath()) == QLatin1String("/") ? hard : removable)
            .append(storage);
    }
    if (removable.isEmpty())
        removable.append(QStorageInfo());

    int nextLetter = 3; // C is the system disk; removable media starts at D.
    auto addGroup = [this, layout, &nextLetter](const QString &title,
                                                const QList<QStorageInfo> &volumes,
                                                bool local) {
        layout->addWidget(sectionTitle(title, volumes.size()));
        auto *gridWidget = new QWidget;
        auto *grid = new QGridLayout(gridWidget);
        grid->setContentsMargins(0, 0, 8, 4);
        grid->setHorizontalSpacing(28);
        grid->setVerticalSpacing(2);
        for (int index = 0; index < volumes.size(); ++index) {
            const QStorageInfo storage = volumes.at(index);
            QString volumeName;
            if (!storage.isValid()) {
                volumeName = QStringLiteral("CD Drive (D:)");
            } else if (local) {
                volumeName = QStringLiteral("Local Disk (C:)");
            } else {
                QString label = storage.displayName().trimmed();
                if (label.isEmpty())
                    label = QStringLiteral("Removable Disk");
                volumeName = QStringLiteral("%1 (%2:)")
                    .arg(label, QString(QChar('A' + nextLetter++)));
            }
            const QString root = local ? QDir::homePath() : storage.rootPath();
            grid->addWidget(driveTile(storage, volumeName,
                                      local ? QStringLiteral("drive-harddisk-root")
                                            : (storage.isValid()
                                                   ? QStringLiteral("drive-removable-media")
                                                   : QStringLiteral("drive-optical")),
                                      this, [this, root]() {
                                          if (!root.isEmpty())
                                              Q_EMIT openRequested(root);
                                      }),
                            index / 2, index % 2);
        }
        if (volumes.isEmpty()) {
            auto *empty = new QLabel(QStringLiteral("No devices are connected."));
            empty->setStyleSheet("color: #666; padding: 8px 4px;");
            grid->addWidget(empty, 0, 0);
        }
        grid->setColumnStretch(0, 1);
        grid->setColumnStretch(1, 1);
        layout->addWidget(gridWidget);
    };

    addGroup(QStringLiteral("Hard Disk Drives"), hard, true);
    addGroup(QStringLiteral("Devices with Removable Storage"), removable, false);
    layout->addStretch(1);

}
