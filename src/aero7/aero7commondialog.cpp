/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aero7commondialog.h"
#include "aero7icons.h"
#include "aero7libraries.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDirIterator>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QStyle>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {
constexpr int PathRole = Qt::UserRole + 10;

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
    m_pathEdit = new QLineEdit;
    m_pathEdit->setPlaceholderText(QStringLiteral("Location"));
    navigationRow->addWidget(m_pathEdit, 1);
    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText(QStringLiteral("Search"));
    m_searchEdit->setMaximumWidth(230);
    navigationRow->addWidget(m_searchEdit);
    outer->addLayout(navigationRow);

    auto *commandBar = new QHBoxLayout;
    auto *organize = new QPushButton(QStringLiteral("Organize ▾"));
    organize->setFlat(true);
    auto *newFolder = new QPushButton(QStringLiteral("New folder"));
    newFolder->setFlat(true);
    connect(newFolder, &QPushButton::clicked, this, [this]() {
        QString base = QStringLiteral("New folder");
        QString path = QDir(m_currentDirectory).filePath(base);
        int suffix = 2;
        while (QFileInfo::exists(path))
            path = QDir(m_currentDirectory).filePath(QStringLiteral("%1 (%2)").arg(base).arg(suffix++));
        if (!QDir().mkpath(path))
            QMessageBox::warning(this, QStringLiteral("New Folder"),
                                 QStringLiteral("The folder could not be created."));
    });
    commandBar->addWidget(organize);
    commandBar->addWidget(newFolder);
    commandBar->addStretch(1);
    outer->addLayout(commandBar);

    auto *splitter = new QSplitter;
    splitter->setChildrenCollapsible(false);
    m_navigation = new QTreeWidget;
    m_navigation->setHeaderHidden(true);
    m_navigation->setIndentation(13);
    m_navigation->setMaximumWidth(230);
    splitter->addWidget(m_navigation);

    m_fileModel = new QFileSystemModel(this);
    m_fileModel->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot);
    m_fileModel->setReadOnly(false);
    m_searchModel = new QStandardItemModel(this);
    m_view = new QListView;
    m_view->setModel(m_fileModel);
    m_view->setViewMode(QListView::IconMode);
    m_view->setResizeMode(QListView::Adjust);
    m_view->setMovement(QListView::Static);
    m_view->setIconSize(QSize(48, 48));
    m_view->setGridSize(QSize(116, 78));
    m_view->setSelectionMode(mode == Mode::OpenFiles
                                 ? QAbstractItemView::ExtendedSelection
                                 : QAbstractItemView::SingleSelection);
    splitter->addWidget(m_view);
    splitter->setStretchFactor(1, 1);
    outer->addWidget(splitter, 1);

    auto *bottom = new QGridLayout;
    m_fileName = new QLineEdit;
    m_fileType = new QComboBox;
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

    buildNavigation();
    connect(m_navigation, &QTreeWidget::itemActivated, this,
            [this](QTreeWidgetItem *item) {
                const QString path = item->data(0, PathRole).toString();
                if (!path.isEmpty())
                    setDirectory(path);
            });
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

    setNameFilters({QStringLiteral("All files (*)")});
    restoreState();
}

void Aero7CommonDialog::buildNavigation()
{
    QFileIconProvider icons;
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
    for (const QStorageInfo &storage : QStorageInfo::mountedVolumes()) {
        if (!storage.isValid() || !storage.isReady() || storage.bytesTotal() <= 0)
            continue;
        QString name = storage.displayName().trimmed();
        if (name.isEmpty())
            name = storage.rootPath();
        location(computer, name, storage.rootPath(), icons.icon(QFileIconProvider::Drive));
    }
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

void Aero7CommonDialog::setNameFilters(const QStringList &filters)
{
    m_fileType->clear();
    m_fileType->addItems(filters.isEmpty() ? QStringList{QStringLiteral("All files (*)")} : filters);
    connect(m_fileType, &QComboBox::currentTextChanged, this, [this](const QString &filter) {
        const qsizetype left = filter.indexOf(QLatin1Char('('));
        const qsizetype right = filter.lastIndexOf(QLatin1Char(')'));
        const QString patterns = left >= 0 && right > left ? filter.mid(left + 1, right - left - 1)
                                                          : QStringLiteral("*");
        m_fileModel->setNameFilters(patterns.split(QLatin1Char(' '), Qt::SkipEmptyParts));
        m_fileModel->setNameFilterDisables(false);
    });
    Q_EMIT m_fileType->currentTextChanged(m_fileType->currentText());
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
    m_currentDirectory = path;
    m_pathEdit->setText(path);
    m_searchEdit->clear();
    m_view->setModel(m_fileModel);
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
    if (query.trimmed().isEmpty()) {
        setDirectory(m_currentDirectory, false);
        return;
    }
    m_searchModel->clear();
    m_searchModel->setHorizontalHeaderLabels({QStringLiteral("Name")});
    QStringList roots;
    const QString libraryId = Aero7Libraries::instance().libraryIdForPath(m_currentDirectory);
    roots = libraryId.isEmpty() ? QStringList{m_currentDirectory}
                                : Aero7Libraries::instance().searchRoots(libraryId);
    QFileIconProvider icons;
    int count = 0;
    for (const QString &root : roots) {
        QDirIterator iterator(root, QDir::AllEntries | QDir::NoDotAndDotDot,
                              QDirIterator::Subdirectories);
        while (iterator.hasNext() && count < 1000) {
            const QString path = iterator.next();
            const QFileInfo info = iterator.fileInfo();
            if (!info.fileName().contains(query, Qt::CaseInsensitive))
                continue;
            auto *item = new QStandardItem(icons.icon(info), info.fileName());
            item->setData(path, PathRole);
            item->setToolTip(path);
            m_searchModel->appendRow(item);
            ++count;
        }
    }
    m_view->setModel(m_searchModel);
    m_view->setRootIndex({});
    connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &Aero7CommonDialog::selectionChanged, Qt::UniqueConnection);
}

void Aero7CommonDialog::updateButtons()
{
    m_back->setEnabled(m_historyIndex > 0);
    m_forward->setEnabled(m_historyIndex + 1 < m_history.size());
    m_up->setEnabled(QDir(m_currentDirectory).absolutePath() != QDir::rootPath());
    if (m_mode == Mode::SaveFile)
        m_acceptButton->setEnabled(!m_fileName->text().trimmed().isEmpty());
    else if (m_mode == Mode::ChooseFolder)
        m_acceptButton->setEnabled(QFileInfo(m_currentDirectory).isDir());
    else
        m_acceptButton->setEnabled(m_view->selectionModel()
                                      && !m_view->selectionModel()->selectedIndexes().isEmpty());
}

void Aero7CommonDialog::accept()
{
    QStringList paths;
    if (m_mode == Mode::ChooseFolder) {
        const QModelIndexList selected = m_view->selectionModel()->selectedIndexes();
        const QString selectedPath = selected.isEmpty() ? QString() : pathForIndex(selected.constFirst());
        paths = {QFileInfo(selectedPath).isDir() ? selectedPath : m_currentDirectory};
    } else if (m_mode == Mode::SaveFile) {
        QString directory = Aero7Libraries::instance().saveLocationForPath(m_currentDirectory);
        if (directory.isEmpty())
            directory = m_currentDirectory;
        QString name = m_fileName->text().trimmed();
        if (!m_defaultSuffix.isEmpty() && QFileInfo(name).suffix().isEmpty())
            name += QLatin1Char('.') + m_defaultSuffix;
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
    const QString savedFilter = settings.value(QStringLiteral("fileType")).toString();
    settings.endGroup();
    resize(savedSize.expandedTo(QSize(640, 420)));
    setDirectory(QFileInfo(initial).isDir() ? initial : QDir::homePath());
    const int filterIndex = m_fileType->findText(savedFilter);
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
