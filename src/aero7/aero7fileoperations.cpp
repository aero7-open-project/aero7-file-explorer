/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aero7fileoperations.h"

#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSaveFile>
#include <QStorageInfo>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace {
QString compactSize(qint64 bytes)
{
    if (bytes >= 1024LL * 1024 * 1024)
        return QStringLiteral("%1 GB").arg(bytes / (1024.0 * 1024 * 1024), 0, 'f', 1);
    if (bytes >= 1024LL * 1024)
        return QStringLiteral("%1 MB").arg(bytes / (1024.0 * 1024), 0, 'f', 1);
    if (bytes >= 1024)
        return QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    return QStringLiteral("%1 bytes").arg(bytes);
}
}

bool Aero7FileOperationDialog::run(Operation operation,
                                   const QList<QUrl> &sourceUrls,
                                   const QUrl &destinationUrl,
                                   QWidget *parent)
{
    if (!destinationUrl.isLocalFile()) {
        QMessageBox::warning(parent, QStringLiteral("File Operation"),
                             QStringLiteral("This destination is not available as a local folder."));
        return false;
    }
    QStringList sources;
    for (const QUrl &url : sourceUrls) {
        if (!url.isLocalFile()) {
            QMessageBox::warning(parent, QStringLiteral("File Operation"),
                                 QStringLiteral("One of the selected items is not a local file."));
            return false;
        }
        sources.append(url.toLocalFile());
    }
    Aero7FileOperationDialog dialog(operation, sources,
                                    destinationUrl.toLocalFile(), parent);
    QTimer::singleShot(0, &dialog, &Aero7FileOperationDialog::start);
    return dialog.exec() == QDialog::Accepted;
}

Aero7FileOperationDialog::Aero7FileOperationDialog(Operation operation,
                                                   QStringList sources,
                                                   QString destination,
                                                   QWidget *parent)
    : QDialog(parent), m_operation(operation), m_sources(std::move(sources)),
      m_destination(std::move(destination))
{
    setWindowTitle(operation == Operation::Copy
                       ? QStringLiteral("Copying Files")
                       : QStringLiteral("Moving Files"));
    setModal(true);
    resize(560, 290);
    auto *outer = new QVBoxLayout(this);
    auto *title = new QLabel(operation == Operation::Copy
                                 ? QStringLiteral("Copying items…")
                                 : QStringLiteral("Moving items…"));
    QFont font = title->font();
    font.setPointSize(font.pointSize() + 2);
    title->setFont(font);
    outer->addWidget(title);
    auto *form = new QFormLayout;
    m_sourceLabel = new QLabel(m_sources.join(QStringLiteral(", ")));
    m_sourceLabel->setWordWrap(true);
    m_destinationLabel = new QLabel(m_destination);
    m_destinationLabel->setWordWrap(true);
    m_currentLabel = new QLabel;
    m_currentLabel->setWordWrap(true);
    form->addRow(QStringLiteral("From:"), m_sourceLabel);
    form->addRow(QStringLiteral("To:"), m_destinationLabel);
    form->addRow(QStringLiteral("Current item:"), m_currentLabel);
    outer->addLayout(form);
    m_progress = new QProgressBar;
    m_progress->setRange(0, 1000);
    outer->addWidget(m_progress);
    auto *status = new QHBoxLayout;
    m_remainingLabel = new QLabel;
    m_speedLabel = new QLabel;
    status->addWidget(m_remainingLabel);
    status->addStretch(1);
    status->addWidget(m_speedLabel);
    outer->addLayout(status);
    outer->addStretch(1);
    auto *cancel = new QPushButton(QStringLiteral("Cancel"));
    connect(cancel, &QPushButton::clicked, this, [this]() { m_cancelled = true; });
    outer->addWidget(cancel, 0, Qt::AlignRight);
}

qint64 Aero7FileOperationDialog::measure(const QString &path, int *files) const
{
    const QFileInfo info(path);
    if (info.isSymLink() || info.isFile()) {
        ++*files;
        return info.size();
    }
    qint64 bytes = 0;
    QDirIterator iterator(path, QDir::AllEntries | QDir::NoDotAndDotDot,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        iterator.next();
        const QFileInfo entry = iterator.fileInfo();
        if (entry.isFile() || entry.isSymLink()) {
            bytes += entry.size();
            ++*files;
        }
    }
    return bytes;
}

void Aero7FileOperationDialog::start()
{
    for (const QString &source : std::as_const(m_sources))
        m_totalBytes += measure(source, &m_totalFiles);
    const QStorageInfo storage(m_destination);
    qint64 available = storage.bytesAvailable();
    bool availableOk = false;
    const qint64 testAvailable = qEnvironmentVariableIntValue(
        "AERO7_TEST_AVAILABLE_BYTES", &availableOk);
    if (availableOk)
        available = testAvailable;
    if (storage.isValid() && available >= 0 && m_totalBytes > available) {
        if (!availableOk) {
            QMessageBox::warning(
                this, QStringLiteral("Not Enough Space"),
                QStringLiteral("There is not enough space on %1.\n\nSpace needed: %2\nSpace available: %3")
                    .arg(storage.displayName(), compactSize(m_totalBytes),
                         compactSize(available)));
        }
        reject();
        return;
    }
    m_timer.start();
    bool success = true;
    for (const QString &source : std::as_const(m_sources)) {
        const QString destination = QDir(m_destination).filePath(QFileInfo(source).fileName());
        if (!processItem(source, destination)) {
            success = false;
            break;
        }
    }
    if (m_cancelled || !success)
        reject();
    else
        accept();
}

bool Aero7FileOperationDialog::processItem(const QString &source,
                                           const QString &destination)
{
    if (m_cancelled)
        return false;
    const QFileInfo info(source);
    const bool copied = info.isDir() && !info.isSymLink()
        ? copyDirectory(source, destination)
        : copyFile(source, destination);
    if (copied && m_operation == Operation::Move) {
        if (info.isDir() && !info.isSymLink())
            QDir(source).removeRecursively();
        else
            QFile::remove(source);
    }
    return copied;
}

QString Aero7FileOperationDialog::keepBothName(const QString &path) const
{
    const QFileInfo info(path);
    const QString directory = info.absolutePath();
    const QString stem = info.completeBaseName();
    const QString suffix = info.completeSuffix();
    int number = 2;
    QString candidate;
    do {
        const QString name = suffix.isEmpty()
            ? QStringLiteral("%1 (%2)").arg(stem).arg(number++)
            : QStringLiteral("%1 (%2).%3").arg(stem).arg(number++).arg(suffix);
        candidate = QDir(directory).filePath(name);
    } while (QFileInfo::exists(candidate));
    return candidate;
}

Aero7FileOperationDialog::Decision
Aero7FileOperationDialog::resolveConflict(const QFileInfo &source,
                                          const QFileInfo &destination)
{
    const QByteArray automatic = qgetenv("AERO7_TEST_CONFLICT").toLower();
    if (automatic == "replace")
        return Decision::Replace;
    if (automatic == "skip")
        return Decision::Skip;
    if (automatic == "keep-both")
        return Decision::KeepBoth;
    if (automatic == "cancel")
        return Decision::Cancel;
    if (m_applyDecision)
        return m_savedDecision;
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("File Already Exists"));
    dialog.resize(560, 330);
    auto *layout = new QVBoxLayout(&dialog);
    auto *text = new QLabel(QStringLiteral("There is already an item with the same name in this location."));
    text->setWordWrap(true);
    layout->addWidget(text);
    auto *details = new QFormLayout;
    details->addRow(QStringLiteral("Copying:"), new QLabel(QStringLiteral("%1\n%2\n%3")
        .arg(source.fileName(), source.absolutePath(), compactSize(source.size()))));
    details->addRow(QStringLiteral("Existing:"), new QLabel(QStringLiteral("%1\n%2\n%3")
        .arg(destination.fileName(), destination.absolutePath(), compactSize(destination.size()))));
    layout->addLayout(details);
    auto *applyAll = new QCheckBox(QStringLiteral("Do this for the next conflicts"));
    layout->addWidget(applyAll);
    auto *buttons = new QDialogButtonBox;
    auto *replace = buttons->addButton(QStringLiteral("Copy and Replace"), QDialogButtonBox::AcceptRole);
    auto *skip = buttons->addButton(QStringLiteral("Don't Copy"), QDialogButtonBox::DestructiveRole);
    auto *keep = buttons->addButton(QStringLiteral("Copy, but keep both files"), QDialogButtonBox::ActionRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    Decision decision = Decision::Cancel;
    connect(replace, &QPushButton::clicked, &dialog, [&]() { decision = Decision::Replace; dialog.accept(); });
    connect(skip, &QPushButton::clicked, &dialog, [&]() { decision = Decision::Skip; dialog.accept(); });
    connect(keep, &QPushButton::clicked, &dialog, [&]() { decision = Decision::KeepBoth; dialog.accept(); });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted)
        return Decision::Cancel;
    if (applyAll->isChecked()) {
        m_applyDecision = true;
        m_savedDecision = decision;
    }
    return decision;
}

Aero7FileOperationDialog::ErrorAction
Aero7FileOperationDialog::resolveError(const QString &title,
                                       const QString &message)
{
    const QByteArray automatic = qgetenv("AERO7_TEST_ERROR").toLower();
    if (automatic == "retry")
        return ErrorAction::Retry;
    if (automatic == "skip")
        return ErrorAction::Skip;
    if (automatic == "cancel")
        return ErrorAction::Cancel;
    QMessageBox dialog(QMessageBox::Warning, title, message, QMessageBox::NoButton, this);
    QPushButton *retry = dialog.addButton(QStringLiteral("Try Again"), QMessageBox::AcceptRole);
    QPushButton *skip = dialog.addButton(QStringLiteral("Skip"), QMessageBox::DestructiveRole);
    dialog.addButton(QStringLiteral("Cancel"), QMessageBox::RejectRole);
    dialog.exec();
    if (dialog.clickedButton() == retry)
        return ErrorAction::Retry;
    if (dialog.clickedButton() == skip)
        return ErrorAction::Skip;
    return ErrorAction::Cancel;
}

bool Aero7FileOperationDialog::copyFile(const QString &source,
                                        const QString &initialDestination)
{
    QString destination = initialDestination;
    if (QFileInfo::exists(destination)) {
        const Decision decision = resolveConflict(QFileInfo(source), QFileInfo(destination));
        if (decision == Decision::Cancel) {
            m_cancelled = true;
            return false;
        }
        if (decision == Decision::Skip) {
            updateProgress(source, QFileInfo(source).size());
            return true;
        }
        if (decision == Decision::KeepBoth)
            destination = keepBothName(destination);
        else if (!QFile::remove(destination)) {
            const ErrorAction action = resolveError(
                QStringLiteral("File In Use"),
                QStringLiteral("The existing item could not be replaced. It may be open or in use."));
            if (action == ErrorAction::Retry)
                return copyFile(source, initialDestination);
            if (action == ErrorAction::Skip)
                return true;
            m_cancelled = true;
            return false;
        }
    }

    QFile input(source);
    QSaveFile output(destination);
    if (!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly)) {
        const ErrorAction action = resolveError(
            QStringLiteral("File Operation Error"),
            QStringLiteral("The file could not be read or written.\n\n%1").arg(source));
        if (action == ErrorAction::Retry)
            return copyFile(source, destination);
        if (action == ErrorAction::Skip)
            return true;
        m_cancelled = true;
        return false;
    }
    while (!input.atEnd()) {
        if (m_cancelled) {
            output.cancelWriting();
            return false;
        }
        const QByteArray block = input.read(1024 * 1024);
        if (block.isEmpty() && input.error() != QFile::NoError) {
            output.cancelWriting();
            return false;
        }
        if (output.write(block) != block.size()) {
            output.cancelWriting();
            return false;
        }
        updateProgress(source, block.size());
    }
    if (!output.commit())
        return false;
    QFile::setPermissions(destination, QFileInfo(source).permissions());
    ++m_doneFiles;
    return true;
}

bool Aero7FileOperationDialog::copyDirectory(const QString &source,
                                             const QString &destination)
{
    if (!QDir().mkpath(destination)) {
        const ErrorAction action = resolveError(
            QStringLiteral("Folder Error"),
            QStringLiteral("The destination folder could not be created.\n\n%1").arg(destination));
        if (action == ErrorAction::Retry)
            return copyDirectory(source, destination);
        if (action == ErrorAction::Skip)
            return true;
        m_cancelled = true;
        return false;
    }
    const QFileInfoList entries = QDir(source).entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot,
        QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo &entry : entries) {
        if (!processItem(entry.absoluteFilePath(), QDir(destination).filePath(entry.fileName())))
            return false;
    }
    return true;
}

void Aero7FileOperationDialog::updateProgress(const QString &current,
                                              qint64 increment)
{
    m_doneBytes += increment;
    m_currentLabel->setText(current);
    m_progress->setValue(m_totalBytes > 0
                             ? static_cast<int>((m_doneBytes * 1000) / m_totalBytes)
                             : 1000);
    m_remainingLabel->setText(QStringLiteral("%1 files remaining — %2")
        .arg(qMax(0, m_totalFiles - m_doneFiles))
        .arg(compactSize(qMax<qint64>(0, m_totalBytes - m_doneBytes))));
    const qint64 elapsed = m_timer.elapsed();
    if (elapsed > 1000) {
        const double speed = m_doneBytes / (elapsed / 1000.0);
        m_speedLabel->setText(QStringLiteral("%1/s").arg(compactSize(static_cast<qint64>(speed))));
    }
    QApplication::processEvents(QEventLoop::AllEvents, 20);
}

bool Aero7FileOperationDialog::confirmPermanentDelete(const QList<QUrl> &urls,
                                                      QWidget *parent)
{
    const QByteArray automatic = qgetenv("AERO7_TEST_CONFIRM_DELETE").toLower();
    if (automatic == "yes")
        return true;
    if (automatic == "no")
        return false;
    QStringList names;
    for (const QUrl &url : urls)
        names << url.fileName();
    QMessageBox dialog(QMessageBox::Warning, QStringLiteral("Delete File"),
                       urls.size() == 1
                           ? QStringLiteral("Are you sure you want to permanently delete this file?\n\n%1")
                                 .arg(names.constFirst())
                           : QStringLiteral("Are you sure you want to permanently delete these %1 items?\n\n%2")
                                 .arg(urls.size()).arg(names.mid(0, 5).join(QStringLiteral("\n"))),
                       QMessageBox::NoButton, parent);
    QPushButton *yes = dialog.addButton(QStringLiteral("Yes"), QMessageBox::DestructiveRole);
    dialog.addButton(QStringLiteral("No"), QMessageBox::RejectRole);
    dialog.exec();
    return dialog.clickedButton() == yes;
}

bool Aero7FileOperationDialog::deletePermanently(const QList<QUrl> &urls,
                                                 QWidget *parent)
{
    if (!confirmPermanentDelete(urls, parent))
        return false;
    for (const QUrl &url : urls) {
        if (!url.isLocalFile()) {
            QMessageBox::warning(parent, QStringLiteral("Delete File"),
                                 QStringLiteral("This location cannot be deleted by the local file service."));
            return false;
        }
        const QString path = url.toLocalFile();
        while (QFileInfo::exists(path)) {
            const QFileInfo info(path);
            const bool removed = info.isDir() && !info.isSymLink()
                ? QDir(path).removeRecursively() : QFile::remove(path);
            if (removed)
                break;
            QMessageBox error(QMessageBox::Warning, QStringLiteral("Delete File"),
                              QStringLiteral("The item could not be deleted. It may be in use or you may not have permission.\n\n%1").arg(path),
                              QMessageBox::NoButton, parent);
            QPushButton *retry = error.addButton(QStringLiteral("Try Again"), QMessageBox::AcceptRole);
            QPushButton *skip = error.addButton(QStringLiteral("Skip"), QMessageBox::DestructiveRole);
            error.addButton(QStringLiteral("Cancel"), QMessageBox::RejectRole);
            error.exec();
            if (error.clickedButton() == retry)
                continue;
            if (error.clickedButton() == skip)
                break;
            return false;
        }
    }
    return true;
}

QList<QUrl> Aero7FileOperationDialog::renameItems(const QList<QUrl> &urls,
                                                  QWidget *parent)
{
    if (urls.isEmpty()
        || std::any_of(urls.cbegin(), urls.cend(),
                       [](const QUrl &url) { return !url.isLocalFile(); })) {
        return {};
    }
    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("Rename"));
    dialog.resize(440, 180);
    auto *layout = new QVBoxLayout(&dialog);
    auto *message = new QLabel(urls.size() == 1
        ? QStringLiteral("Type a new name for this item:")
        : QStringLiteral("Type a base name for the selected items. A number will be added to each name:"));
    message->setWordWrap(true);
    layout->addWidget(message);
    auto *name = new QLineEdit(urls.size() == 1 ? urls.constFirst().fileName()
                                                : QStringLiteral("Renamed item"));
    name->selectAll();
    layout->addWidget(name);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted || name->text().trimmed().isEmpty())
        return {};

    QList<QUrl> renamed;
    const QString requested = name->text().trimmed();
    for (int index = 0; index < urls.size(); ++index) {
        const QFileInfo source(urls.at(index).toLocalFile());
        QString targetName = requested;
        if (urls.size() > 1) {
            const QString suffix = source.completeSuffix();
            targetName = suffix.isEmpty()
                ? QStringLiteral("%1 (%2)").arg(requested).arg(index + 1)
                : QStringLiteral("%1 (%2).%3").arg(requested).arg(index + 1).arg(suffix);
        }
        const QString target = QDir(source.absolutePath()).filePath(targetName);
        if (QFileInfo::exists(target)) {
            QMessageBox::warning(parent, QStringLiteral("Rename"),
                                 QStringLiteral("An item named %1 already exists.").arg(targetName));
            return {};
        }
        if (!QFile::rename(source.absoluteFilePath(), target)) {
            QMessageBox::warning(parent, QStringLiteral("Rename"),
                                 QStringLiteral("The item could not be renamed.\n\n%1").arg(source.absoluteFilePath()));
            return {};
        }
        renamed.append(QUrl::fromLocalFile(target));
    }
    return renamed;
}
