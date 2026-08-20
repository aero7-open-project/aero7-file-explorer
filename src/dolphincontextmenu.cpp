/*
 * SPDX-FileCopyrightText: 2006 Peter Penz (peter.penz@gmx.at) and Cvetoslav Ludmiloff
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "dolphincontextmenu.h"

#include "dolphin_contextmenusettings.h"
#include "dolphin_generalsettings.h"
#include "dolphinmainwindow.h"
#include "dolphinnewfilemenu.h"
#include "dolphinplacesmodelsingleton.h"
#include "dolphinremoveaction.h"
#include "dolphinviewcontainer.h"
#include "global.h"
#include "trash/dolphintrash.h"
#include "views/dolphinview.h"
#include "aero7properties.h"

#include <KActionCollection>
#include <KFileItemListProperties>
#include <KHamburgerMenu>
#include <KIO/EmptyTrashJob>
#include <KIO/JobUiDelegate>
#include <KIO/ListJob>
#include <KIO/Paste>
#include <KIO/RestoreJob>
#include <KJobWidgets>
#include <KLocalizedString>
#include <KNewFileMenu>
#include <KStandardAction>

#include <QApplication>
#include <QClipboard>
#include <QKeyEvent>
#include <QAction>

DolphinContextMenu::DolphinContextMenu(DolphinMainWindow *parent,
                                       const KFileItem &fileInfo,
                                       const KFileItemList &selectedItems,
                                       const QUrl &baseUrl,
                                       KFileItemActions *fileItemActions)
    : QMenu(parent)
    , m_mainWindow(parent)
    , m_fileInfo(fileInfo)
    , m_baseUrl(baseUrl)
    , m_baseFileItem(nullptr)
    , m_selectedItems(selectedItems)
    , m_selectedItemsProperties(nullptr)
    , m_context(NoContext)
    , m_copyToMenu(parent)
    , m_removeAction(nullptr)
    , m_fileItemActions(fileItemActions)
{
    QApplication::instance()->installEventFilter(this);

    addAllActions();
}

DolphinContextMenu::~DolphinContextMenu()
{
    delete m_baseFileItem;
    m_baseFileItem = nullptr;
    delete m_selectedItemsProperties;
    m_selectedItemsProperties = nullptr;
}

void DolphinContextMenu::addAllActions()
{
    // get the context information
    const auto scheme = m_baseUrl.scheme();
    if (scheme == QLatin1String("trash")) {
        m_context |= TrashContext;
    } else if (scheme.contains(QLatin1String("search"))) {
        m_context |= SearchContext;
    } else if (scheme.contains(QLatin1String("timeline"))) {
        m_context |= TimelineContext;
    } else if (scheme == QStringLiteral("recentlyused")) {
        m_context |= RecentlyUsedContext;
    }

    if (!m_fileInfo.isNull() && !m_selectedItems.isEmpty()) {
        m_context |= ItemContext;
        // TODO: handle other use cases like devices + desktop files
    }

    // open the corresponding popup for the context
    if (m_context & TrashContext) {
        if (m_context & ItemContext) {
            addTrashItemContextMenu();
        } else {
            addTrashContextMenu();
        }
    } else if (m_context & ItemContext) {
        addItemContextMenu();
    } else {
        addViewportContextMenu();
    }
}

bool DolphinContextMenu::eventFilter(QObject *object, QEvent *event)
{
    Q_UNUSED(object)

    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);

        if (m_removeAction && keyEvent->key() == Qt::Key_Shift) {
            if (event->type() == QEvent::KeyPress) {
                m_removeAction->update(DolphinRemoveAction::ShiftState::Pressed);
            } else {
                m_removeAction->update(DolphinRemoveAction::ShiftState::Released);
            }
        }
    }

    return false;
}

void DolphinContextMenu::addTrashContextMenu()
{
    // Insert 'Sort By' and 'View Mode'
    if (ContextMenuSettings::showSortBy()) {
        addAction(m_mainWindow->actionCollection()->action(QStringLiteral("sort")));
    }
    if (ContextMenuSettings::showViewMode()) {
        addAction(m_mainWindow->actionCollection()->action(QStringLiteral("view_mode")));
    }

    if (ContextMenuSettings::showSortBy() || ContextMenuSettings::showViewMode()) {
        addSeparator();
    }

    Q_ASSERT(m_context & TrashContext);

    QAction *restoreAllAction = addAction(QIcon::fromTheme(QStringLiteral("edit-reset")),
                                          QStringLiteral("Restore all items"), this, [this]() {
        auto *job = KIO::listDir(QUrl(QStringLiteral("trash:/")), KIO::HideProgressInfo);
        auto *urls = new QList<QUrl>;
        connect(job, &KIO::ListJob::entries, job,
                [urls](KIO::Job *, const KIO::UDSEntryList &entries) {
            for (const KIO::UDSEntry &entry : entries) {
                const QString url = entry.stringValue(KIO::UDSEntry::UDS_URL);
                const QString name = entry.stringValue(KIO::UDSEntry::UDS_NAME);
                if (!url.isEmpty())
                    urls->append(QUrl(url));
                else if (!name.isEmpty() && name != QLatin1String(".") && name != QLatin1String(".."))
                    urls->append(QUrl(QStringLiteral("trash:/") + name));
            }
        });
        connect(job, &KJob::result, job, [this, urls](KJob *completed) {
            if (!completed->error() && !urls->isEmpty())
                KIO::restoreFromTrash(*urls);
            delete urls;
        });
    });
    restoreAllAction->setEnabled(!Trash::isEmpty());

    QAction *emptyTrashAction = addAction(QIcon::fromTheme(QStringLiteral("edit-delete")), i18nc("@action:inmenu", "Empty Recycle Bin"), this, [this]() {
        Trash::empty(m_mainWindow);
    });
    emptyTrashAction->setEnabled(!Trash::isEmpty());

    connect(&Trash::instance(), &Trash::emptinessChanged, this, [emptyTrashAction](bool isEmpty) {
        emptyTrashAction->setEnabled(!isEmpty);
    });

    addSeparator();

    auto *configureTrashAction = new QAction(QIcon::fromTheme(QStringLiteral("configure")), i18nc("@action:inmenu", "Recycle Bin Properties…"), this);
    connect(configureTrashAction, &QAction::triggered, this, &DolphinContextMenu::configureTrash);
    addAction(configureTrashAction);
}

void DolphinContextMenu::configureTrash()
{
    Aero7Properties::showTrash(m_mainWindow);
}

void DolphinContextMenu::addTrashItemContextMenu()
{
    Q_ASSERT(m_context & TrashContext);
    Q_ASSERT(m_context & ItemContext);

    addAction(QIcon::fromTheme(QStringLiteral("edit-reset")),
              i18ncp("@action:inmenu Restore the selected files that are in the trash to the place they lived at the moment they were trashed. Minimize the "
                     "length of this string if possible.",
                     "Restore to Former Location",
                     "Restore to Former Locations",
                     m_selectedItems.count()),
              this,
              [this]() {
                  QList<QUrl> selectedUrls;
                  selectedUrls.reserve(m_selectedItems.count());
                  for (const KFileItem &item : std::as_const(m_selectedItems)) {
                      selectedUrls.append(item.url());
                  }

                  KIO::RestoreJob *job = KIO::restoreFromTrash(selectedUrls);
                  KJobWidgets::setWindow(job, m_mainWindow);
                  job->uiDelegate()->setAutoErrorHandlingEnabled(true);
              });

    addSeparator();

    addAction(m_mainWindow->actionCollection()->action(KStandardAction::name(KStandardAction::Cut)));
    addAction(m_mainWindow->actionCollection()->action(KStandardAction::name(KStandardAction::Copy)));

    addSeparator();

    QAction *deleteAction = m_mainWindow->actionCollection()->action(KStandardAction::name(KStandardAction::DeleteFile));
    addAction(deleteAction);

    addSeparator();

    addAction(m_mainWindow->actionCollection()->action(QStringLiteral("properties")));
}

void DolphinContextMenu::addDirectoryItemContextMenu()
{
    QAction *openAction = addAction(QIcon::fromTheme(QStringLiteral("document-open-folder")),
                                    QStringLiteral("Open"));
    connect(openAction, &QAction::triggered, this, [this]() {
        m_mainWindow->changeUrl(DolphinView::openItemAsFolderUrl(m_fileInfo));
    });
    if (ContextMenuSettings::showOpenInNewWindow()) {
        addAction(m_mainWindow->actionCollection()->action(QStringLiteral("open_in_new_window")));
    }
    addSeparator();
}

void DolphinContextMenu::addOpenParentFolderActions()
{
    addAction(QIcon::fromTheme(QStringLiteral("document-open-folder")), i18nc("@action:inmenu", "Open Path"), [this]() {
        const QUrl url = m_fileInfo.targetUrl();
        const QUrl parentUrl = KIO::upUrl(url);
        m_mainWindow->changeUrl(parentUrl);
        m_mainWindow->activeViewContainer()->view()->markUrlsAsSelected({url});
        m_mainWindow->activeViewContainer()->view()->markUrlAsCurrent(url);
    });

    addAction(QIcon::fromTheme(QStringLiteral("tab-new")), i18nc("@action:inmenu", "Open Path in New Tab"), [this]() {
        const QUrl url = m_fileInfo.targetUrl();
        const QUrl parentUrl = KIO::upUrl(url);
        DolphinTabPage *tabPage = m_mainWindow->openNewTab(parentUrl);
        tabPage->activeViewContainer()->view()->markUrlsAsSelected({url});
        tabPage->activeViewContainer()->view()->markUrlAsCurrent(url);
    });

    addAction(QIcon::fromTheme(QStringLiteral("window-new")), i18nc("@action:inmenu", "Open Path in New Window"), [this]() {
        Dolphin::openNewWindow({m_fileInfo.targetUrl()}, m_mainWindow, Dolphin::OpenNewWindowFlag::Select);
    });
}

void DolphinContextMenu::addItemContextMenu()
{
    Q_ASSERT(!m_fileInfo.isNull());

    const KFileItemListProperties &selectedItemsProps = selectedItemsProperties();
    // This is updated live in DolphinMainWindow::slotSelectionChanged but there can
    // be situations where there are no selected items.
    m_fileItemActions->setItemListProperties(selectedItemsProps);

    if (m_selectedItems.count() == 1) {
        // single files
        if (m_fileInfo.isDir()) {
            addDirectoryItemContextMenu();
        } else if (m_context & TimelineContext || m_context & SearchContext || m_context & RecentlyUsedContext) {
            addOpenWithActions();

            addOpenParentFolderActions();

            addSeparator();
        } else {
            QAction *openAction = addAction(QIcon::fromTheme(QStringLiteral("document-open")),
                                            QStringLiteral("Open"));
            connect(openAction, &QAction::triggered, this, [this]() {
                m_mainWindow->openFiles({m_fileInfo.url()}, false);
            });
            addOpenWithActions();
            addSeparator();
        }
        if (m_fileInfo.isLink()) {
            addAction(m_mainWindow->actionCollection()->action(QStringLiteral("show_target")));
            addSeparator();
        }
    } else {
        // multiple files
        bool selectionHasOnlyDirs = true;
        for (const auto &item : std::as_const(m_selectedItems)) {
            const QUrl &url = DolphinView::openItemAsFolderUrl(item);
            if (url.isEmpty()) {
                selectionHasOnlyDirs = false;
                break;
            }
        }

        if (selectionHasOnlyDirs && ContextMenuSettings::showOpenInNewTab()) {
            // insert 'Open in new tab' entry
            addAction(m_mainWindow->actionCollection()->action(QStringLiteral("open_in_new_tabs")));
        }
        // Insert 'Open With" entries
        addOpenWithActions();
    }

    insertDefaultItemActions(selectedItemsProps);

    // insert 'Properties...' entry
    addSeparator();
    QAction *propertiesAction = m_mainWindow->actionCollection()->action(QStringLiteral("properties"));
    addAction(propertiesAction);
}

void DolphinContextMenu::addViewportContextMenu()
{
    const KFileItemListProperties baseUrlProperties(KFileItemList() << baseFileItem());
    m_fileItemActions->setItemListProperties(baseUrlProperties);

    auto *viewMenu = addMenu(QStringLiteral("View"));
    const auto addView = [this, viewMenu](const QString &name, DolphinView::Mode mode, int zoom) {
        viewMenu->addAction(name, this, [this, mode, zoom]() {
            DolphinView *view = m_mainWindow->activeViewContainer()->view();
            view->setViewMode(mode);
            if (zoom >= 0)
                view->setZoomLevel(zoom);
        });
    };
    addView(QStringLiteral("Extra large icons"), DolphinView::IconsView, 7);
    addView(QStringLiteral("Large icons"), DolphinView::IconsView, 5);
    addView(QStringLiteral("Medium icons"), DolphinView::IconsView, 3);
    addView(QStringLiteral("Small icons"), DolphinView::IconsView, 1);
    addView(QStringLiteral("List"), DolphinView::CompactView, 1);
    addView(QStringLiteral("Details"), DolphinView::DetailsView, -1);

    QAction *sortAction = m_mainWindow->actionCollection()->action(QStringLiteral("sort"));
    if (sortAction && sortAction->menu()) {
        QMenu *sortMenu = addMenu(QStringLiteral("Sort by"));
        sortMenu->addActions(sortAction->menu()->actions());
    }
    addAction(m_mainWindow->actionCollection()->action(
        KStandardAction::name(KStandardAction::Redisplay)));
    addSeparator();

    QAction *pasteAction = m_mainWindow->actionCollection()->action(
        KStandardAction::name(KStandardAction::Paste));
    if (pasteAction) {
        pasteAction->setText(QStringLiteral("Paste"));
        addAction(pasteAction);
    }

    KNewFileMenu *newFileMenu = m_mainWindow->newFileMenu();
    newFileMenu->checkUpToDate();
    newFileMenu->setWorkingDirectory(m_baseUrl);
    newFileMenu->menu()->setTitle(QStringLiteral("New"));
    addMenu(newFileMenu->menu());
    addSeparator();

    QAction *propertiesAction = m_mainWindow->actionCollection()->action(QStringLiteral("properties"));
    addAction(propertiesAction);
}

void DolphinContextMenu::insertDefaultItemActions(const KFileItemListProperties &properties)
{
    const KActionCollection *collection = m_mainWindow->actionCollection();

    // Insert 'Cut', 'Copy', 'Copy Location' and 'Paste'
    addAction(collection->action(KStandardAction::name(KStandardAction::Cut)));
    addAction(collection->action(KStandardAction::name(KStandardAction::Copy)));
    QAction *pasteAction = createPasteAction();
    if (pasteAction) {
        addAction(pasteAction);
    }

    // Insert 'Rename'
    QAction *renameAction = collection->action(KStandardAction::name(KStandardAction::RenameFile));
    renameAction->setText(QStringLiteral("Rename"));
    addAction(renameAction);

    addSeparator();

    // Insert 'Move to Trash' and/or 'Delete'
    const bool showMoveToTrashAction = (properties.isLocal() && properties.supportsMoving());

    if (showMoveToTrashAction) {
        QAction *trashAction = m_mainWindow->actionCollection()->action(
            KStandardAction::name(KStandardAction::MoveToTrash));
        trashAction->setText(QStringLiteral("Delete"));
        addAction(trashAction);
    } else if (properties.supportsDeleting()) {
        addAction(m_mainWindow->actionCollection()->action(KStandardAction::name(KStandardAction::DeleteFile)));
    }
}

bool DolphinContextMenu::placeExists(const QUrl &url) const
{
    const KFilePlacesModel *placesModel = DolphinPlacesModelSingleton::instance().placesModel();

    QModelIndex url_index = placesModel->closestItem(url);
    return url_index.isValid() && placesModel->url(url_index).matches(url, QUrl::StripTrailingSlash);
}

QAction *DolphinContextMenu::createPasteAction()
{
    QAction *action = nullptr;
    KFileItem destItem;
    if (!m_fileInfo.isNull() && m_selectedItems.count() <= 1) {
        destItem = m_fileInfo;
    } else {
        destItem = baseFileItem();
    }

    if (!destItem.isNull() && destItem.isDir()) {
        const QMimeData *mimeData = QApplication::clipboard()->mimeData();
        bool canPaste;
        const QString text = KIO::pasteActionText(mimeData, &canPaste, destItem);
        if (canPaste) {
            if (destItem == m_fileInfo) {
                // if paste destination is a selected folder
                action = new QAction(QIcon::fromTheme(QStringLiteral("edit-paste")), text, this);
                connect(action, &QAction::triggered, m_mainWindow, &DolphinMainWindow::pasteIntoFolder);
            } else {
                action = m_mainWindow->actionCollection()->action(KStandardAction::name(KStandardAction::Paste));
            }
        }
    }

    return action;
}

KFileItemListProperties &DolphinContextMenu::selectedItemsProperties() const
{
    if (!m_selectedItemsProperties) {
        m_selectedItemsProperties = new KFileItemListProperties(m_selectedItems);
    }
    return *m_selectedItemsProperties;
}

KFileItem DolphinContextMenu::baseFileItem()
{
    if (!m_baseFileItem) {
        const DolphinView *view = m_mainWindow->activeViewContainer()->view();
        KFileItem baseItem = view->rootItem();
        if (baseItem.isNull() || baseItem.url() != m_baseUrl) {
            m_baseFileItem = new KFileItem(m_baseUrl);
        } else {
            m_baseFileItem = new KFileItem(baseItem);
        }
    }
    return *m_baseFileItem;
}

void DolphinContextMenu::addOpenWithActions()
{
    // insert 'Open With...' action or sub menu
    m_fileItemActions->insertOpenWithActionsTo(nullptr, this, QStringList{qApp->desktopFileName()});

    // For a single file, hint in "Open with" menu that middle-clicking would open it in the secondary app,
    // and shift + middle-clicking would open it in the third associated app.
    // (Unless those actions would open it as a folder in a new tab (e.g. archives).)
    const QUrl &url = DolphinView::openItemAsFolderUrl(m_fileInfo, GeneralSettings::browseThroughArchives());
    if (m_selectedItems.count() == 1 && url.isEmpty()) {
        if (QAction *openWithSubMenu = findChild<QAction *>(QStringLiteral("openWith_submenu"))) {
            Q_ASSERT(openWithSubMenu->menu());
            Q_ASSERT(!openWithSubMenu->menu()->isEmpty());

            auto *secondaryApp = openWithSubMenu->menu()->actions().first();
            // Add it like a keyboard shortcut, Qt uses \t as a separator.
            if (!secondaryApp->text().contains(QLatin1Char('\t'))) {
                secondaryApp->setText(secondaryApp->text() + QLatin1Char('\t')
                                      + i18nc("@action:inmenu Shortcut, middle click to trigger menu item, keep short", "Middle Click"));
            }

            // To add a hint for the third app, check if it is defined.
            // Note: Apart from apps, the submenu contains an empty action (separator) and "Other application" action (in this order).
            // Thus, add the hint only when there are four actions - two apps + two of the above.
            if (openWithSubMenu->menu()->actions().size() >= 4) {
                auto *thirdApp = *(openWithSubMenu->menu()->actions().begin() + 1);
                Q_ASSERT(!thirdApp->isSeparator());
                if (!thirdApp->text().contains(QLatin1Char('\t'))) {
                    thirdApp->setText(thirdApp->text() + QLatin1Char('\t')
                                      + i18nc("@action:inmenu Shortcut, shift + middle click to trigger menu item, keep short", "Shift+Middle Click"));
                }
            }
        }
    }
}

void DolphinContextMenu::addAdditionalActions(const KFileItemListProperties &props)
{
    addSeparator();

    QList<QAction *> additionalActions;
    if (props.isLocal() && props.isDirectory() && ContextMenuSettings::showOpenTerminal()) {
        additionalActions << m_mainWindow->actionCollection()->action(QStringLiteral("open_terminal_here"));
    }
    m_fileItemActions->addActionsTo(this, KFileItemActions::MenuActionSource::All, additionalActions);

    const DolphinView *view = m_mainWindow->activeViewContainer()->view();
    const QList<QAction *> versionControlActions = view->versionControlActions(m_selectedItems);
    if (!versionControlActions.isEmpty()) {
        addSeparator();
        addActions(versionControlActions);
        addSeparator();
    }
}

#include "moc_dolphincontextmenu.cpp"
