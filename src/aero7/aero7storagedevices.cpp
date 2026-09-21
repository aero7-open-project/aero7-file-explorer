/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aero7storagedevices.h"
#include "aero7storage.h"
#include "aero7devicevisibility.h"

#include <KFilePlacesModel>
#include <QEvent>
#include <QFileInfo>
#include <QPersistentModelIndex>
#include <QSet>
#include <QTimer>
#include <Solid/StorageAccess>
#include <Solid/StorageDrive>
#include <Solid/StorageVolume>

Aero7StorageDevices::Aero7StorageDevices(QObject *parent)
    : QObject(parent), m_places(new KFilePlacesModel(this))
{
    setObjectName(QStringLiteral("aero7StorageDevices"));
    parent->installEventFilter(this);
    // Coalesce native model notifications; do not change a menu/tree while
    // its activation callback or the native model's mutation is on the stack.
    auto *refresh = new QTimer(this);
    refresh->setSingleShot(true);
    refresh->setInterval(0);
    connect(refresh, &QTimer::timeout, this, &Aero7StorageDevices::changed);
    const auto schedule = [refresh] { refresh->start(); };
    connect(m_places, &QAbstractItemModel::rowsInserted, this, schedule);
    connect(m_places, &QAbstractItemModel::rowsRemoved, this, schedule);
    connect(m_places, &QAbstractItemModel::dataChanged, this, schedule);
    connect(m_places, &QAbstractItemModel::modelReset, this, schedule);
}

bool Aero7StorageDevices::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == parent() && event->type() == QEvent::Hide) cancelPending();
    return QObject::eventFilter(watched, event);
}

QList<Aero7StorageDevices::Device> Aero7StorageDevices::unmounted() const
{
    QList<Device> result;
    QSet<QString> seen;
    for (int row = 0; row < m_places->rowCount(); ++row) {
        const auto index = m_places->index(row, 0);
        if (!m_places->isDevice(index)) continue;
        const auto device = m_places->deviceForIndex(index);
        const auto *access = device.as<Solid::StorageAccess>();
        const auto *volume = device.as<Solid::StorageVolume>();
        if (!access || access->isAccessible()) continue;
        bool removable = false;
        auto ancestor = device;
        for (int depth = 0; ancestor.isValid() && depth < 16; ++depth) {
            if (const auto *drive = ancestor.as<Solid::StorageDrive>()) {
                removable = drive->isRemovable() || drive->isHotpluggable();
                break;
            }
            ancestor = ancestor.parent();
        }
        const bool ignored = Aero7Storage::deviceIgnored(true, false, access->isIgnored(),
                                                        volume, volume && volume->isIgnored());
        if (!Aero7Storage::deviceVisible(false, false, ignored,
                volume && volume->usage() == Solid::StorageVolume::FileSystem, removable)) continue;
        if (device.udi().isEmpty() || seen.contains(device.udi())) continue;
        seen.insert(device.udi());
        QString name = m_places->text(index).trimmed();
        if (name.isEmpty() || name.contains(QLatin1Char('/'))) name = QStringLiteral("Removable Disk");
        result.append({device.udi(), name, QStringLiteral("drive-removable-media")});
    }
    return result;
}

void Aero7StorageDevices::cancelPending()
{
    if (!m_pending) return;
    // Cancel only delayed navigation; never force-cancel a filesystem setup
    // already being performed by UDisks or disable its authorization checks.
    disconnect(m_places, nullptr, m_pending, nullptr);
    m_pending->deleteLater();
    m_pending = nullptr;
}

void Aero7StorageDevices::openDevice(const QString &id)
{
    if (m_pending && m_pending->property("deviceId").toString() == id) return;
    cancelPending();
    QPersistentModelIndex target;
    for (int row = 0; row < m_places->rowCount(); ++row) {
        const auto index = m_places->index(row, 0);
        if (m_places->isDevice(index) && m_places->deviceForIndex(index).udi() == id) {
            target = index;
            break;
        }
    }
    if (!target.isValid() || id.isEmpty()) {
        Q_EMIT failed(QStringLiteral("This drive is no longer available."));
        return;
    }
    auto *access = m_places->deviceForIndex(target).as<Solid::StorageAccess>();
    const auto openMounted = [this, target] {
        const QUrl url = target.isValid() ? m_places->url(target) : QUrl();
        const auto device = target.isValid() ? m_places->deviceForIndex(target) : Solid::Device();
        const auto *access = device.as<Solid::StorageAccess>();
        const auto *volume = device.as<Solid::StorageVolume>();
        if (access && access->isAccessible() && !access->isIgnored()
            && (!volume || !volume->isIgnored())
            && url.isLocalFile() && QFileInfo(url.toLocalFile()).isDir()
            && url.toLocalFile() != QLatin1String("/")
            && Aero7Storage::visibleRoot(url.toLocalFile(), qEnvironmentVariable("USER")))
            Q_EMIT opened(url.toLocalFile());
        else Q_EMIT failed(QStringLiteral("This drive is no longer available."));
    };
    if (access && access->isAccessible()) { openMounted(); return; }
    const auto candidates = unmounted();
    if (!std::any_of(candidates.cbegin(), candidates.cend(), [&id](const auto &device) { return device.id == id; })) {
        Q_EMIT failed(QStringLiteral("This drive cannot be opened."));
        return;
    }
    auto *request = new QObject(this);
    request->setProperty("deviceId", id);
    m_pending = request;
    connect(m_places, &KFilePlacesModel::setupDone, request,
        [this, request, target, openMounted](const QModelIndex &finished, bool success) {
            if (m_pending != request || !target.isValid() || finished != target) return;
            cancelPending();
            if (success) openMounted();
            else Q_EMIT failed(QStringLiteral("The drive could not be opened. It may have been removed or access was denied."));
        });
    connect(this, &Aero7StorageDevices::changed, request, [this, request, target] {
        if (m_pending == request && !target.isValid()) {
            cancelPending();
            Q_EMIT failed(QStringLiteral("The drive was removed before it could be opened."));
        }
    });
    QTimer::singleShot(30000, request, [this, request] {
        if (m_pending != request) return;
        cancelPending();
        Q_EMIT failed(QStringLiteral("The drive did not become available. Please try again."));
    });
    m_places->requestSetup(target);
}
