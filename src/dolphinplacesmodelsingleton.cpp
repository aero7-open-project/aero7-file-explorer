/*
 * SPDX-FileCopyrightText: 2018 Kai Uwe Broulik <kde@privat.broulik.de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "dolphinplacesmodelsingleton.h"
#include "trash/dolphintrash.h"
#include "views/draganddrophelper.h"
#include "aero7libraries.h"
#include "aero7storage.h"
#include "aero7/aero7devicevisibility.h"

#include <Solid/StorageAccess>
#include <Solid/StorageDrive>
#include <Solid/StorageVolume>

#include <KAboutData>

#include <QCoreApplication>
#include <QIcon>
#include <QDir>
#include <QFileInfo>
#include <QMimeData>
#include <QSettings>
#include <QSet>
#include <QStandardPaths>
#include <QTimer>
#include "aero7/aero7mountwatcher.h"

namespace {
QString specialPlacePath(const QString &name)
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
        .filePath(QStringLiteral("Aero7/Shell Places/%1").arg(name));
}

QString favoritesSettingsPath()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation))
        .filePath(QStringLiteral("aero7-file-explorer-favorites.ini"));
}
}

QVector<DolphinPlacesModel::Favorite> DolphinPlacesModel::defaultFavorites()
{
    return {
        {QStringLiteral("Recent Places"), QUrl::fromLocalFile(specialPlacePath(QStringLiteral("Recent Places"))),
         QStringLiteral("document-open-recent")},
        {QStringLiteral("Desktop"), QUrl::fromLocalFile(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)),
         QStringLiteral("user-desktop")},
        {QStringLiteral("Downloads"), QUrl::fromLocalFile(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)),
         QStringLiteral("folder-download")},
    };
}

void DolphinPlacesModel::saveFavorites() const
{
    QSettings settings(favoritesSettingsPath(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("Configured"), true);
    settings.beginWriteArray(QStringLiteral("Favorites"));
    for (int i = 0; i < m_favorites.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue(QStringLiteral("Name"), m_favorites.at(i).name);
        settings.setValue(QStringLiteral("Url"), m_favorites.at(i).url.toString());
        settings.setValue(QStringLiteral("Icon"), m_favorites.at(i).icon);
    }
    settings.endArray();
    settings.sync();
}

QModelIndex DolphinPlacesModel::favoriteIndex(const QUrl &favoriteUrl) const
{
    for (int row = 0; row < rowCount(); ++row) {
        const QModelIndex item = index(row, 0);
        if (url(item).matches(favoriteUrl, QUrl::StripTrailingSlash)) return item;
    }
    return {};
}

bool DolphinPlacesModel::isFavorite(const QUrl &favoriteUrl) const
{
    for (const Favorite &favorite : m_favorites)
        if (favorite.url.matches(favoriteUrl, QUrl::StripTrailingSlash)) return true;
    return false;
}

void DolphinPlacesModel::ensureFavorites()
{
    for (const Favorite &favorite : std::as_const(m_favorites)) {
        QModelIndex item = favoriteIndex(favorite.url);
        if (!item.isValid()) {
            addPlace(favorite.name, favorite.url, favorite.icon);
            item = favoriteIndex(favorite.url);
        } else if (text(item) != favorite.name) {
            editPlace(item, favorite.name, favorite.url, favorite.icon);
        }
        if (item.isValid() && isHidden(item)) setPlaceHidden(item, false);
    }
}

bool DolphinPlacesModel::addFavorite(const QUrl &favoriteUrl)
{
    if (!favoriteUrl.isLocalFile() || !QFileInfo(favoriteUrl.toLocalFile()).isDir()
        || isFavorite(favoriteUrl)) return false;
    const QString name = QFileInfo(favoriteUrl.toLocalFile()).fileName();
    m_favorites.append({name.isEmpty() ? favoriteUrl.toLocalFile() : name,
                        favoriteUrl, QStringLiteral("folder-open")});
    saveFavorites();
    ensureFavorites();
    return true;
}

bool DolphinPlacesModel::removeFavorite(const QUrl &favoriteUrl)
{
    for (int i = 0; i < m_favorites.size(); ++i) {
        if (!m_favorites.at(i).url.matches(favoriteUrl, QUrl::StripTrailingSlash)) continue;
        const QModelIndex item = favoriteIndex(favoriteUrl);
        const bool builtIn = [&]() {
            for (const Favorite &entry : defaultFavorites())
                if (entry.url.matches(favoriteUrl, QUrl::StripTrailingSlash)) return true;
            return false;
        }();
        if (item.isValid()) {
            if (builtIn) setPlaceHidden(item, true);
            else removePlace(item);
        }
        m_favorites.removeAt(i);
        saveFavorites();
        return true;
    }
    return false;
}

bool DolphinPlacesModel::renameFavorite(const QUrl &favoriteUrl, const QString &name)
{
    const QString label = name.trimmed();
    if (label.isEmpty()) return false;
    for (Favorite &favorite : m_favorites) {
        if (!favorite.url.matches(favoriteUrl, QUrl::StripTrailingSlash)) continue;
        favorite.name = label;
        if (const QModelIndex item = favoriteIndex(favoriteUrl); item.isValid())
            editPlace(item, label, favorite.url, favorite.icon);
        saveFavorites();
        return true;
    }
    return false;
}

void DolphinPlacesModel::restoreFavorites()
{
    const QVector<Favorite> defaults = defaultFavorites();
    for (const Favorite &favorite : std::as_const(m_favorites)) {
        bool builtIn = false;
        for (const Favorite &entry : defaults)
            if (entry.url.matches(favorite.url, QUrl::StripTrailingSlash)) builtIn = true;
        if (!builtIn) {
            if (const QModelIndex item = favoriteIndex(favorite.url); item.isValid()) removePlace(item);
        }
    }
    m_favorites = defaults;
    saveFavorites();
    ensureFavorites();
}

DolphinPlacesModel::DolphinPlacesModel(QObject *parent)
    : KFilePlacesModel(parent)
{
    QSettings favoriteSettings(favoritesSettingsPath(), QSettings::IniFormat);
    if (favoriteSettings.value(QStringLiteral("Configured"), false).toBool()) {
        const int count = favoriteSettings.beginReadArray(QStringLiteral("Favorites"));
        for (int i = 0; i < count; ++i) {
            favoriteSettings.setArrayIndex(i);
            const QUrl url(favoriteSettings.value(QStringLiteral("Url")).toString());
            if (url.isValid() && url.isLocalFile())
                m_favorites.append({favoriteSettings.value(QStringLiteral("Name")).toString(), url,
                                    favoriteSettings.value(QStringLiteral("Icon"), QStringLiteral("folder-open")).toString()});
        }
        favoriteSettings.endArray();
    } else {
        m_favorites = defaultFavorites();
    }
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
    // Remove the old synthetic bookmark below, but never delete its directory:
    // users may have placed real files there in an earlier version.
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

    ensureFavorites();

    setGroupHidden(KFilePlacesModel::RecentlySavedType, true);
    setGroupHidden(KFilePlacesModel::SearchForType, true);
    setGroupHidden(KFilePlacesModel::DevicesType, false);
    setGroupHidden(KFilePlacesModel::RemovableDevicesType, false);
    setGroupHidden(KFilePlacesModel::RemoteType, true);
    QSet<QString> visible;
    for (const Favorite &favorite : std::as_const(m_favorites))
        visible.insert(favorite.url.toString());
    visible.insert(QUrl::fromLocalFile(computerPath).toString());
    visible.insert(QUrl::fromLocalFile(localDiskPath).toString());
    visible.insert(QUrl::fromLocalFile(networkPath).toString());
    for (const Aero7Library &library : Aero7Libraries::instance().libraries())
        if (library.shownInNavigationPane)
            visible.insert(QUrl::fromLocalFile(
                Aero7Libraries::instance().materializedPath(library.id)).toString());
    for (int row = 0; row < rowCount(); ++row) {
        const QModelIndex item = index(row, 0);
        setPlaceHidden(item, !visible.contains(url(item).toString()));
    }
    refreshStoragePlaces();
    // Native KIO device entries retain mount/eject actions. Re-evaluate their
    // visibility after mount changes, without persisting synthetic drive links.
    new Aero7Storage::MountWatcher(this, [this] { refreshStoragePlaces(); });
    connect(this, &QAbstractItemModel::rowsInserted, this, [this] {
        QTimer::singleShot(0, this, &DolphinPlacesModel::refreshStoragePlaces);
    });
    // Solid may update a device URL after the kernel mount notification. Retry
    // on the native model update as well, retaining its real mount/eject entry.
    connect(this, &QAbstractItemModel::dataChanged, this, [this] {
        QTimer::singleShot(0, this, &DolphinPlacesModel::refreshStoragePlaces);
    });
    connect(this, &QAbstractItemModel::modelReset, this, [this] {
        QTimer::singleShot(0, this, &DolphinPlacesModel::refreshStoragePlaces);
    });
}

DolphinPlacesModel::~DolphinPlacesModel() = default;

void DolphinPlacesModel::refreshStoragePlaces()
{
    QHash<QString, QString> names;
    for (const auto &entry : Aero7Storage::mounted())
        if (entry.root != QLatin1String("/")) names.insert(entry.root, entry.name);
    const bool namesChanged = names != m_storageNames;
    m_storageNames = names;
    for (int row = 0; row < rowCount(); ++row) {
        const QModelIndex item = index(row, 0);
        if (!isDevice(item)) continue;
        const QUrl deviceUrl = url(item);
        const bool visibleMount = deviceUrl.isLocalFile()
            && names.contains(QDir::cleanPath(deviceUrl.toLocalFile()));
        const Solid::Device device = deviceForIndex(item);
        const auto *access = device.as<Solid::StorageAccess>();
        const auto *volume = device.as<Solid::StorageVolume>();
        bool removable = false;
        // Partition devices may have more than one parent before the drive.
        // Bound the traversal in case a backend reports malformed ancestry.
        Solid::Device ancestor = device;
        for (int depth = 0; ancestor.isValid() && depth < 16; ++depth) {
            if (const auto *drive = ancestor.as<Solid::StorageDrive>()) {
                removable = drive->isRemovable() || drive->isHotpluggable();
                break;
            }
            ancestor = ancestor.parent();
        }
        const bool ignored = Aero7Storage::deviceIgnored(access, access && access->isAccessible(),
            access && access->isIgnored(), volume, volume && volume->isIgnored());
        const bool filesystem = volume && volume->usage() == Solid::StorageVolume::FileSystem;
        const bool hidden = !Aero7Storage::deviceVisible(access && access->isAccessible(),
                                                       visibleMount, ignored, filesystem, removable);
        if (isHidden(item) != hidden) setPlaceHidden(item, hidden);
    }
    if (namesChanged && rowCount() > 0)
        Q_EMIT dataChanged(index(0, 0), index(rowCount() - 1, 0),
                           {Qt::DisplayRole, KFilePlacesModel::GroupRole});
}

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
    case Qt::DisplayRole:
        if (isDevice(index) && url(index).isLocalFile()) {
            const auto name = m_storageNames.constFind(QDir::cleanPath(url(index).toLocalFile()));
            if (name != m_storageNames.cend()) return *name;
        }
        break;
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
            && itemUrl.toLocalFile() == specialPlacePath(QStringLiteral("Local Disk (C:)")))
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
    // KIO jobs owned by this model hold event-loop locks. Release them while
    // QCoreApplication still exists, not from the process-static destructor.
    qAddPostRoutine([]() {
        DolphinPlacesModelSingleton::instance().m_placesModel.reset();
    });
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
