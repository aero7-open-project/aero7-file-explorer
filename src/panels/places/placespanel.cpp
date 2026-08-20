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
#include <QFileInfo>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QShowEvent>

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

    m_openInSplitView = std::make_unique<QAction>(QIcon::fromTheme(QStringLiteral("view-split-left-right")), i18nc("@action:inmenu", "Open in Split View"));
    m_openInSplitView->setPriority(QAction::HighPriority);
    connect(m_openInSplitView.get(), &QAction::triggered, this, [this]() {
        const QUrl url = currentIndex().data(KFilePlacesModel::UrlRole).toUrl();
        Q_EMIT openInSplitViewRequested(url);
    });
    addAction(m_openInSplitView.get());

    m_configureTrashAction = std::make_unique<QAction>(QIcon::fromTheme(QStringLiteral("configure")), i18nc("@action:inmenu", "Configure Trash…"));
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
        // KFilePlacesView otherwise keeps its closest hidden device row visible
        // for ordinary folders, leaving an empty KDE device-section heading.
        if (!matchedVisiblePlace)
            navigationUrl = QUrl();
    }
    KFilePlacesView::setUrl(navigationUrl);
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

void PlacesPanel::dragMoveEvent(QDragMoveEvent *event)
{
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

    const auto drawItem = [&](const QString &name, int itemY, bool showDisclosure) {
        const QModelIndex index = aero7IndexForName(name);
        if (!index.isValid())
            return;
        const QRect hitRect(1, itemY, qMax(0, width - 2), rowHeight);
        drawSelection(hitRect, selected == index);
        const int iconX = 29;
        if (showDisclosure) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(QStringLiteral("#8293a4")));
            painter.drawPolygon(QPolygon({QPoint(19, itemY + 7), QPoint(19, itemY + 13),
                                           QPoint(23, itemY + 10)}));
        }
        const QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        icon.paint(&painter, QRect(iconX, itemY + 2, iconSize, iconSize), Qt::AlignCenter,
                   selected == index ? QIcon::Selected : QIcon::Normal);
        painter.setPen(QColor(QStringLiteral("#111111")));
        painter.drawText(QRect(iconX + 21, itemY, width - iconX - 24, rowHeight),
                         Qt::AlignVCenter | Qt::AlignLeft, name);
        m_aero7NavigationHits.append({hitRect, index});
    };

    const auto drawGroup = [&](const QString &name, const QString &iconName,
                               const QStringList &children, bool clickable,
                               bool childDisclosures) {
        const QModelIndex groupIndex = clickable ? aero7IndexForName(name) : QModelIndex();
        const QRect groupRect(1, y, qMax(0, width - 2), groupHeight);
        drawSelection(groupRect, groupIndex.isValid() && selected == groupIndex);

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(QStringLiteral("#66798d")));
        const QPolygon triangle = children.isEmpty()
            ? QPolygon({QPoint(11, y + 7), QPoint(11, y + 13), QPoint(15, y + 10)})
            : QPolygon({QPoint(10, y + 9), QPoint(16, y + 9), QPoint(13, y + 13)});
        painter.drawPolygon(triangle);
        QIcon::fromTheme(iconName).paint(&painter, QRect(20, y + 2, iconSize, iconSize));
        painter.setPen(QColor(QStringLiteral("#274b72")));
        painter.drawText(QRect(41, y, width - 44, groupHeight),
                         Qt::AlignVCenter | Qt::AlignLeft, name);
        if (groupIndex.isValid())
            m_aero7NavigationHits.append({groupRect, groupIndex});

        y += groupHeight + 2;
        for (const QString &child : children) {
            drawItem(child, y, childDisclosures);
            y += rowHeight;
        }
    };

    drawGroup(QStringLiteral("Favorites"), QStringLiteral("favorites"),
              {QStringLiteral("Recent Places"), QStringLiteral("Desktop"),
               QStringLiteral("Downloads")}, false, false);
    y += 20;
    drawGroup(QStringLiteral("Libraries"), QStringLiteral("folder-library"),
              {QStringLiteral("Documents"), QStringLiteral("Music"),
               QStringLiteral("New Library"), QStringLiteral("Pictures"),
               QStringLiteral("Videos")}, false, true);
    y += 20;
    drawGroup(QStringLiteral("Computer"), QStringLiteral("computer"),
              {QStringLiteral("Local Disk (C:)"), QStringLiteral("CD Drive (D:)")}, true, true);
    y += 20;
    drawGroup(QStringLiteral("Network"), QStringLiteral("network-workgroup"), {}, true, false);
}

void PlacesPanel::mousePressEvent(QMouseEvent *event)
{
    const QModelIndex index = aero7IndexAt(event->position().toPoint());
    if (event->button() == Qt::LeftButton && index.isValid()) {
        setCurrentIndex(index);
        viewport()->update();
        Q_EMIT placeActivated(index.data(KFilePlacesModel::UrlRole).toUrl());
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
    const QModelIndex index = aero7IndexAt(event->pos());
    if (!index.isValid()) {
        event->accept();
        return;
    }

    const QUrl url = index.data(KFilePlacesModel::UrlRole).toUrl();
    QMenu menu(this);
    QAction *open = menu.addAction(QIcon::fromTheme(QStringLiteral("document-open-folder")),
                                   QStringLiteral("Open"));
    connect(open, &QAction::triggered, this, [this, url]() { Q_EMIT placeActivated(url); });
    QAction *newWindow = menu.addAction(QIcon::fromTheme(QStringLiteral("window-new")),
                                       QStringLiteral("Open in new window"));
    connect(newWindow, &QAction::triggered, this, [this, url]() { Q_EMIT newWindowRequested(url); });

    if (url.isLocalFile()) {
        const QString id = Aero7Libraries::instance().libraryIdForPath(url.toLocalFile());
        if (!id.isEmpty()) {
            menu.addSeparator();
            QAction *properties = menu.addAction(QIcon::fromTheme(QStringLiteral("document-properties")),
                                                 QStringLiteral("Properties"));
            connect(properties, &QAction::triggered, this,
                    [this, id]() { Aero7Properties::showLibrary(id, this); });
        }
    }
    menu.exec(event->globalPos());
    event->accept();
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
            QAction *properties = menu->addAction(QIcon::fromTheme(QStringLiteral("document-properties")),
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

    Q_EMIT storageTearDownExternallyRequested(storageAccess->filePath());
}

void PlacesPanel::slotTearDownDone(const QModelIndex &index, Solid::ErrorType error, const QVariant &errorData)
{
    Q_UNUSED(errorData); // All error handling is currently done in frameworks.

    if (index == m_indexToTearDown) {
        if (error == Solid::ErrorType::NoError) {
            // No error; it must have been unmounted successfully
            Q_EMIT storageTearDownSuccessful();
        }
        m_indexToTearDown = QPersistentModelIndex();
    }
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

    connect(storageAccess, &Solid::StorageAccess::teardownRequested, this, &PlacesPanel::slotTearDownRequestedExternally);
}

#include "moc_placespanel.cpp"
