/*
 * SPDX-FileCopyrightText: 2018 Kai Uwe Broulik <kde@privat.broulik.de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "dolphinplacesmodelsingleton.h"
#include "trash/dolphintrash.h"
#include "views/draganddrophelper.h"
#include "aero7libraries.h"

#include <KAboutData>

#include <QIcon>
#include <QDir>
#include <QMimeData>
#include <QSet>
#include <QStandardPaths>

namespace {
QString specialPlacePath(const QString &name)
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
        .filePath(QStringLiteral("Aero7/Shell Places/%1").arg(name));
}
}

DolphinPlacesModel::DolphinPlacesModel(QObject *parent)
    : KFilePlacesModel(parent)
{
    connect(&Trash::instance(), &Trash::emptinessChanged, this, &DolphinPlacesModel::slotTrashEmptinessChanged);

    // Seed the Windows 7-style navigation locations once. KFilePlacesModel
    // persists these entries in the user's normal places file, while the
    // Library definitions themselves remain owned by Aero7Libraries.
    const auto ensurePlace = [this](const QString &name, const QUrl &url,
                                    const QString &icon) {
        for (int row = 0; row < rowCount(); ++row) {
            if (this->url(index(row, 0)).matches(url, QUrl::StripTrailingSlash))
                return;
        }
        addPlace(name, url, icon);
    };
    ensurePlace(QStringLiteral("Desktop"),
                QUrl::fromLocalFile(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)),
                QStringLiteral("user-desktop"));
    ensurePlace(QStringLiteral("Downloads"),
                QUrl::fromLocalFile(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)),
                QStringLiteral("folder-download"));
    const QString recentPath = specialPlacePath(QStringLiteral("Recent Places"));
    const QString computerPath = specialPlacePath(QStringLiteral("Computer"));
    const QString localDiskPath = specialPlacePath(QStringLiteral("Local Disk (C:)"));
    const QString cdDrivePath = specialPlacePath(QStringLiteral("CD Drive (D:)"));
    const QString networkPath = specialPlacePath(QStringLiteral("Network"));
    QDir().mkpath(recentPath);
    QDir().mkpath(computerPath);
    QDir().mkpath(localDiskPath);
    QDir().mkpath(cdDrivePath);
    QDir().mkpath(networkPath);
    QSet<QString> managed;
    managed.insert(QStringLiteral("aero7recent:/"));
    managed.insert(QStringLiteral("aero7network:/"));
    managed.insert(QStringLiteral("aero7computer:/"));
    managed.insert(QUrl::fromLocalFile(recentPath).toString());
    managed.insert(QUrl::fromLocalFile(computerPath).toString());
    managed.insert(QUrl::fromLocalFile(localDiskPath).toString());
    managed.insert(QUrl::fromLocalFile(cdDrivePath).toString());
    managed.insert(QUrl::fromLocalFile(networkPath).toString());
    for (const Aero7Library &library : Aero7Libraries::instance().libraries())
        managed.insert(QUrl::fromLocalFile(
            Aero7Libraries::instance().materializedPath(library.id)).toString());
    for (int row = rowCount() - 1; row >= 0; --row)
        if (managed.contains(url(index(row, 0)).toString()))
            removePlace(index(row, 0));

    QModelIndex after;
    const QUrl downloadsUrl = QUrl::fromLocalFile(
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
    for (int row = 0; row < rowCount(); ++row)
        if (url(index(row, 0)).matches(downloadsUrl, QUrl::StripTrailingSlash))
            after = index(row, 0);
    const auto addManaged = [this, &after](const QString &name, const QUrl &url,
                                           const QString &icon) {
        addPlace(name, url, icon, QString(), after);
        for (int row = 0; row < rowCount(); ++row)
            if (this->url(index(row, 0)).matches(url, QUrl::StripTrailingSlash)) {
                after = index(row, 0);
                break;
            }
    };
    addManaged(QStringLiteral("Recent Places"), QUrl::fromLocalFile(recentPath),
               QStringLiteral("document-open-recent"));
    for (const Aero7Library &library : Aero7Libraries::instance().libraries()) {
        if (library.shownInNavigationPane)
            addManaged(library.name,
                       QUrl::fromLocalFile(Aero7Libraries::instance().materializedPath(library.id)),
                       library.id == QLatin1String("new-library")
                           ? QStringLiteral("folder-library")
                           : QStringLiteral("folder-%1").arg(library.id));
    }
    addManaged(QStringLiteral("Computer"), QUrl::fromLocalFile(computerPath),
               QStringLiteral("computer"));
    addManaged(QStringLiteral("Local Disk (C:)"), QUrl::fromLocalFile(localDiskPath),
               QStringLiteral("drive-harddisk-root"));
    addManaged(QStringLiteral("CD Drive (D:)"), QUrl::fromLocalFile(cdDrivePath),
               QStringLiteral("drive-optical"));
    addManaged(QStringLiteral("Network"), QUrl::fromLocalFile(networkPath),
               QStringLiteral("network-workgroup"));

    // The supplied Windows 7 reference orders Favorites as Recent Places,
    // Desktop, Downloads. Keep that order stable even when an existing KDE
    // places file originally seeded Desktop and Downloads first.
    int recentRow = -1;
    int desktopRow = -1;
    const QUrl desktopUrl = QUrl::fromLocalFile(
        QStandardPaths::writableLocation(QStandardPaths::DesktopLocation));
    for (int row = 0; row < rowCount(); ++row) {
        const QUrl rowUrl = url(index(row, 0));
        if (rowUrl == QUrl::fromLocalFile(recentPath))
            recentRow = row;
        else if (rowUrl.matches(desktopUrl, QUrl::StripTrailingSlash))
            desktopRow = row;
    }
    if (recentRow >= 0 && desktopRow >= 0 && recentRow > desktopRow)
        movePlace(recentRow, desktopRow);

    setGroupHidden(KFilePlacesModel::RecentlySavedType, true);
    setGroupHidden(KFilePlacesModel::SearchForType, true);
    setGroupHidden(KFilePlacesModel::DevicesType, true);
    setGroupHidden(KFilePlacesModel::RemovableDevicesType, true);
    setGroupHidden(KFilePlacesModel::RemoteType, true);
    QSet<QString> visible;
    visible.insert(QUrl::fromLocalFile(QStandardPaths::writableLocation(
                       QStandardPaths::DesktopLocation)).toString());
    visible.insert(QUrl::fromLocalFile(QStandardPaths::writableLocation(
                       QStandardPaths::DownloadLocation)).toString());
    visible.insert(QUrl::fromLocalFile(recentPath).toString());
    visible.insert(QUrl::fromLocalFile(computerPath).toString());
    visible.insert(QUrl::fromLocalFile(localDiskPath).toString());
    visible.insert(QUrl::fromLocalFile(cdDrivePath).toString());
    visible.insert(QUrl::fromLocalFile(networkPath).toString());
    for (const Aero7Library &library : Aero7Libraries::instance().libraries())
        if (library.shownInNavigationPane)
            visible.insert(QUrl::fromLocalFile(
                Aero7Libraries::instance().materializedPath(library.id)).toString());
    for (int row = 0; row < rowCount(); ++row) {
        const QModelIndex item = index(row, 0);
        setPlaceHidden(item, !visible.contains(url(item).toString()));
    }
}

DolphinPlacesModel::~DolphinPlacesModel() = default;

bool DolphinPlacesModel::panelsLocked() const
{
    return m_panelsLocked;
}

void DolphinPlacesModel::setPanelsLocked(bool locked)
{
    if (m_panelsLocked == locked) {
        return;
    }

    m_panelsLocked = locked;

    if (rowCount() > 0) {
        int lastPlace = rowCount() - 1;

        for (int i = 0; i < rowCount(); ++i) {
            if (KFilePlacesModel::groupType(index(i, 0)) != KFilePlacesModel::PlacesType) {
                lastPlace = i - 1;
                break;
            }
        }

        Q_EMIT dataChanged(index(0, 0), index(lastPlace, 0), {KFilePlacesModel::GroupRole});
    }
}

QStringList DolphinPlacesModel::mimeTypes() const
{
    QStringList types = KFilePlacesModel::mimeTypes();
    types << DragAndDropHelper::arkDndServiceMimeType() << DragAndDropHelper::arkDndPathMimeType();
    return types;
}

bool DolphinPlacesModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent)
{
    // We make the view accept the drag by returning them from mimeTypes()
    // but the drop should be handled exclusively by PlacesPanel::slotUrlsDropped
    if (DragAndDropHelper::isArkDndMimeType(data)) {
        return false;
    }

    return KFilePlacesModel::dropMimeData(data, action, row, column, parent);
}

QVariant DolphinPlacesModel::data(const QModelIndex &index, int role) const
{
    switch (role) {
    case Qt::DecorationRole:
        if (url(index).isLocalFile()
            && url(index).toLocalFile() == specialPlacePath(QStringLiteral("Local Disk (C:)"))) {
            // Existing profiles may still have the pre-fork generic disk icon
            // persisted in user-places.xbel. Always expose the Windows system
            // drive overlay from the active Aero7 icon theme.
            return QIcon::fromTheme(QStringLiteral("drive-harddisk-root"));
        }
        if (isTrash(index)) {
            if (m_isEmpty) {
                return QIcon::fromTheme(QStringLiteral("user-trash"));
            } else {
                return QIcon::fromTheme(QStringLiteral("user-trash-full"));
            }
        }
        break;
    case KFilePlacesModel::GroupRole: {
        if (isHidden(index))
            return QString();
        const QUrl itemUrl = url(index);
        if (itemUrl.isLocalFile()
            && itemUrl.toLocalFile() == specialPlacePath(QStringLiteral("Computer")))
            return QStringLiteral("Computer");
        if (itemUrl.isLocalFile()
            && (itemUrl.toLocalFile() == specialPlacePath(QStringLiteral("Local Disk (C:)"))
                || itemUrl.toLocalFile() == specialPlacePath(QStringLiteral("CD Drive (D:)"))))
            return QStringLiteral("Computer");
        if (itemUrl.isLocalFile()
            && itemUrl.toLocalFile() == specialPlacePath(QStringLiteral("Network")))
            return QStringLiteral("Network");
        if (itemUrl.isLocalFile()
            && itemUrl.toLocalFile() == specialPlacePath(QStringLiteral("Recent Places")))
            return QStringLiteral("Favorites");
        if (itemUrl.isLocalFile()
            && !Aero7Libraries::instance().libraryIdForPath(itemUrl.toLocalFile()).isEmpty()) {
            return QStringLiteral("Libraries");
        }
        switch (KFilePlacesModel::groupType(index)) {
        case KFilePlacesModel::PlacesType:
        case KFilePlacesModel::RecentlySavedType:
        case KFilePlacesModel::SearchForType:
            return QStringLiteral("Favorites");
        case KFilePlacesModel::DevicesType:
        case KFilePlacesModel::RemovableDevicesType:
            return QStringLiteral("Computer");
        case KFilePlacesModel::RemoteType:
            return QStringLiteral("Network");
        default:
            break;
        }
        // When panels are unlocked, avoid a double "Places" heading,
        // one from the panel title bar, one from the places view section.
        if (!m_panelsLocked) {
            const auto groupType = KFilePlacesModel::groupType(index);
            if (groupType == KFilePlacesModel::PlacesType) {
                return QString();
            }
        }
        break;
    }
    }

    return KFilePlacesModel::data(index, role);
}

void DolphinPlacesModel::slotTrashEmptinessChanged(bool isEmpty)
{
    if (m_isEmpty == isEmpty) {
        return;
    }

    // NOTE Trash::isEmpty() reads the config file whereas emptinessChanged is
    // hooked up to whether a dirlister in trash:/ has any files and they disagree...
    m_isEmpty = isEmpty;

    for (int i = 0; i < rowCount(); ++i) {
        const QModelIndex index = this->index(i, 0);
        if (isTrash(index)) {
            Q_EMIT dataChanged(index, index, {Qt::DecorationRole});
        }
    }
}

bool DolphinPlacesModel::isTrash(const QModelIndex &index) const
{
    return url(index) == QUrl(QStringLiteral("trash:/"));
}

DolphinPlacesModelSingleton::DolphinPlacesModelSingleton()
    : m_placesModel(new DolphinPlacesModel())
{
}

DolphinPlacesModelSingleton &DolphinPlacesModelSingleton::instance()
{
    static DolphinPlacesModelSingleton s_self;
    return s_self;
}

DolphinPlacesModel *DolphinPlacesModelSingleton::placesModel() const
{
    return m_placesModel.data();
}

#include "moc_dolphinplacesmodelsingleton.cpp"
