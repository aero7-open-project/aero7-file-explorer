/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aero7properties.h"
#include "aero7commondialog.h"
#include "aero7libraries.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMessageBox>
#include <QMimeDatabase>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QTabWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

#include <sys/stat.h>

namespace {
QString humanSize(qint64 bytes)
{
    const char *units[] = {"bytes", "KB", "MB", "GB", "TB"};
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < 4) {
        value /= 1024.0;
        ++unit;
    }
    return unit == 0 ? QStringLiteral("%1 bytes").arg(bytes)
                     : QStringLiteral("%1 %2 (%3 bytes)").arg(value, 0, 'f', 1)
                           .arg(QString::fromLatin1(units[unit])).arg(bytes);
}

QLabel *valueLabel(const QString &value)
{
    auto *label = new QLabel(value);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    // Paths and sizes must remain on one line. Word-wrapping path segments in
    // a QFormLayout can hide the final component underneath the following row.
    label->setWordWrap(false);
    return label;
}

QString accessText(QFile::Permissions permissions)
{
    QStringList values;
    if (permissions.testFlag(QFile::ReadOwner)) values << QStringLiteral("Read");
    if (permissions.testFlag(QFile::WriteOwner)) values << QStringLiteral("Write");
    if (permissions.testFlag(QFile::ExeOwner)) values << QStringLiteral("Execute");
    return values.isEmpty() ? QStringLiteral("None") : values.join(QStringLiteral(", "));
}

QList<QPair<QString, QString>> applicationsForMime(const QString &mime)
{
    QList<QPair<QString, QString>> applications;
    for (const QString &root : QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation)) {
        QDirIterator iterator(root, {QStringLiteral("*.desktop")}, QDir::Files,
                              QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            const QString path = iterator.next();
            QSettings desktop(path, QSettings::IniFormat);
            desktop.beginGroup(QStringLiteral("Desktop Entry"));
            const QStringList mimeTypes = desktop.value(QStringLiteral("MimeType")).toString()
                                              .split(QLatin1Char(';'), Qt::SkipEmptyParts);
            const QString name = desktop.value(QStringLiteral("Name")).toString();
            desktop.endGroup();
            if (mimeTypes.contains(mime) && !name.isEmpty())
                applications.append({name, QFileInfo(path).fileName()});
        }
    }
    std::sort(applications.begin(), applications.end(),
              [](const auto &left, const auto &right) {
                  return left.first.compare(right.first, Qt::CaseInsensitive) < 0;
              });
    return applications;
}

struct FolderMetrics {
    qint64 bytes = 0;
    qint64 allocated = 0;
    int files = 0;
    int folders = 0;
};

FolderMetrics metricsFor(const QString &path)
{
    FolderMetrics metrics;
    auto add = [&metrics](const QFileInfo &info) {
        if (info.isDir()) {
            ++metrics.folders;
            return;
        }
        ++metrics.files;
        metrics.bytes += info.size();
        struct stat status {};
        if (::lstat(info.absoluteFilePath().toLocal8Bit().constData(), &status) == 0)
            metrics.allocated += static_cast<qint64>(status.st_blocks) * 512;
    };
    const QFileInfo root(path);
    if (root.isDir()) {
        QDirIterator iterator(path, QDir::AllEntries | QDir::NoDotAndDotDot,
                              QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            iterator.next();
            add(iterator.fileInfo());
        }
    } else {
        add(root);
    }
    return metrics;
}

void stylePropertyButtons(QDialogButtonBox *buttons)
{
    for (QPushButton *button : buttons->findChildren<QPushButton *>()) {
        button->setIcon(QIcon());
        button->setMinimumWidth(75);
    }
}
}

void Aero7Properties::showLibrary(const QString &id, QWidget *parent)
{
    Aero7Library library = Aero7Libraries::instance().library(id);
    if (library.id.isEmpty())
        return;

    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("%1 Properties").arg(library.name));
    dialog.resize(530, 470);
    auto *outer = new QVBoxLayout(&dialog);
    auto *tabs = new QTabWidget;
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->addWidget(new QLabel(QStringLiteral("Library locations:")));
    auto *locations = new QListWidget;
    auto repopulate = [&]() {
        locations->clear();
        for (const QString &path : std::as_const(library.locations)) {
            const bool available = QFileInfo(path).isDir();
            auto *item = new QListWidgetItem(
                QIcon::fromTheme(available ? QStringLiteral("folder") : QStringLiteral("dialog-warning")),
                available ? path : QStringLiteral("%1 — unavailable").arg(path));
            item->setData(Qt::UserRole, path);
            if (path == library.saveLocation)
                item->setCheckState(Qt::Checked);
            locations->addItem(item);
        }
    };
    repopulate();
    layout->addWidget(locations, 1);

    auto *locationButtons = new QHBoxLayout;
    auto *setSave = new QPushButton(QStringLiteral("Set save location"));
    auto *include = new QPushButton(QStringLiteral("Include a folder…"));
    auto *remove = new QPushButton(QStringLiteral("Remove"));
    locationButtons->addWidget(setSave);
    locationButtons->addStretch(1);
    locationButtons->addWidget(include);
    locationButtons->addWidget(remove);
    layout->addLayout(locationButtons);

    auto *form = new QFormLayout;
    auto *optimize = new QComboBox;
    optimize->addItems({QStringLiteral("General Items"), QStringLiteral("Documents"),
                        QStringLiteral("Music"), QStringLiteral("Pictures"),
                        QStringLiteral("Videos")});
    optimize->setCurrentText(library.optimizeFor);
    form->addRow(QStringLiteral("Optimize this library for:"), optimize);
    layout->addLayout(form);

    auto *shown = new QCheckBox(QStringLiteral("Shown in navigation pane"));
    shown->setChecked(library.shownInNavigationPane);
    layout->addWidget(shown);
    auto *restore = new QPushButton(QStringLiteral("Restore Defaults"));
    layout->addWidget(restore, 0, Qt::AlignRight);
    tabs->addTab(page, QStringLiteral("Library"));
    outer->addWidget(tabs);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok
                                         | QDialogButtonBox::Cancel
                                         | QDialogButtonBox::Apply);
    stylePropertyButtons(buttons);
    outer->addWidget(buttons);

    QObject::connect(setSave, &QPushButton::clicked, &dialog, [&]() {
        if (QListWidgetItem *item = locations->currentItem()) {
            library.saveLocation = item->data(Qt::UserRole).toString();
            repopulate();
        }
    });
    QObject::connect(remove, &QPushButton::clicked, &dialog, [&]() {
        if (library.locations.size() <= 1) {
            QMessageBox::warning(&dialog, QStringLiteral("Library Properties"),
                                 QStringLiteral("A Library must contain at least one folder."));
            return;
        }
        if (QListWidgetItem *item = locations->currentItem()) {
            const QString path = item->data(Qt::UserRole).toString();
            library.locations.removeAll(path);
            if (library.saveLocation == path)
                library.saveLocation = library.locations.constFirst();
            repopulate();
        }
    });
    QObject::connect(include, &QPushButton::clicked, &dialog, [&]() {
        Aero7CommonDialog chooser(Aero7CommonDialog::Mode::ChooseFolder,
                                  QStringLiteral("library-properties"), &dialog);
        if (chooser.exec() == QDialog::Accepted) {
            const QString path = chooser.selectedFiles().value(0);
            if (!path.isEmpty() && !library.locations.contains(path))
                library.locations.append(path);
            repopulate();
        }
    });
    QObject::connect(restore, &QPushButton::clicked, &dialog, [&]() {
        library = Aero7Libraries::instance().defaultLibrary(id);
        if (library.id.isEmpty()) {
            QMessageBox::warning(&dialog, QStringLiteral("Library Properties"),
                                 QStringLiteral("No default definition exists for this Library."));
            return;
        }
        optimize->setCurrentText(library.optimizeFor);
        shown->setChecked(library.shownInNavigationPane);
        repopulate();
    });

    auto apply = [&]() -> bool {
        library.optimizeFor = optimize->currentText();
        library.shownInNavigationPane = shown->isChecked();
        QString error;
        if (!Aero7Libraries::instance().saveLibrary(library, &error)) {
            QMessageBox::warning(&dialog, QStringLiteral("Library Properties"), error);
            return false;
        }
        return true;
    };
    QObject::connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked,
            &dialog, [apply]() { apply(); });
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
        if (apply()) dialog.accept();
    });
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    dialog.exec();
}

void Aero7Properties::showTrash(QWidget *parent)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("Recycle Bin Properties"));
    dialog.resize(500, 390);
    auto *outer = new QVBoxLayout(&dialog);

    const QString trashPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + QStringLiteral("/Trash");
    const QStorageInfo storage(trashPath);
    auto *locations = new QGroupBox(QStringLiteral("Recycle Bin Location"));
    auto *locationForm = new QFormLayout(locations);
    locationForm->addRow(QStringLiteral("Location:"), valueLabel(trashPath));
    locationForm->addRow(QStringLiteral("Space available:"), valueLabel(humanSize(storage.bytesAvailable())));
    outer->addWidget(locations);

    QSettings settings(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
                           + QStringLiteral("/trashrc"), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Aero7"));
    const int maximum = settings.value(QStringLiteral("MaximumSizeMiB"), 0).toInt();
    const bool immediate = settings.value(QStringLiteral("DeleteImmediately"), false).toBool();
    const bool confirm = settings.value(QStringLiteral("ConfirmDelete"), true).toBool();
    settings.endGroup();

    auto *box = new QGroupBox(QStringLiteral("Settings for selected location"));
    auto *layout = new QVBoxLayout(box);
    auto *customSize = new QCheckBox(QStringLiteral("Custom size"));
    customSize->setChecked(maximum > 0);
    auto *size = new QSpinBox;
    size->setRange(1, 1024 * 1024);
    size->setSuffix(QStringLiteral(" MB"));
    size->setValue(maximum > 0 ? maximum : 1024);
    size->setEnabled(customSize->isChecked());
    QObject::connect(customSize, &QCheckBox::toggled, size, &QWidget::setEnabled);
    layout->addWidget(customSize);
    layout->addWidget(size);
    auto *deleteImmediately = new QCheckBox(
        QStringLiteral("Don't move files to the Recycle Bin. Remove files immediately when deleted."));
    deleteImmediately->setChecked(immediate);
    layout->addWidget(deleteImmediately);
    outer->addWidget(box);
    auto *confirmation = new QCheckBox(QStringLiteral("Display delete confirmation dialog"));
    confirmation->setChecked(confirm);
    outer->addWidget(confirmation);
    outer->addStretch(1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok
                                         | QDialogButtonBox::Cancel
                                         | QDialogButtonBox::Apply);
    stylePropertyButtons(buttons);
    outer->addWidget(buttons);
    auto apply = [&]() {
        QSettings output(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
                             + QStringLiteral("/trashrc"), QSettings::IniFormat);
        output.beginGroup(QStringLiteral("Aero7"));
        output.setValue(QStringLiteral("MaximumSizeMiB"), customSize->isChecked() ? size->value() : 0);
        output.setValue(QStringLiteral("DeleteImmediately"), deleteImmediately->isChecked());
        output.setValue(QStringLiteral("ConfirmDelete"), confirmation->isChecked());
        output.endGroup();
        output.sync();
    };
    QObject::connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, &dialog, apply);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() { apply(); dialog.accept(); });
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    dialog.exec();
}

void Aero7Properties::show(const QList<QUrl> &urls, QWidget *parent)
{
    if (urls.size() == 1 && urls.constFirst().scheme() == QLatin1String("trash")) {
        showTrash(parent);
        return;
    }
    if (urls.size() == 1 && urls.constFirst().isLocalFile()) {
        const QString id = Aero7Libraries::instance().libraryIdForPath(urls.constFirst().toLocalFile());
        if (!id.isEmpty()
            && QDir::cleanPath(urls.constFirst().toLocalFile())
                == QDir::cleanPath(Aero7Libraries::instance().materializedPath(id))) {
            showLibrary(id, parent);
            return;
        }
    }
    if (urls.isEmpty())
        return;

    QDialog dialog(parent);
    const bool single = urls.size() == 1;
    const QFileInfo info(single && urls.constFirst().isLocalFile()
                             ? urls.constFirst().toLocalFile() : QString());
    dialog.setWindowTitle(single ? QStringLiteral("%1 Properties").arg(info.fileName())
                                 : QStringLiteral("%1 Items Properties").arg(urls.size()));
    dialog.resize(430, 470);
    auto *outer = new QVBoxLayout(&dialog);
    auto *tabs = new QTabWidget;

    auto *general = new QWidget;
    auto *form = new QFormLayout(general);
    auto *name = new QLineEdit(single ? info.fileName() : QStringLiteral("Multiple items"));
    name->setEnabled(single && info.exists());
    form->addRow(QStringLiteral("Name:"), name);
    QMimeDatabase database;
    const QString mime = single ? database.mimeTypeForFile(info).name() : QString();
    const FolderMetrics metrics = single && info.exists() ? metricsFor(info.absoluteFilePath())
                                                           : FolderMetrics{};
    form->addRow(QStringLiteral("Type:"), valueLabel(single ? database.mimeTypeForFile(info).comment()
                                                             : QStringLiteral("Multiple types")));
    auto *openWith = new QComboBox;
    if (single && info.isFile()) {
        QSettings associations(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
                                   + QStringLiteral("/mimeapps.list"), QSettings::IniFormat);
        associations.beginGroup(QStringLiteral("Default Applications"));
        const QString currentApplication = associations.value(mime).toString().section(QLatin1Char(';'), 0, 0);
        associations.endGroup();
        for (const auto &application : applicationsForMime(mime))
            openWith->addItem(application.first, application.second);
        const int applicationIndex = openWith->findData(currentApplication);
        if (applicationIndex >= 0)
            openWith->setCurrentIndex(applicationIndex);
        form->addRow(QStringLiteral("Opens with:"), openWith);
    }
    form->addRow(QStringLiteral("Location:"), valueLabel(single ? info.absolutePath() : QStringLiteral("Various locations")));
    form->addRow(QStringLiteral("Size:"), valueLabel(single ? humanSize(metrics.bytes) : QStringLiteral("—")));
    form->addRow(QStringLiteral("Size on disk:"), valueLabel(single ? humanSize(metrics.allocated) : QStringLiteral("—")));
    if (single && info.isDir())
        form->addRow(QStringLiteral("Contains:"), valueLabel(QStringLiteral("%1 files, %2 folders")
                                                                  .arg(metrics.files).arg(metrics.folders)));
    if (single) {
        const QLocale locale;
        form->addRow(QStringLiteral("Created:"), valueLabel(locale.toString(info.birthTime(), QLocale::ShortFormat)));
        form->addRow(QStringLiteral("Modified:"), valueLabel(locale.toString(info.lastModified(), QLocale::ShortFormat)));
        form->addRow(QStringLiteral("Accessed:"), valueLabel(locale.toString(info.lastRead(), QLocale::ShortFormat)));
    }
    auto *readOnly = new QCheckBox(QStringLiteral("Read-only"));
    readOnly->setChecked(single && !info.isWritable());
    auto *hidden = new QCheckBox(QStringLiteral("Hidden"));
    hidden->setChecked(single && info.fileName().startsWith(QLatin1Char('.')));
    auto *attributes = new QHBoxLayout;
    attributes->addWidget(readOnly);
    attributes->addWidget(hidden);
    attributes->addStretch(1);
    form->addRow(QStringLiteral("Attributes:"), attributes);
    tabs->addTab(general, QStringLiteral("General"));

    auto *security = new QWidget;
    auto *securityForm = new QFormLayout(security);
    securityForm->addRow(QStringLiteral("Owner:"), valueLabel(single ? info.owner() : QStringLiteral("Various")));
    securityForm->addRow(QStringLiteral("Group:"), valueLabel(single ? info.group() : QStringLiteral("Various")));
    securityForm->addRow(QStringLiteral("Owner permissions:"), valueLabel(single ? accessText(info.permissions()) : QStringLiteral("Various")));
    securityForm->addRow(QStringLiteral("Linux mode:"), valueLabel(single ? QString::number(static_cast<int>(info.permissions()) & 0x0fff, 8) : QStringLiteral("—")));
    auto *truth = new QLabel(QStringLiteral("These are the file's real Linux ownership and permission settings. They are not represented as Windows ACLs."));
    truth->setWordWrap(true);
    securityForm->addRow(truth);
    tabs->addTab(security, QStringLiteral("Security"));

    auto *details = new QWidget;
    auto *detailsForm = new QFormLayout(details);
    detailsForm->addRow(QStringLiteral("MIME type:"), valueLabel(mime));
    detailsForm->addRow(QStringLiteral("File name:"), valueLabel(single ? info.fileName() : QStringLiteral("—")));
    detailsForm->addRow(QStringLiteral("Path:"), valueLabel(single ? info.absoluteFilePath() : QStringLiteral("—")));
    if (single && mime.startsWith(QLatin1String("image/"))) {
        QImageReader reader(info.absoluteFilePath());
        const QSize dimensions = reader.size();
        if (dimensions.isValid())
            detailsForm->addRow(QStringLiteral("Dimensions:"), valueLabel(QStringLiteral("%1 × %2").arg(dimensions.width()).arg(dimensions.height())));
    }
    tabs->addTab(details, QStringLiteral("Details"));
    outer->addWidget(tabs);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok
                                         | QDialogButtonBox::Cancel
                                         | QDialogButtonBox::Apply);
    stylePropertyButtons(buttons);
    outer->addWidget(buttons);
    auto apply = [&]() -> bool {
        if (!single || !info.exists())
            return true;
        QString path = info.absoluteFilePath();
        QString targetName = name->text().trimmed();
        if (hidden->isChecked() && !targetName.startsWith(QLatin1Char('.')))
            targetName.prepend(QLatin1Char('.'));
        else if (!hidden->isChecked() && targetName.startsWith(QLatin1Char('.')))
            targetName.remove(0, 1);
        const QString target = QDir(info.absolutePath()).filePath(targetName);
        if (path != target && !QFile::rename(path, target)) {
            QMessageBox::warning(&dialog, QStringLiteral("Properties"),
                                 QStringLiteral("The item could not be renamed."));
            return false;
        }
        QFile file(target);
        QFile::Permissions permissions = file.permissions();
        permissions.setFlag(QFile::WriteOwner, !readOnly->isChecked());
        if (!file.setPermissions(permissions)) {
            QMessageBox::warning(&dialog, QStringLiteral("Properties"),
                                 QStringLiteral("The attributes could not be changed."));
            return false;
        }
        if (openWith->count() > 0) {
            QSettings associations(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
                                       + QStringLiteral("/mimeapps.list"), QSettings::IniFormat);
            associations.beginGroup(QStringLiteral("Default Applications"));
            associations.setValue(mime, QString(openWith->currentData().toString()
                                                + QLatin1Char(';')));
            associations.endGroup();
            associations.sync();
        }
        return true;
    };
    QObject::connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked,
            &dialog, [apply]() { apply(); });
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
        if (apply()) dialog.accept();
    });
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    dialog.exec();
}
