/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aero7commondialog.h"
#include "aero7dialogsearch.h"
#include "aero7computerdialog.h"
#include "aero7storage.h"
#include "aero7storagedevices.h"
#include "aero7mountwatcher.h"
#include "aero7icons.h"
#include "aero7libraries.h"
#include "aero7dialoglocation.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QActionGroup>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMessageBox>
#include <QMenu>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QStyle>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {
constexpr int PathRole = Qt::UserRole + 10;
constexpr int StorageHeadingRole = Qt::UserRole + 11;
constexpr int DeviceRole = Qt::UserRole + 12;

class NavigationTree final : public QTreeWidget
{
protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            if (currentItem()) Q_EMIT itemActivated(currentItem(), currentColumn());
            // Do not propagate navigation Enter to the default Save/Open button.
            event->accept();
            return;
        }
        QTreeWidget::keyPressEvent(event);
    }
};

class LocationEdit final : public QLineEdit
{
protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            Q_EMIT returnPressed();
            event->accept();
            return;
        }
        QLineEdit::keyPressEvent(event);
    }
};

void setViewModel(QListView *view, QAbstractItemModel *model)
{
    QItemSelectionModel *oldSelection = view->selectionModel();
    view->setModel(model);
    // This private view does not share its default selection model. Qt leaves
    // old ones alive on model replacement; repeated search/navigation leaked them.
    if (oldSelection && oldSelection != view->selectionModel() && oldSelection->parent() == view)
        delete oldSelection;
}

QTreeWidgetItem *heading(QTreeWidget *tree, const QString &text)
{
    auto *item = new QTreeWidgetItem(tree, {text});
    QFont font = item->font(0);
    font.setBold(true);
    item->setFont(0, font);
    item->setFlags(Qt::ItemIsEnabled);
    return item;
}

void location(QTreeWidgetItem *parent, const QString &text, const QString &path,
              const QIcon &icon)
{
    auto *item = new QTreeWidgetItem(parent, {text});
    item->setData(0, PathRole, path);
    item->setIcon(0, icon);
}
}

Aero7CommonDialog::Aero7CommonDialog(Mode mode, const QString &applicationId,
                                     QWidget *parent)
    : QDialog(parent), m_mode(mode), m_applicationId(applicationId)
{
    setWindowTitle(mode == Mode::SaveFile ? QStringLiteral("Save As")
                   : mode == Mode::ChooseFolder ? QStringLiteral("Select Folder")
                                                : QStringLiteral("Open"));
    setWindowIcon(Aero7Icons::icon(QStringLiteral("system-file-manager")));
    resize(820, 560);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(8, 8, 8, 8);
    outer->setSpacing(7);

    auto *navigationRow = new QHBoxLayout;
    navigationRow->setSpacing(4);
    m_back = new QPushButton(Aero7Icons::icon(QStringLiteral("back")), QString());
    m_forward = new QPushButton(Aero7Icons::icon(QStringLiteral("forward")), QString());
    m_up = new QPushButton(Aero7Icons::icon(QStringLiteral("up")), QString());
    for (QPushButton *button : {m_back, m_forward, m_up})
        button->setFixedSize(30, 27);
    navigationRow->addWidget(m_back);
    navigationRow->addWidget(m_forward);
    navigationRow->addWidget(m_up);
    m_pathEdit = new LocationEdit;
    m_pathEdit->setObjectName(QStringLiteral("location"));
    m_pathEdit->setPlaceholderText(QStringLiteral("Location"));
    m_pathEdit->setAccessibleName(QStringLiteral("Location"));
    navigationRow->addWidget(new Aero7DialogLocation::Bar(m_pathEdit, this,
        [this](const QString &path) { setDirectory(path); }), 1);
    m_searchEdit = new QLineEdit;
    m_searchEdit->setObjectName(QStringLiteral("search"));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setPlaceholderText(QStringLiteral("Search"));
    m_searchEdit->setMaximumWidth(230);
    navigationRow->addWidget(m_searchEdit);
    outer->addLayout(navigationRow);

    auto *commandBar = new QHBoxLayout;
    auto *organize = new QPushButton(QStringLiteral("Organize"));
    organize->setObjectName(QStringLiteral("organize"));
    organize->setFlat(true);
    auto *newFolder = new QPushButton(QStringLiteral("New folder"));
    newFolder->setFlat(true);
    connect(newFolder, &QPushButton::clicked, this, [this]() {
        if (!saveLocationAvailable()) {
            QMessageBox::warning(this, QStringLiteral("New Folder"),
                                 QStringLiteral("The location is no longer available. Select another folder."));
            return;
        }
        const QString libraryDestination = Aero7Libraries::instance().saveLocationForPath(m_currentDirectory);
        const QString directory = libraryDestination.isEmpty() ? m_currentDirectory : libraryDestination;
        QString base = QStringLiteral("New folder");
        QString path = QDir(directory).filePath(base);
        int suffix = 2;
        while (QFileInfo::exists(path))
            path = QDir(directory).filePath(QStringLiteral("%1 (%2)").arg(base).arg(suffix++));
        // Do not recreate a missing/unmounted library destination implicitly.
        if (!QDir(directory).mkdir(QFileInfo(path).fileName())) {
            QMessageBox::warning(this, QStringLiteral("New Folder"),
                                 QStringLiteral("The folder could not be created."));
            return;
        }
        if (!libraryDestination.isEmpty()) {
            QString error;
            if (!Aero7Libraries::instance().refresh(&error))
                QMessageBox::warning(this, QStringLiteral("Libraries"), error);
        }
    });
    commandBar->addWidget(organize);
    commandBar->addWidget(newFolder);
    commandBar->addStretch(1);
    auto *searchStatus = new QLabel;
    searchStatus->setObjectName(QStringLiteral("searchStatus"));
    searchStatus->hide();
    commandBar->addWidget(searchStatus);
    outer->addLayout(commandBar);

    auto *splitter = new QSplitter;
    splitter->setChildrenCollapsible(false);
    m_navigation = new NavigationTree;
    m_navigation->setHeaderHidden(true);
    m_navigation->setIndentation(13);
    m_navigation->setMaximumWidth(230);
    splitter->addWidget(m_navigation);

    m_fileModel = new QFileSystemModel(this);
    m_fileModel->setFilter((mode == Mode::ChooseFolder ? QDir::Dirs : QDir::AllEntries)
                          | QDir::AllDirs | QDir::NoDotAndDotDot);
    m_fileModel->setReadOnly(false);
    m_searchModel = new QStandardItemModel(this);
    auto *search = new Aero7DialogSearch(m_searchModel, searchStatus, this);
    connect(this, &QDialog::finished, search, &Aero7DialogSearch::cancel);
    m_view = new QListView;
    setViewModel(m_view, m_fileModel);
    m_view->setViewMode(QListView::IconMode);
    m_view->setResizeMode(QListView::Adjust);
    m_view->setMovement(QListView::Static);
    m_view->setIconSize(QSize(48, 48));
    m_view->setGridSize(QSize(116, 78));
    m_view->setSelectionMode(mode == Mode::OpenFiles
                                 ? QAbstractItemView::ExtendedSelection
                                 : QAbstractItemView::SingleSelection);
    auto *fileArea = new QStackedWidget;
    fileArea->setObjectName(QStringLiteral("fileArea"));
    fileArea->addWidget(m_view);
    auto *unavailable = new QLabel(QStringLiteral("This location is no longer available.\nSelect another folder to continue."));
    unavailable->setObjectName(QStringLiteral("unavailableLocation"));
    unavailable->setAlignment(Qt::AlignCenter);
    unavailable->setWordWrap(true);
    unavailable->setStyleSheet(QStringLiteral("background: white; color: #555; padding: 24px;"));
    fileArea->addWidget(unavailable);
    splitter->addWidget(fileArea);
    splitter->setStretchFactor(1, 1);
    outer->addWidget(splitter, 1);

    auto *organizeMenu = new QMenu(organize);
    organize->setMenu(organizeMenu);
    organizeMenu->addAction(QStringLiteral("New folder"), newFolder, &QPushButton::click);
    auto *selectAll = organizeMenu->addAction(QStringLiteral("Select all"), m_view, &QListView::selectAll);
    selectAll->setEnabled(mode == Mode::OpenFiles);
    auto *layoutMenu = organizeMenu->addMenu(QStringLiteral("Layout"));
    auto *views = new QActionGroup(layoutMenu);
    for (const bool icons : {true, false}) {
        auto *action = layoutMenu->addAction(icons ? QStringLiteral("Large icons") : QStringLiteral("List"));
        action->setCheckable(true);
        action->setChecked(icons);
        views->addAction(action);
        connect(action, &QAction::triggered, this, [this, icons] {
            m_view->setViewMode(icons ? QListView::IconMode : QListView::ListMode);
            m_view->setIconSize(icons ? QSize(48, 48) : QSize(16, 16));
            m_view->setGridSize(icons ? QSize(116, 78) : QSize());
        });
    }
    auto *hidden = organizeMenu->addAction(QStringLiteral("Show hidden files"));
    hidden->setCheckable(true);
    hidden->setObjectName(QStringLiteral("showHidden"));
    connect(hidden, &QAction::toggled, this, [this](bool enabled) {
        auto filters = m_fileModel->filter();
        filters.setFlag(QDir::Hidden, enabled);
        m_fileModel->setFilter(filters);
        if (!m_searchEdit->text().trimmed().isEmpty()) runSearch(m_searchEdit->text());
    });
    connect(newFolder, &QPushButton::clicked, this, [this] {
        if (!m_searchEdit->text().trimmed().isEmpty()) runSearch(m_searchEdit->text());
    });

    auto *bottom = new QGridLayout;
    m_fileName = new QLineEdit;
    m_fileName->setObjectName(QStringLiteral("fileName"));
    m_fileType = new QComboBox;
    m_fileType->setObjectName(QStringLiteral("fileType"));
    // Applications may offer hundreds of image extensions in one filter. The
    // label must not become the dialog's minimum width on smaller screens.
    m_fileType->setMinimumContentsLength(28);
    m_fileType->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    bottom->addWidget(new QLabel(mode == Mode::ChooseFolder
                                     ? QStringLiteral("Folder:")
                                     : QStringLiteral("File name:")), 0, 0);
    bottom->addWidget(m_fileName, 0, 1);
    bottom->addWidget(new QLabel(mode == Mode::SaveFile
                                     ? QStringLiteral("Save as type:")
                                     : QStringLiteral("File type:")), 1, 0);
    bottom->addWidget(m_fileType, 1, 1);
    auto *buttons = new QDialogButtonBox;
    m_acceptButton = buttons->addButton(mode == Mode::SaveFile ? QStringLiteral("Save")
                                               : mode == Mode::ChooseFolder ? QStringLiteral("Select Folder")
                                                                            : QStringLiteral("Open"),
                                         QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    bottom->addWidget(buttons, 0, 2, 2, 1);
    outer->addLayout(bottom);

    auto *devices = new Aero7StorageDevices(this);
    connect(devices, &Aero7StorageDevices::changed, this, &Aero7CommonDialog::refreshStorageNavigation);
    connect(devices, &Aero7StorageDevices::opened, this, [this](const QString &root) { setDirectory(root); });
    connect(devices, &Aero7StorageDevices::failed, this, [this](const QString &message) {
        QMessageBox::warning(this, QStringLiteral("Open drive"), message);
    });
    connect(this, &QDialog::finished, devices, &Aero7StorageDevices::cancelPending);
    buildNavigation();
    const auto activateLocation = [this, devices](QTreeWidgetItem *item) {
        const QString id = item->data(0, DeviceRole).toString();
        const QString path = item->data(0, PathRole).toString();
        if (!id.isEmpty()) devices->openDevice(id);
        else if (!path.isEmpty()) setDirectory(path);
    };
    connect(m_navigation, &QTreeWidget::itemClicked, this, activateLocation);
    connect(m_navigation, &QTreeWidget::itemActivated, this, activateLocation);
    connect(m_view, &QListView::doubleClicked, this, &Aero7CommonDialog::activateIndex);
    connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &Aero7CommonDialog::selectionChanged);
    connect(m_back, &QPushButton::clicked, this, &Aero7CommonDialog::navigateBack);
    connect(m_forward, &QPushButton::clicked, this, &Aero7CommonDialog::navigateForward);
    connect(m_up, &QPushButton::clicked, this, &Aero7CommonDialog::navigateUp);
    connect(m_pathEdit, &QLineEdit::returnPressed, this,
            [this]() { setDirectory(m_pathEdit->text()); });
    connect(m_searchEdit, &QLineEdit::textChanged, this, &Aero7CommonDialog::runSearch);
    connect(buttons, &QDialogButtonBox::accepted, this, &Aero7CommonDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_fileName, &QLineEdit::textChanged, this, &Aero7CommonDialog::updateButtons);
    connect(m_fileName, &QLineEdit::textEdited, this, [this]() {
        // An edited filename supersedes the old selection, including search
        // results whose basename belongs to a different directory.
        if (m_view->selectionModel())
            m_view->selectionModel()->clearSelection();
        updateButtons();
    });
    connect(m_fileType, &QComboBox::currentTextChanged, this, &Aero7CommonDialog::applyNameFilter);

    setNameFilters({QStringLiteral("All files (*)")});
    restoreState();
    new Aero7Storage::MountWatcher(this, [this] { refreshStorageNavigation(); });
    // Establish the initial keyboard target before showing the dialog. Do not
    // refocus on model refresh/navigation: that would steal focus from a user
    // typing a search or using the location bar.
    if (mode == Mode::ChooseFolder)
        m_view->setFocus(Qt::OtherFocusReason);
    else
        m_fileName->setFocus(Qt::OtherFocusReason);
}

void Aero7CommonDialog::buildNavigation()
{
    auto *favorites = heading(m_navigation, QStringLiteral("Favorites"));
    location(favorites, QStringLiteral("Desktop"),
             QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
             QIcon::fromTheme(QStringLiteral("user-desktop")));
    location(favorites, QStringLiteral("Downloads"),
             QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),
             QIcon::fromTheme(QStringLiteral("folder-download")));
    location(favorites, QStringLiteral("Recent Places"), QDir::homePath(),
             QIcon::fromTheme(QStringLiteral("document-open-recent")));

    auto *libraries = heading(m_navigation, QStringLiteral("Libraries"));
    for (const Aero7Library &library : Aero7Libraries::instance().libraries()) {
        if (library.shownInNavigationPane)
            location(libraries, library.name,
                     Aero7Libraries::instance().materializedPath(library.id),
                     QIcon::fromTheme(QStringLiteral("folder-%1").arg(library.id)));
    }

    auto *computer = heading(m_navigation, QStringLiteral("Computer"));
    computer->setData(0, StorageHeadingRole, true);
    refreshStorageNavigation();
    auto *network = heading(m_navigation, QStringLiteral("Network"));
    const QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    bool foundNetwork = false;
    for (const QString &candidate : {QDir(runtime).filePath(QStringLiteral("kio-fuse")),
                                     QDir(runtime).filePath(QStringLiteral("gvfs"))}) {
        if (QFileInfo(candidate).isDir()) {
            location(network, QFileInfo(candidate).fileName(), candidate,
                     QIcon::fromTheme(QStringLiteral("network-workgroup")));
            foundNetwork = true;
        }
    }
    if (!foundNetwork)
        new QTreeWidgetItem(network, {QStringLiteral("No network locations available")});
    m_navigation->expandAll();
}

void Aero7CommonDialog::refreshStorageNavigation()
{
    QTreeWidgetItem *computer = nullptr;
    for (int index = 0; index < m_navigation->topLevelItemCount(); ++index) {
        auto *item = m_navigation->topLevelItem(index);
        if (item->data(0, StorageHeadingRole).toBool()) { computer = item; break; }
    }
    if (!computer) return;
    const QSignalBlocker blocker(m_navigation);
    struct Location { QString root; QString id; QString name; QString icon; };
    QList<Location> entries;
    for (const auto &entry : Aero7Storage::mounted())
        entries.append({entry.root, {}, entry.name, entry.icon});
    if (auto *devices = findChild<Aero7StorageDevices *>()) {
        for (const auto &device : devices->unmounted())
            entries.append({{}, device.id, device.name, device.icon});
    }
    const auto *current = m_navigation->currentItem();
    const bool hadDriveSelection = current && current->parent() == computer;
    const QString selectedRoot = hadDriveSelection ? current->data(0, PathRole).toString() : QString();
    const QString selectedId = hadDriveSelection ? current->data(0, DeviceRole).toString() : QString();
    // Only update drive children. Do not rebuild Favorites/Libraries, navigate,
    // restart a search, clear the filename, or reset the dialog's history.
    for (int index = computer->childCount() - 1; index >= 0; --index) {
        const QString root = computer->child(index)->data(0, PathRole).toString();
        const QString id = computer->child(index)->data(0, DeviceRole).toString();
        const bool present = std::any_of(entries.cbegin(), entries.cend(), [&root, &id](const auto &entry) {
            return entry.root == root && entry.id == id;
        });
        if (!present) delete computer->takeChild(index);
    }
    QTreeWidgetItem *selected = nullptr;
    for (int index = 0; index < entries.size(); ++index) {
        const auto &entry = entries.at(index);
        QTreeWidgetItem *item = nullptr;
        for (int child = 0; child < computer->childCount(); ++child) {
            if (computer->child(child)->data(0, PathRole).toString() == entry.root
                && computer->child(child)->data(0, DeviceRole).toString() == entry.id) {
                item = computer->child(child);
                if (child != index) computer->insertChild(index, computer->takeChild(child));
                break;
            }
        }
        if (!item) {
            item = new QTreeWidgetItem;
            computer->insertChild(index, item);
            item->setData(0, PathRole, entry.root);
            item->setData(0, DeviceRole, entry.id);
        }
        item->setText(0, entry.name);
        item->setIcon(0, QIcon::fromTheme(entry.icon));
        if (entry.root == selectedRoot && entry.id == selectedId) selected = item;
    }
    if (hadDriveSelection) m_navigation->setCurrentItem(selected);
    if (!m_currentDirectory.isEmpty()) {
        const QStorageInfo backing(m_currentDirectory);
        const bool available = QFileInfo(m_currentDirectory).isDir()
            && backing.isValid() && backing.isReady()
            && backing.rootPath() == property("_aero7BrowseRoot").toString()
            && backing.device() == property("_aero7BrowseDevice").toByteArray();
        if (!available && !property("_aero7LocationUnavailable").toBool()) {
            setProperty("_aero7LocationUnavailable", true);
            findChild<Aero7DialogSearch *>()->cancel();
            // QFileSystemModel can retain cached rows after the mount inode
            // disappears. Hide those rows instead of presenting phantom files.
            if (auto *selection = m_view->selectionModel()) {
                const QSignalBlocker selectionBlocker(selection);
                selection->clearSelection();
            }
            findChild<QStackedWidget *>(QStringLiteral("fileArea"))->setCurrentIndex(1);
            m_searchEdit->setEnabled(false);
        }
    }
    updateButtons();
}

void Aero7CommonDialog::setNameFilters(const QStringList &filters)
{
    const QString previous = m_fileType->currentText();
    const QSignalBlocker blocker(m_fileType);
    m_fileType->clear();
    m_fileType->addItems(filters.isEmpty() ? QStringList{QStringLiteral("All files (*)")} : filters);
    int selected = m_fileType->findText(previous);
    if (selected < 0)
        selected = m_fileType->findText(m_restoredFilter);
    m_fileType->setCurrentIndex(selected >= 0 ? selected : 0);
    applyNameFilter(m_fileType->currentText());
}

void Aero7CommonDialog::applyNameFilter(const QString &filter)
{
    const qsizetype left = filter.lastIndexOf(QLatin1Char('('));
    const qsizetype right = filter.lastIndexOf(QLatin1Char(')'));
    const QString patterns = left >= 0 && right > left ? filter.mid(left + 1, right - left - 1)
                                                      : QStringLiteral("*");
    m_fileModel->setNameFilters(patterns.split(QLatin1Char(' '), Qt::SkipEmptyParts));
    m_fileModel->setNameFilterDisables(false);
    if (!m_searchEdit->text().trimmed().isEmpty()) runSearch(m_searchEdit->text());
    if (m_appliedFilter != filter) {
        m_appliedFilter = filter;
        Q_EMIT filterChanged(filter);
    }
}

QString Aero7CommonDialog::selectedNameFilter() const
{
    return m_fileType->currentText();
}

void Aero7CommonDialog::selectNameFilter(const QString &filter)
{
    const int index = m_fileType->findText(filter);
    if (index >= 0)
        m_fileType->setCurrentIndex(index);
}

void Aero7CommonDialog::setCustomWidget(QWidget *widget)
{
    if (widget == m_customWidget || widget == this || (widget && widget->isAncestorOf(this)))
        return;
    delete m_customWidget.data();
    m_customWidget = widget;
    if (widget) {
        auto *outer = qobject_cast<QVBoxLayout *>(layout());
        outer->insertWidget(outer->count() - 1, widget);
        widget->show();
    }
}

void Aero7CommonDialog::setSuggestedFileName(const QString &name)
{
    m_fileName->setText(name);
    m_fileName->selectAll();
}

void Aero7CommonDialog::setDefaultSuffix(const QString &suffix)
{
    m_defaultSuffix = suffix;
}

void Aero7CommonDialog::setInitialDirectory(const QString &path)
{
    if (QFileInfo(path).isDir())
        setDirectory(path);
}

QStringList Aero7CommonDialog::selectedFiles() const
{
    return m_result;
}

void Aero7CommonDialog::setDirectory(const QString &input, bool addHistory)
{
    QString path = QDir::cleanPath(QFileInfo(input).absoluteFilePath());
    if (!QFileInfo(path).isDir())
        return;
    if (auto *devices = findChild<Aero7StorageDevices *>()) devices->cancelPending();
    m_currentDirectory = path;
    const QStorageInfo browsing(path);
    setProperty("_aero7BrowseRoot", browsing.rootPath());
    setProperty("_aero7BrowseDevice", browsing.device());
    setProperty("_aero7LocationUnavailable", false);
    findChild<QStackedWidget *>(QStringLiteral("fileArea"))->setCurrentIndex(0);
    m_searchEdit->setEnabled(true);
    const QString libraryDestination = Aero7Libraries::instance().saveLocationForPath(path);
    const QStorageInfo storage(libraryDestination.isEmpty() ? path : libraryDestination);
    // Dynamic properties retain private state without changing the exported
    // class size expected by already-installed shared-dialog consumers.
    setProperty("_aero7SaveRoot", storage.rootPath());
    setProperty("_aero7SaveDevice", storage.device());
    static_cast<Aero7DialogLocation::Bar *>(findChild<QWidget *>(QStringLiteral("locationBreadcrumbs")))->setLocation(path);
    findChild<Aero7DialogSearch *>()->cancel();
    {
        const QSignalBlocker blocker(m_searchEdit);
        m_searchEdit->clear();
    }
    setViewModel(m_view, m_fileModel);
    const QModelIndex root = m_fileModel->setRootPath(path);
    m_view->setRootIndex(root);
    connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &Aero7CommonDialog::selectionChanged, Qt::UniqueConnection);
    if (addHistory) {
        while (m_history.size() > m_historyIndex + 1)
            m_history.removeLast();
        if (m_history.isEmpty() || m_history.constLast() != path)
            m_history.append(path);
        m_historyIndex = m_history.size() - 1;
    }
    updateButtons();
}

void Aero7CommonDialog::navigateBack()
{
    if (m_historyIndex > 0)
        setDirectory(m_history.at(--m_historyIndex), false);
}

void Aero7CommonDialog::navigateForward()
{
    if (m_historyIndex + 1 < m_history.size())
        setDirectory(m_history.at(++m_historyIndex), false);
}

void Aero7CommonDialog::navigateUp()
{
    QDir directory(m_currentDirectory);
    if (directory.cdUp())
        setDirectory(directory.absolutePath());
}

QString Aero7CommonDialog::pathForIndex(const QModelIndex &index) const
{
    if (!index.isValid())
        return {};
    return m_view->model() == m_fileModel
        ? m_fileModel->filePath(index)
        : index.data(PathRole).toString();
}

void Aero7CommonDialog::activateIndex(const QModelIndex &index)
{
    const QString path = pathForIndex(index);
    if (QFileInfo(path).isDir())
        setDirectory(path);
    else if (m_mode != Mode::ChooseFolder)
        accept();
}

void Aero7CommonDialog::selectionChanged()
{
    const QModelIndexList selected = m_view->selectionModel()->selectedIndexes();
    if (selected.size() == 1) {
        const QString path = pathForIndex(selected.constFirst());
        if (m_mode != Mode::ChooseFolder || QFileInfo(path).isDir())
            m_fileName->setText(QFileInfo(path).fileName());
    }
    updateButtons();
}

void Aero7CommonDialog::runSearch(const QString &query)
{
    if (property("_aero7LocationUnavailable").toBool()) return;
    const QString trimmed = query.trimmed();
    {
        const QSignalBlocker blocker(m_searchEdit);
        m_searchEdit->setText(query);
    }
    if (trimmed.isEmpty()) {
        setDirectory(m_currentDirectory, false);
        return;
    }
    findChild<Aero7DialogSearch *>()->cancel();
    m_searchModel->clear();
    m_searchModel->setHorizontalHeaderLabels({QStringLiteral("Name")});
    QStringList roots;
    const QString libraryId = Aero7Libraries::instance().libraryIdForPath(m_currentDirectory);
    const bool libraryRoot = !libraryId.isEmpty() && QDir::cleanPath(m_currentDirectory)
        == QDir::cleanPath(Aero7Libraries::instance().materializedPath(libraryId));
    roots = libraryRoot ? Aero7Libraries::instance().library(libraryId).locations
                        : QStringList{m_currentDirectory};
    setViewModel(m_view, m_searchModel);
    m_view->setRootIndex({});
    connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &Aero7CommonDialog::selectionChanged, Qt::UniqueConnection);
    updateButtons();
    findChild<Aero7DialogSearch *>()->submit(roots, trimmed, m_fileModel->nameFilters(),
        m_mode == Mode::ChooseFolder, m_fileModel->filter().testFlag(QDir::Hidden));
}

bool Aero7CommonDialog::saveLocationAvailable() const
{
    const QString libraryDestination = Aero7Libraries::instance().saveLocationForPath(m_currentDirectory);
    const QString directory = libraryDestination.isEmpty() ? m_currentDirectory : libraryDestination;
    if (directory.isEmpty() || !QFileInfo(directory).isDir()) return false;
    const QStorageInfo storage(directory);
    // An unmounted drive can leave an ordinary empty mount-point directory.
    // Never silently redirect a save/new-folder into the underlying system disk.
    return storage.isValid() && storage.isReady()
        && storage.rootPath() == property("_aero7SaveRoot").toString()
        && storage.device() == property("_aero7SaveDevice").toByteArray();
}

void Aero7CommonDialog::updateButtons()
{
    m_back->setEnabled(m_historyIndex > 0);
    m_forward->setEnabled(m_historyIndex + 1 < m_history.size());
    m_up->setEnabled(QDir(m_currentDirectory).absolutePath() != QDir::rootPath());
    if (property("_aero7LocationUnavailable").toBool()) {
        m_acceptButton->setEnabled(false);
        return;
    }
    if (m_mode == Mode::SaveFile)
        m_acceptButton->setEnabled(!m_fileName->text().trimmed().isEmpty() && saveLocationAvailable());
    else if (m_mode == Mode::ChooseFolder)
        m_acceptButton->setEnabled(QFileInfo(m_currentDirectory).isDir());
    else {
        const QString name = m_fileName->text();
        const QFileInfo typed(QDir(m_currentDirectory).filePath(name));
        const bool validTyped = !name.trimmed().isEmpty() && (typed.isFile() || typed.isDir());
        m_acceptButton->setEnabled(validTyped || (m_view->selectionModel()
                                      && !m_view->selectionModel()->selectedIndexes().isEmpty()));
    }
}

void Aero7CommonDialog::accept()
{
    if (property("_aero7LocationUnavailable").toBool()) {
        QMessageBox::warning(this, windowTitle(),
            QStringLiteral("The location is no longer available. Select another folder."));
        return;
    }
    QStringList paths;
    if (m_mode == Mode::ChooseFolder) {
        const QModelIndexList selected = m_view->selectionModel()->selectedIndexes();
        const QString selectedPath = selected.isEmpty() ? QString() : pathForIndex(selected.constFirst());
        paths = {QFileInfo(selectedPath).isDir() ? selectedPath : m_currentDirectory};
        if (!QFileInfo(paths.constFirst()).isDir()) return;
    } else if (m_mode == Mode::SaveFile) {
        QString directory = Aero7Libraries::instance().saveLocationForPath(m_currentDirectory);
        if (directory.isEmpty())
            directory = m_currentDirectory;
        QString name = m_fileName->text().trimmed();
        // Keyboard activation must enforce the same invariant as the button.
        // Otherwise an empty name with a default suffix becomes a hidden file.
        if (name.isEmpty())
            return;
        if (!saveLocationAvailable()) {
            QMessageBox::warning(this, QStringLiteral("Save As"),
                                 QStringLiteral("The location is no longer available. Select another folder."));
            return;
        }
        const QString typedPath = QDir(directory).filePath(name);
        if (QFileInfo(typedPath).isDir()) {
            setDirectory(typedPath);
            m_fileName->clear();
            return;
        }
        QString suffix = m_defaultSuffix;
        // Prefer the first concrete extension of the selected format. Generic
        // wildcard filters retain the caller's default; explicit names stay intact.
        const QRegularExpression extension(QStringLiteral("^\\*\\.([A-Za-z0-9][A-Za-z0-9._-]*)$"));
        for (const QString &pattern : m_fileModel->nameFilters()) {
            const auto match = extension.match(pattern);
            if (match.hasMatch()) {
                suffix = match.captured(1);
                break;
            }
        }
        if (!suffix.isEmpty() && QFileInfo(name).suffix().isEmpty())
            name += QLatin1Char('.') + suffix;
        const QString path = QDir(directory).filePath(name);
        if (QFileInfo::exists(path)) {
            const auto answer = QMessageBox::question(
                this, QStringLiteral("Confirm Save As"),
                QStringLiteral("%1 already exists.\n\nDo you want to replace it?").arg(name),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (answer != QMessageBox::Yes)
                return;
        }
        paths = {path};
    } else {
        for (const QModelIndex &index : m_view->selectionModel()->selectedIndexes()) {
            const QString path = pathForIndex(index);
            if (QFileInfo(path).isFile())
                paths.append(path);
        }
        if (paths.isEmpty() && !m_fileName->text().trimmed().isEmpty()) {
            const QString path = QDir(m_currentDirectory).filePath(m_fileName->text());
            if (QFileInfo(path).isDir()) {
                setDirectory(path);
                m_fileName->clear();
                return;
            }
            if (QFileInfo(path).isFile())
                paths.append(path);
        }
        if (m_mode == Mode::OpenFile && paths.size() > 1)
            paths = {paths.constFirst()};
        if (paths.isEmpty())
            return;
    }
    m_result = paths;
    persistState();
    QDialog::accept();
}

QString Aero7CommonDialog::saveStateGroup() const
{
    return QStringLiteral("Applications/%1").arg(
        m_applicationId.isEmpty() ? QStringLiteral("unknown") : m_applicationId);
}

void Aero7CommonDialog::restoreState()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("Aero7"), QStringLiteral("CommonItemDialog"));
    settings.beginGroup(saveStateGroup());
    const QString initial = settings.value(QStringLiteral("lastFolder"),
                                            Aero7Libraries::instance().materializedPath(QStringLiteral("documents"))).toString();
    const QSize savedSize = settings.value(QStringLiteral("windowSize"), size()).toSize();
    m_restoredFilter = settings.value(QStringLiteral("fileType")).toString();
    settings.endGroup();
    resize(savedSize.expandedTo(QSize(640, 420)));
    setDirectory(QFileInfo(initial).isDir() ? initial : QDir::homePath());
    const int filterIndex = m_fileType->findText(m_restoredFilter);
    if (filterIndex >= 0)
        m_fileType->setCurrentIndex(filterIndex);
}

void Aero7CommonDialog::persistState() const
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("Aero7"), QStringLiteral("CommonItemDialog"));
    settings.beginGroup(saveStateGroup());
    settings.setValue(QStringLiteral("lastFolder"), m_currentDirectory);
    settings.setValue(QStringLiteral("windowSize"), size());
    settings.setValue(QStringLiteral("fileType"), m_fileType->currentText());
    settings.endGroup();
}
