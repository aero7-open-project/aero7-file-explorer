/*
 * SPDX-FileCopyrightText: 2008-2012 Peter Penz <peter.penz19@gmail.com>
 * SPDX-FileCopyrightText: 2021 Kai Uwe Broulik <kde@broulik.de>
 *
 * Based on KFilePlacesView from kdelibs:
 * SPDX-FileCopyrightText: 2007 Kevin Ottens <ervin@kde.org>
 * SPDX-FileCopyrightText: 2007 David Faure <faure@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "placespanel.h"
#include "aero7icons.h"

#include "dolphin_generalsettings.h"
#include "dolphin_placespanelsettings.h"
#include "dolphinplacesmodelsingleton.h"
#include "settings/dolphinsettingsdialog.h"
#include "views/draganddrophelper.h"
#include "aero7properties.h"
#include "aero7libraries.h"

#include <KFilePlacesModel>
#include <KIO/DropJob>
#include <KIO/Job>
#include <KLocalizedString>
#include <KProtocolManager>

#include <QIcon>
#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QInputDialog>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QShowEvent>
#include <QTimer>

#include <Solid/StorageAccess>

PlacesPanel::PlacesPanel(QWidget *parent)
    : KFilePlacesView(parent)
{
    setDropOnPlaceEnabled(true);
    connect(this, &PlacesPanel::urlsDropped, this, &PlacesPanel::slotUrlsDropped);

    setAutoResizeItemsEnabled(false);

    setTeardownFunction([this](const QModelIndex &index) {
        slotTearDownRequested(index);
    });

    m_openInSplitView = std::make_unique<QAction>(Aero7Icons::icon(QStringLiteral("view-split-left-right")), i18nc("@action:inmenu", "Open in Split View"));
    m_openInSplitView->setPriority(QAction::HighPriority);
    connect(m_openInSplitView.get(), &QAction::triggered, this, [this]() {
        const QUrl url = currentIndex().data(KFilePlacesModel::UrlRole).toUrl();
        Q_EMIT openInSplitViewRequested(url);
    });
    addAction(m_openInSplitView.get());

    m_configureTrashAction = std::make_unique<QAction>(Aero7Icons::icon(QStringLiteral("configure")), i18nc("@action:inmenu", "Configure Trash…"));
    m_configureTrashAction->setPriority(QAction::HighPriority);
    connect(m_configureTrashAction.get(), &QAction::triggered, this, &PlacesPanel::slotConfigureTrash);
    addAction(m_configureTrashAction.get());

    connect(this, &PlacesPanel::contextMenuAboutToShow, this, &PlacesPanel::slotContextMenuAboutToShow);

    connect(this, &PlacesPanel::iconSizeChanged, this, [](const QSize &newSize) {
        int iconSize = qMin(newSize.width(), newSize.height());
        if (iconSize == 0) {
            // Don't store 0 size, let's keep -1 for default/small/automatic
            iconSize = -1;
        }
        PlacesPanelSettings *settings = PlacesPanelSettings::self();
        settings->setIconSize(iconSize);
        settings->save();
    });

    readSettings();
    setIconSize(QSize(16, 16));
    setSpacing(0);
    setStyleSheet(QStringLiteral(R"(
        KFilePlacesView::item {
            min-height: 20px;
            padding: 0;
        }
        KFilePlacesView::item:selected {
            color: #111111;
            background: #dcecf9;
            border: 1px solid #7da2ce;
        }
    )"));
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Set the model here so that it's loaded in time for the sizeHint to properly apply (setting it upon showEvent is too late)
    auto *placesModel = DolphinPlacesModelSingleton::instance().placesModel();
    setModel(placesModel);

    connect(placesModel, &KFilePlacesModel::errorMessage, this, &PlacesPanel::errorMessage);
    connect(placesModel, &KFilePlacesModel::teardownDone, this, &PlacesPanel::slotTearDownDone);

    connect(placesModel, &QAbstractItemModel::rowsInserted, this, &PlacesPanel::slotRowsInserted);
    connect(placesModel, &QAbstractItemModel::rowsAboutToBeRemoved, this, &PlacesPanel::slotRowsAboutToBeRemoved);

    for (int i = 0; i < model()->rowCount(); ++i) {
        connectDeviceSignals(model()->index(i, 0, QModelIndex()));
    }
}

PlacesPanel::~PlacesPanel() = default;

void PlacesPanel::setUrl(const QUrl &url)
{
    m_currentFolderUrl = url;
    QUrl navigationUrl = url;
    if (url.isLocalFile()) {
        const QString candidate = QDir::cleanPath(QFileInfo(url.toLocalFile()).absoluteFilePath());
        bool matchedVisiblePlace = false;
        const auto *placesModel = DolphinPlacesModelSingleton::instance().placesModel();
        for (int row = 0; row < placesModel->rowCount(); ++row) {
            const QModelIndex index = placesModel->index(row, 0);
            if (placesModel->isHidden(index))
                continue;
            const QUrl placeUrl = placesModel->url(index);
            if (!placeUrl.isLocalFile())
                continue;
            const QString placePath = QDir::cleanPath(placeUrl.toLocalFile());
            if (candidate == placePath || candidate.startsWith(placePath + QDir::separator())) {
                navigationUrl = placeUrl;
                matchedVisiblePlace = true;
                break;
            }
        }
        if (!matchedVisiblePlace) {
            for (const Aero7Library &library : Aero7Libraries::instance().libraries()) {
                for (const QString &location : library.locations) {
                    const QString root = QDir::cleanPath(QFileInfo(location).absoluteFilePath());
                    if (candidate == root || candidate.startsWith(root + QDir::separator())) {
                        navigationUrl = QUrl::fromLocalFile(
                            Aero7Libraries::instance().materializedPath(library.id));
                        matchedVisiblePlace = true;
                        break;
                    }
                }
                if (matchedVisiblePlace)
                    break;
            }
        }
        // Ordinary folders belong to Computer's visible disk entry. An empty
        // URL leaves no keyboard-activatable place; a raw mount exposes hidden
        // KDE device headings instead of the Aero7 navigation tree.
        if (!matchedVisiblePlace)
            navigationUrl = QUrl::fromLocalFile(QDir(
                QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
                .filePath(QStringLiteral("Aero7/Shell Places/Local Disk (C:)")));
    }
    KFilePlacesView::setUrl(navigationUrl);
    // Native KFilePlacesView invalidates its own row geometry. Aero7 paints
    // a different grouped layout, so refresh the complete selection surface.
    viewport()->update();
}

QList<QAction *> PlacesPanel::customContextMenuActions() const
{
    return m_customContextMenuActions;
}

void PlacesPanel::setCustomContextMenuActions(const QList<QAction *> &actions)
{
    m_customContextMenuActions = actions;
}

void PlacesPanel::proceedWithTearDown()
{
    if (m_indexToTearDown.isValid()) {
        auto *placesModel = static_cast<KFilePlacesModel *>(model());
        placesModel->requestTeardown(m_indexToTearDown);
    } else {
        qWarning() << "Places entry to tear down is no longer valid";
    }
}

void PlacesPanel::readSettings()
{
    if (GeneralSettings::autoExpandFolders()) {
        setDragAutoActivationDelay(750);
    } else {
        setDragAutoActivationDelay(0);
    }

    const int iconSize = qMax(0, PlacesPanelSettings::iconSize());
    setIconSize(QSize(iconSize, iconSize));
}

static bool isInternalDrag(const QMimeData *mimeData)
{
    const auto formats = mimeData->formats();
    for (const auto &format : formats) {
        // from KFilePlacesModel::_k_internalMimetype
        if (format.startsWith(QLatin1String("application/x-kfileplacesmodel-"))) {
            return true;
        }
    }
    return false;
}

static QUrl draggedFavoriteFolder(const QMimeData *mimeData)
{
    if (!mimeData || !mimeData->hasUrls() || mimeData->urls().size() != 1) return {};
    const QUrl url = mimeData->urls().constFirst();
    return url.isLocalFile() && QFileInfo(url.toLocalFile()).isDir() ? url : QUrl();
}

void PlacesPanel::dragEnterEvent(QDragEnterEvent *event)
{
    if (draggedFavoriteFolder(event->mimeData()).isValid()) {
        event->acceptProposedAction();
        return;
    }
    KFilePlacesView::dragEnterEvent(event);
}

void PlacesPanel::dragMoveEvent(QDragMoveEvent *event)
{
    if (m_favoritesHeaderRect.contains(event->position().toPoint())
        && draggedFavoriteFolder(event->mimeData()).isValid()) {
        event->acceptProposedAction();
        return;
    }
    const QModelIndex index = indexAt(event->position().toPoint());
    if (index.isValid()) {
        auto *placesModel = static_cast<KFilePlacesModel *>(model());

        // Reject drag ontop of a non-writable protocol
        // We don't know whether we're dropping inbetween or ontop of a place
        // so still allow internal drag events so that re-arranging still works.
        if (!isInternalDrag(event->mimeData())) {
            const QUrl url = placesModel->url(index);
            if (!url.isValid() || !KProtocolManager::supportsWriting(url)) {
                event->setDropAction(Qt::IgnoreAction);
            } else {
                DragAndDropHelper::updateDropAction(event, url);
            }
        }
    }

    KFilePlacesView::dragMoveEvent(event);
}

void PlacesPanel::dropEvent(QDropEvent *event)
{
    if (m_favoritesHeaderRect.contains(event->position().toPoint())) {
        const QUrl folder = draggedFavoriteFolder(event->mimeData());
        if (folder.isValid()) {
            DolphinPlacesModelSingleton::instance().placesModel()->addFavorite(folder);
            viewport()->update();
            event->setDropAction(Qt::LinkAction);
            event->accept();
            return;
        }
    }
    KFilePlacesView::dropEvent(event);
}

QModelIndex PlacesPanel::aero7IndexForName(const QString &name) const
{
    if (!model())
        return {};
    const auto *placesModel = static_cast<const KFilePlacesModel *>(model());
    for (int row = 0; row < model()->rowCount(); ++row) {
        const QModelIndex index = model()->index(row, 0);
        // KFilePlacesModel retains hidden KDE defaults such as ~/Documents
        // alongside Aero7's visible materialized Library entries.  The custom
        // Windows 7 renderer addresses rows by their display name, so choosing
        // the first matching row could silently activate the hidden KDE place
        // and expose "aero > Documents" instead of "Libraries > Documents".
        // Only rows that the Aero7 model deliberately exposes may back a
        // painted navigation item.
        if (!index.isValid() || placesModel->isHidden(index))
            continue;
        if (index.data(Qt::DisplayRole).toString() == name)
            return index;
    }
    return {};
}

QSize PlacesPanel::sizeHint() const
{
    QSize result = KFilePlacesView::sizeHint();
    // Include the icon/indent and right margin, not just the text width.
    int width = 160;
    for (const auto &label : {QStringLiteral("Local Disk (C:)"), QStringLiteral("Recent Places"),
                              QStringLiteral("Documents"), QStringLiteral("Downloads")})
        width = qMax(width, fontMetrics().horizontalAdvance(label) + 54);
    result.setWidth(width + 2 * frameWidth());
    return result;
}

QModelIndex PlacesPanel::aero7IndexAt(const QPoint &position) const
{
    for (const Aero7NavigationHit &hit : m_aero7NavigationHits) {
        if (hit.rect.contains(position))
            return hit.index;
    }
    return {};
}

void PlacesPanel::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(viewport());
    painter.fillRect(viewport()->rect(), palette().base());
    painter.setRenderHint(QPainter::Antialiasing, true);
    m_aero7NavigationHits.clear();

    const QModelIndex selected = currentIndex();
    const int width = viewport()->width();
    const int rowHeight = 21;
    const int groupHeight = 21;
    const int iconSize = 16;
    int y = 7;
    const auto *placesModel = DolphinPlacesModelSingleton::instance().placesModel();

    const auto drawSelection = [&](const QRect &rect, bool active) {
        if (!active)
            return;
        QLinearGradient gradient(rect.topLeft(), rect.bottomLeft());
        gradient.setColorAt(0.0, QColor(QStringLiteral("#edf7ff")));
        gradient.setColorAt(1.0, QColor(QStringLiteral("#d6eafb")));
        painter.setPen(QColor(QStringLiteral("#7da2ce")));
        painter.setBrush(gradient);
        painter.drawRect(rect.adjusted(0, 0, -1, -1));
    };

    const auto drawItem = [&](const QModelIndex &index, int itemY, bool showDisclosure) {
        if (!index.isValid())
            return;
        const QString name = index.data(Qt::DisplayRole).toString();
        const QRect hitRect(1, itemY, qMax(0, width - 2), rowHeight);
        drawSelection(hitRect, selected == index);
        const int iconX = 29;
        if (showDisclosure) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(QStringLiteral("#8293a4")));
            painter.drawPolygon(QPolygon({QPoint(19, itemY + 7), QPoint(19, itemY + 13),
                                           QPoint(23, itemY + 10)}));
        }
        QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        for (const DolphinPlacesModel::Favorite &favorite : placesModel->favorites()) {
            if (placesModel->url(index).matches(favorite.url, QUrl::StripTrailingSlash)) {
                icon = Aero7Icons::icon(favorite.icon);
                break;
            }
        }
        icon.paint(&painter, QRect(iconX, itemY + 2, iconSize, iconSize), Qt::AlignCenter,
                   selected == index ? QIcon::Selected : QIcon::Normal);
        painter.setPen(QColor(QStringLiteral("#111111")));
        const int textWidth = qMax(0, width - iconX - 24);
        painter.drawText(QRect(iconX + 21, itemY, textWidth, rowHeight),
                         Qt::AlignVCenter | Qt::AlignLeft,
                         painter.fontMetrics().elidedText(name, Qt::ElideRight, textWidth));
        m_aero7NavigationHits.append({hitRect, index});
    };

    const auto drawGroup = [&](const QString &name, const QString &iconName,
                               const QModelIndexList &children, bool clickable,
                               bool childDisclosures) {
        const QModelIndex groupIndex = clickable ? aero7IndexForName(name) : QModelIndex();
        const QRect groupRect(1, y, qMax(0, width - 2), groupHeight);
        if (name == QLatin1String("Favorites")) m_favoritesHeaderRect = groupRect;
        drawSelection(groupRect, groupIndex.isValid() && selected == groupIndex);

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(QStringLiteral("#66798d")));
        const QPolygon triangle = children.isEmpty()
            ? QPolygon({QPoint(11, y + 7), QPoint(11, y + 13), QPoint(15, y + 10)})
            : QPolygon({QPoint(10, y + 9), QPoint(16, y + 9), QPoint(13, y + 13)});
        painter.drawPolygon(triangle);
        Aero7Icons::icon(iconName).paint(&painter, QRect(20, y + 2, iconSize, iconSize));
        painter.setPen(QColor(QStringLiteral("#274b72")));
        painter.drawText(QRect(41, y, width - 44, groupHeight),
                         Qt::AlignVCenter | Qt::AlignLeft, name);
        if (groupIndex.isValid())
            m_aero7NavigationHits.append({groupRect, groupIndex});

        y += groupHeight + 2;
        for (const QModelIndex &child : children) {
            if (!child.isValid()) continue;
            drawItem(child, y, childDisclosures);
            y += rowHeight;
        }
    };

    QModelIndexList libraries;
    QModelIndexList drives;
    QModelIndexList favorites;
    for (const DolphinPlacesModel::Favorite &favorite : placesModel->favorites()) {
        const QModelIndex item = placesModel->favoriteIndex(favorite.url);
        if (item.isValid() && !placesModel->isHidden(item)) favorites.append(item);
    }
    for (int row = 0; row < placesModel->rowCount(); ++row) {
        const QModelIndex item = placesModel->index(row, 0);
        if (placesModel->isHidden(item)) continue;
        const QString group = item.data(KFilePlacesModel::GroupRole).toString();
        if (group == QLatin1String("Libraries")) libraries.append(item);
        if (group == QLatin1String("Computer") && item != aero7IndexForName(QStringLiteral("Computer")))
            drives.append(item);
    }
    drawGroup(QStringLiteral("Favorites"), QStringLiteral("bookmarks"), favorites, false, false);
    y += 20;
    drawGroup(QStringLiteral("Libraries"), QStringLiteral("folder-library"), libraries, false, true);
    y += 20;
    drawGroup(QStringLiteral("Computer"), QStringLiteral("computer"), drives, true, true);
    y += 20;
    drawGroup(QStringLiteral("Network"), QStringLiteral("network-workgroup"), {}, true, false);
}

void PlacesPanel::activateAero7Place(const QModelIndex &index, bool newWindow)
{
    auto *places = static_cast<KFilePlacesModel *>(model());
    if (!index.isValid() || places->isHidden(index)) return;
    const auto *access = places->deviceForIndex(index).as<Solid::StorageAccess>();
    if (access && !access->isAccessible()) {
        // Custom Aero7 painting must not bypass the native asynchronous mount
        // path. Do not navigate to a device's empty pre-mount URL.
        if (findChild<QObject *>(QStringLiteral("aero7PendingSetup"))) return;
        auto *request = new QObject(this);
        request->setObjectName(QStringLiteral("aero7PendingSetup"));
        const QPersistentModelIndex target(index);
        connect(places, &KFilePlacesModel::setupDone, request,
                [this, places, request, target, newWindow](const QModelIndex &finished, bool success) {
            if (!target.isValid() || finished != target) return;
            request->setObjectName(QString());
            request->deleteLater();
            if (!success) return; // KIO's errorMessage signal supplies the failure.
            const QUrl opened = places->url(target);
            if (!opened.isValid() || opened.isEmpty()) return;
            if (newWindow) Q_EMIT newWindowRequested(opened);
            else Q_EMIT placeActivated(opened);
        });
        QTimer::singleShot(30000, request, [this, request] {
            request->setObjectName(QString());
            request->deleteLater();
            Q_EMIT errorMessage(QStringLiteral("The drive did not become available. Please try again."));
        });
        places->requestSetup(index);
        return;
    }
    // Choosing somewhere else cancels delayed navigation, not the operating
    // system's already-started mount operation.
    if (auto *request = findChild<QObject *>(QStringLiteral("aero7PendingSetup"))) {
        disconnect(places, nullptr, request, nullptr);
        request->setObjectName(QString());
        request->deleteLater();
    }
    const QUrl opened = places->url(index);
    if (!opened.isValid() || opened.isEmpty()) return;
    if (newWindow) Q_EMIT newWindowRequested(opened);
    else Q_EMIT placeActivated(opened);
}

void PlacesPanel::mousePressEvent(QMouseEvent *event)
{
    const QModelIndex index = aero7IndexAt(event->position().toPoint());
    if (event->button() == Qt::LeftButton && index.isValid()) {
        setCurrentIndex(index);
        viewport()->update();
        activateAero7Place(index);
        event->accept();
        return;
    }
    if (event->button() == Qt::RightButton) {
        event->accept();
        return;
    }
    KFilePlacesView::mousePressEvent(event);
}

void PlacesPanel::contextMenuEvent(QContextMenuEvent *event)
{
    if (event->reason() != QContextMenuEvent::Keyboard && m_favoritesHeaderRect.contains(event->pos())) {
        QMenu menu(this);
        populateFavoritesHeaderMenu(menu);
        menu.exec(event->globalPos());
        event->accept();
        return;
    }
    const QModelIndex index = event->reason() == QContextMenuEvent::Keyboard
        ? currentIndex() : aero7IndexAt(event->pos());
    if (!index.isValid()) {
        event->accept();
        return;
    }

    QMenu menu(this);
    populateAero7ContextMenu(menu, index);
    if (!menu.isEmpty()) menu.exec(event->globalPos());
    event->accept();
}

void PlacesPanel::populateAero7ContextMenu(QMenu &menu, const QModelIndex &index)
{
    auto *places = static_cast<DolphinPlacesModel *>(model());
    if (!index.isValid() || index.model() != places || places->isHidden(index)) return;
    const QUrl url = places->url(index);
    QAction *open = menu.addAction(Aero7Icons::icon(QStringLiteral("document-open-folder")),
                                   QStringLiteral("Open"));
    const QPersistentModelIndex target(index);
    connect(open, &QAction::triggered, this, [this, target]() { activateAero7Place(target); });
    QAction *newWindow = menu.addAction(Aero7Icons::icon(QStringLiteral("window-new")),
                                       QStringLiteral("Open in new window"));
    connect(newWindow, &QAction::triggered, this, [this, target]() { activateAero7Place(target, true); });

    if (places->isFavorite(url)) {
        menu.addSeparator();
        QAction *remove = menu.addAction(Aero7Icons::icon(QStringLiteral("edit-delete")), QStringLiteral("Remove"));
        connect(remove, &QAction::triggered, this, [this, places, url] {
            places->removeFavorite(url);
            viewport()->update();
        });
        QAction *rename = menu.addAction(Aero7Icons::icon(QStringLiteral("document-properties")), QStringLiteral("Rename"));
        connect(rename, &QAction::triggered, this, [this, places, url] {
            bool accepted = false;
            const QString current = places->text(places->favoriteIndex(url));
            const QString name = QInputDialog::getText(this, QStringLiteral("Rename Favorite"),
                                                        QStringLiteral("Name:"), QLineEdit::Normal,
                                                        current, &accepted);
            if (accepted && places->renameFavorite(url, name)) viewport()->update();
        });
        QAction *properties = menu.addAction(Aero7Icons::icon(QStringLiteral("document-properties")),
                                             QStringLiteral("Properties"));
        connect(properties, &QAction::triggered, this, [this, url] { Aero7Properties::show({url}, this); });
        return;
    }

    // KIO supplies capability/state-aware actions, but does not connect them.
    // Own them with this short-lived menu and retain persistent indices: a USB
    // drive can disappear while the context menu is open.
    if (places->isDevice(index)) {
        if (auto *eject = places->ejectActionForIndex(index)) {
            eject->setParent(&menu);
            eject->setObjectName(QStringLiteral("aero7EjectDrive"));
            eject->setIcon(Aero7Icons::icon(QStringLiteral("media-eject")));
            menu.addSeparator();
            menu.addAction(eject);
            connect(eject, &QAction::triggered, &menu, [places, target] {
                if (target.isValid() && !places->isHidden(target)) {
                    // Recheck capability after hotplug/model changes.
                    const std::unique_ptr<QAction> current(places->ejectActionForIndex(target));
                    if (current && current->isEnabled()) places->requestEject(target);
                }
            });
        }
        if (auto *teardown = places->teardownActionForIndex(index)) {
            teardown->setParent(&menu);
            teardown->setObjectName(QStringLiteral("aero7UnmountDrive"));
            teardown->setIcon(Aero7Icons::icon(QStringLiteral("media-eject")));
            teardown->setEnabled(teardown->isEnabled() && places->isTeardownAllowed(index)
                                 && !m_indexToTearDown.isValid());
            if (menu.actions().constLast()->objectName() != QLatin1String("aero7EjectDrive"))
                menu.addSeparator();
            menu.addAction(teardown);
            connect(teardown, &QAction::triggered, &menu, [this, places, target] {
                if (!target.isValid() || places->isHidden(target)
                    || !places->isTeardownAllowed(target) || m_indexToTearDown.isValid()) return;
                const std::unique_ptr<QAction> current(places->teardownActionForIndex(target));
                if (current && current->isEnabled()) slotTearDownRequested(target);
            });
        }
    }

    if (url.isLocalFile()) {
        const QString id = Aero7Libraries::instance().libraryIdForPath(url.toLocalFile());
        if (!id.isEmpty()) {
            menu.addSeparator();
            QAction *properties = menu.addAction(Aero7Icons::icon(QStringLiteral("document-properties")),
                                                 QStringLiteral("Properties"));
            connect(properties, &QAction::triggered, this,
                    [this, id]() { Aero7Properties::showLibrary(id, this); });
        }
    }
}

void PlacesPanel::populateFavoritesHeaderMenu(QMenu &menu)
{
    auto *places = DolphinPlacesModelSingleton::instance().placesModel();
    QAction *restore = menu.addAction(Aero7Icons::icon(QStringLiteral("edit-reset")),
                                      QStringLiteral("Restore favorite links"));
    connect(restore, &QAction::triggered, this, [this, places] {
        places->restoreFavorites();
        viewport()->update();
    });
    if (m_currentFolderUrl.isLocalFile() && QFileInfo(m_currentFolderUrl.toLocalFile()).isDir()
        && !places->isFavorite(m_currentFolderUrl)) {
        QAction *add = menu.addAction(Aero7Icons::icon(QStringLiteral("bookmarks")),
                                      QStringLiteral("Add current folder to Favorites"));
        connect(add, &QAction::triggered, this, [this, places] {
            places->addFavorite(m_currentFolderUrl);
            viewport()->update();
        });
    }
}

void PlacesPanel::slotConfigureTrash()
{
    Aero7Properties::showTrash(this);
}

void PlacesPanel::slotUrlsDropped(const QUrl &dest, QDropEvent *event, QWidget *parent)
{
    KIO::DropJob *job = DragAndDropHelper::dropUrls(dest, event, parent);
    if (job) {
        connect(job, &KIO::DropJob::result, this, [this](KJob *job) {
            if (job->error() && job->error() != KIO::ERR_USER_CANCELED) {
                Q_EMIT errorMessage(job->errorString());
            }
        });
    }
}

void PlacesPanel::slotContextMenuAboutToShow(const QModelIndex &index, QMenu *menu)
{
    auto *placesModel = static_cast<KFilePlacesModel *>(model());
    const QUrl url = placesModel->url(index);
    const Solid::Device device = placesModel->deviceForIndex(index);

    m_configureTrashAction->setVisible(url.scheme() == QLatin1String("trash"));
    m_openInSplitView->setVisible(url.isValid());

    if (url.isLocalFile()) {
        const QString id = Aero7Libraries::instance().libraryIdForPath(url.toLocalFile());
        if (!id.isEmpty()
            && QDir::cleanPath(url.toLocalFile())
                == QDir::cleanPath(Aero7Libraries::instance().materializedPath(id))) {
            menu->addSeparator();
            QAction *properties = menu->addAction(Aero7Icons::icon(QStringLiteral("document-properties")),
                                                  QStringLiteral("Properties"));
            connect(properties, &QAction::triggered, this,
                    [this, id]() { Aero7Properties::showLibrary(id, this); });
        }
    }

    // show customContextMenuActions only on the view's context menu
    if (!url.isValid() && !device.isValid()) {
        addActions(m_customContextMenuActions);
    } else {
        const auto actions = this->actions();
        for (QAction *action : actions) {
            if (m_customContextMenuActions.contains(action)) {
                removeAction(action);
            }
        }
    }
}

void PlacesPanel::slotTearDownRequested(const QModelIndex &index)
{
    auto *placesModel = static_cast<KFilePlacesModel *>(model());

    Solid::StorageAccess *storageAccess = placesModel->deviceForIndex(index).as<Solid::StorageAccess>();
    if (!storageAccess) {
        return;
    }

    m_indexToTearDown = QPersistentModelIndex(index);
    m_tearDownPaths.insert(storageAccess, storageAccess->filePath());

    // disconnect the Solid::StorageAccess::teardownRequested
    // to prevent emitting PlacesPanel::storageTearDownExternallyRequested
    // after we have emitted PlacesPanel::storageTearDownRequested
    disconnect(storageAccess, &Solid::StorageAccess::teardownRequested, this, &PlacesPanel::slotTearDownRequestedExternally);
    Q_EMIT storageTearDownRequested(storageAccess->filePath());
}

void PlacesPanel::slotTearDownRequestedExternally(const QString &udi)
{
    Q_UNUSED(udi);
    auto *storageAccess = static_cast<Solid::StorageAccess *>(sender());
    m_tearDownPaths.insert(storageAccess, storageAccess->filePath());
    Q_EMIT storageTearDownExternallyRequested(storageAccess->filePath());
}

void PlacesPanel::slotTearDownDone(const QModelIndex &index, Solid::ErrorType error, const QVariant &errorData)
{
    Q_UNUSED(errorData); // All error handling is currently done in frameworks.

    if (index == m_indexToTearDown) {
        m_indexToTearDown = QPersistentModelIndex();
    }
    Q_UNUSED(error);
}

void PlacesPanel::completeTearDown(const QObject *access, Solid::ErrorType error)
{
    // Consume the matching request on failure too. A later success on another
    // drive must neither run stale recovery nor disconnect another listener.
    const QString mountPath = m_tearDownPaths.take(access);
    if (error == Solid::ErrorType::NoError && !mountPath.isEmpty() && mountPath != QLatin1String("/")) {
        Q_EMIT storageTearDownSuccessful(mountPath);
    }
}

void PlacesPanel::slotNativeTearDownDone(Solid::ErrorType error, const QVariant &errorData, const QString &udi)
{
    Q_UNUSED(errorData);
    Q_UNUSED(udi);
    auto *access = qobject_cast<Solid::StorageAccess *>(sender());
    if (!access) return;
    // Internal requests temporarily suppress the external-request callback.
    // Restore it for the next native operation, including after a failure.
    connect(access, &Solid::StorageAccess::teardownRequested, this,
            &PlacesPanel::slotTearDownRequestedExternally, Qt::UniqueConnection);
    completeTearDown(access, error);
}

void PlacesPanel::slotStorageAccessDestroyed(QObject *access)
{
    m_tearDownPaths.remove(access);
}

void PlacesPanel::slotRowsInserted(const QModelIndex &parent, int first, int last)
{
    for (int i = first; i <= last; ++i) {
        connectDeviceSignals(model()->index(i, 0, parent));
    }
}

void PlacesPanel::slotRowsAboutToBeRemoved(const QModelIndex &parent, int first, int last)
{
    auto *placesModel = static_cast<KFilePlacesModel *>(model());

    for (int i = first; i <= last; ++i) {
        const QModelIndex index = placesModel->index(i, 0, parent);

        Solid::StorageAccess *storageAccess = placesModel->deviceForIndex(index).as<Solid::StorageAccess>();
        if (!storageAccess) {
            continue;
        }

        disconnect(storageAccess, &Solid::StorageAccess::teardownRequested, this, nullptr);
    }
}

void PlacesPanel::connectDeviceSignals(const QModelIndex &index)
{
    auto *placesModel = static_cast<KFilePlacesModel *>(model());

    Solid::StorageAccess *storageAccess = placesModel->deviceForIndex(index).as<Solid::StorageAccess>();
    if (!storageAccess) {
        return;
    }

    connect(storageAccess, &Solid::StorageAccess::teardownRequested, this,
            &PlacesPanel::slotTearDownRequestedExternally, Qt::UniqueConnection);
    connect(storageAccess, &Solid::StorageAccess::teardownDone, this,
            &PlacesPanel::slotNativeTearDownDone, Qt::UniqueConnection);
    connect(storageAccess, &QObject::destroyed, this,
            &PlacesPanel::slotStorageAccessDestroyed, Qt::UniqueConnection);
}

#include "moc_placespanel.cpp"
