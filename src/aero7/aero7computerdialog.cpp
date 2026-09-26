/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aero7computerdialog.h"
#include "aero7storage.h"
#include "aero7storagedevices.h"
#include "aero7mountwatcher.h"

#include <QDir>
#include <QApplication>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>

#include <functional>

namespace {

QLabel *sectionTitle(const QString &title, int count)
{
    auto *label = new QLabel(QStringLiteral("▾  %1 (%2)").arg(title).arg(count));
    label->setStyleSheet(
        "QLabel { color: #174a86; font-weight: bold; border-bottom: 1px solid #c8d7e8; "
        "padding: 3px 0 4px 0; background: transparent; }");
    return label;
}

QWidget *driveTile(const QStorageInfo &storage, const QString &displayName,
                   const QString &iconName, const QString &deviceId, QObject *context,
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
    drive->setProperty("storageRoot", storage.rootPath());
    drive->setProperty("deviceId", deviceId);
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
    name->setProperty("storageRoot", storage.rootPath());
    name->setProperty("deviceId", deviceId);
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
            .arg(Aero7Storage::sizeText(storage.bytesAvailable()), Aero7Storage::sizeText(storage.bytesTotal())));
        free->setStyleSheet("color: #4a4a4a; background: transparent;");
        details->addWidget(free);
    } else if (!deviceId.isEmpty()) {
        auto *hint = new QLabel(QStringLiteral("Click to open"));
        hint->setStyleSheet("color: #4a4a4a; background: transparent;");
        details->addWidget(hint);
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
    auto *devices = new Aero7StorageDevices(this);
    connect(devices, &Aero7StorageDevices::changed, this, &Aero7ComputerView::refresh);
    connect(devices, &Aero7StorageDevices::opened, this, &Aero7ComputerView::openRequested);
    connect(devices, &Aero7StorageDevices::failed, this, [this](const QString &message) {
        QMessageBox::warning(this, QStringLiteral("Open drive"), message);
    });
    refresh();
    new Aero7Storage::MountWatcher(this, [this] { refresh(); });
}

bool Aero7ComputerView::isUserVisibleStorage(const QStorageInfo &storage)
{
    return Aero7Storage::visible(storage);
}

void Aero7ComputerView::refresh()
{
    auto *scroll = findChild<QScrollArea *>();
    const int scrollPosition = scroll->verticalScrollBar()->value();
    const QWidget *focused = QApplication::focusWidget();
    const QString focusedRoot = focused && isAncestorOf(focused)
        ? focused->property("storageRoot").toString() : QString();
    const QString focusedId = focused && isAncestorOf(focused)
        ? focused->property("deviceId").toString() : QString();
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

    struct Drive { Aero7Storage::Entry entry; QString id; };
    QList<Drive> hard;
    QList<Drive> removable;
    for (const auto &entry : Aero7Storage::mounted()) {
        (entry.root == QLatin1String("/") || Aero7Storage::isManagedStartupMount(entry.root)
            ? hard : removable).append({entry, {}});
    }
    auto *devices = findChild<Aero7StorageDevices *>();
    for (const auto &device : devices->unmounted()) {
        Aero7Storage::Entry entry;
        entry.name = device.name;
        entry.icon = device.icon;
        removable.append({entry, device.id});
    }

    auto addGroup = [this, layout, devices](const QString &title,
                                  const QList<Drive> &volumes) {
        layout->addWidget(sectionTitle(title, volumes.size()));
        auto *gridWidget = new QWidget;
        auto *grid = new QGridLayout(gridWidget);
        grid->setContentsMargins(0, 0, 8, 4);
        grid->setHorizontalSpacing(28);
        grid->setVerticalSpacing(2);
        for (int index = 0; index < volumes.size(); ++index) {
            const auto &entry = volumes.at(index).entry;
            const QString id = volumes.at(index).id;
            grid->addWidget(driveTile(entry.storage, entry.name, entry.icon, id,
                                      this, [this, devices, id, root = entry.root]() {
                                          if (!id.isEmpty()) devices->openDevice(id);
                                          else {
                                              devices->cancelPending();
                                              Q_EMIT openRequested(root);
                                          }
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

    addGroup(QStringLiteral("Hard Disk Drives"), hard);
    if (!removable.isEmpty())
        addGroup(QStringLiteral("Devices with Removable Storage"), removable);
    layout->addStretch(1);
    if (!focusedRoot.isEmpty() || !focusedId.isEmpty()) {
        bool restored = false;
        for (auto *button : m_content->findChildren<QPushButton *>(QStringLiteral("computerDrive"))) {
            if (button->property("storageRoot").toString() == focusedRoot
                && button->property("deviceId").toString() == focusedId) {
                button->setFocus(Qt::OtherFocusReason);
                restored = true;
                break;
            }
        }
        if (!restored) scroll->setFocus(Qt::OtherFocusReason);
    }
    // Restore after the replacement layout has updated the scroll range.
    QTimer::singleShot(0, this, [scroll, scrollPosition] {
        scroll->verticalScrollBar()->setValue(scrollPosition);
    });
}
