/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aero7dialogsearch.h"
#include "aero7icons.h"
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QLabel>
#include <QMimeDatabase>
#include <QStandardItemModel>

void Aero7DialogSearchWorker::run()
{
    // Resolve roots off-thread and suppress duplicate/overlapping library roots.
    QStringList normalized;
    int unavailable = 0;
    for (const QString &root : roots) {
        if (isInterruptionRequested()) return;
        const QFileInfo info(root);
        if (!info.isDir() || !info.isReadable()) { ++unavailable; continue; }
        const QString path = info.canonicalFilePath();
        if (!path.isEmpty() && !normalized.contains(path)) normalized.append(path);
    }
    QStringList pending;
    for (const QString &path : normalized) {
        bool contained = false;
        for (const QString &other : normalized)
            if (path != other && path.startsWith(other.endsWith('/') ? other : other + '/')) {
                contained = true;
                break;
            }
        if (!contained) pending.append(path);
    }
    QStringList paths, names;
    QList<bool> directories;
    QElapsedTimer elapsed;
    elapsed.start();
    const auto flush = [&]() {
        if (paths.isEmpty()) return !isInterruptionRequested();
        Q_EMIT batch(paths, names, directories);
        // Only one batch may be queued. A fast disk cannot flood the UI queue.
        while (!m_ack.tryAcquire(1, 25))
            if (isInterruptionRequested()) return false;
        paths.clear(); names.clear(); directories.clear();
        elapsed.restart();
        return !isInterruptionRequested();
    };
    QDir::Filters flags = QDir::AllEntries | QDir::NoDotAndDotDot;
    if (showHidden) flags |= QDir::Hidden;
    while (!pending.isEmpty()) {
        if (isInterruptionRequested()) return;
        const QString directory = pending.takeLast();
        const QFileInfo directoryInfo(directory);
        if (!directoryInfo.isDir() || !directoryInfo.isReadable()) { ++unavailable; continue; }
        // Explicit traversal allows cancellation between entries/directories.
        // Directory symlinks are visible but never recursively followed.
        QDirIterator iterator(directory, flags);
        while (!isInterruptionRequested() && iterator.hasNext()) {
            const QString path = iterator.next();
            const QFileInfo info = iterator.fileInfo();
            const bool isDirectory = info.isDir();
            if (isDirectory && !info.isSymLink()) pending.append(path);
            if ((!foldersOnly || isDirectory)
                && (isDirectory || info.isFile())
                && info.fileName().contains(query, Qt::CaseInsensitive)
                && (isDirectory || patterns.isEmpty() || QDir::match(patterns, info.fileName()))) {
                paths.append(path); names.append(info.fileName()); directories.append(isDirectory);
            }
            if ((paths.size() >= 64 || elapsed.elapsed() >= 40) && !flush()) return;
        }
    }
    if (!isInterruptionRequested() && flush()) Q_EMIT completed(unavailable);
}

Aero7DialogSearch::Aero7DialogSearch(QStandardItemModel *model, QLabel *status, QObject *parent)
    : QObject(parent), m_model(model), m_status(status)
{
    setObjectName(QStringLiteral("dialogSearch"));
    setProperty("busy", false);
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(150);
    connect(&m_debounce, &QTimer::timeout, this, &Aero7DialogSearch::startPending);
}

Aero7DialogSearch::~Aero7DialogSearch()
{
    // Never wait on filesystem I/O from the GUI thread, including destruction.
    if (m_worker) m_worker->requestInterruption();
}

void Aero7DialogSearch::cancel()
{
    ++m_generation;
    m_pending = false;
    m_debounce.stop();
    if (m_worker) m_worker->requestInterruption();
    setProperty("busy", false);
    m_status->hide();
}

void Aero7DialogSearch::submit(const QStringList &roots, const QString &query,
                             const QStringList &patterns, bool foldersOnly, bool showHidden)
{
    cancel();
    m_roots = roots; m_query = query; m_patterns = patterns;
    m_foldersOnly = foldersOnly; m_showHidden = showHidden;
    m_pending = true;
    setProperty("busy", true);
    m_status->setText(QStringLiteral("Searching…"));
    m_status->show();
    m_debounce.start();
}

void Aero7DialogSearch::startPending()
{
    // At most one worker per dialog, even during rapid query/filter changes.
    if (!m_pending || m_worker || m_debounce.isActive()) return;
    m_pending = false;
    const quint64 generation = m_generation;
    auto *worker = new Aero7DialogSearchWorker;
    m_worker = worker;
    worker->roots = m_roots; worker->query = m_query; worker->patterns = m_patterns;
    worker->foldersOnly = m_foldersOnly; worker->showHidden = m_showHidden;
    const QPointer<Aero7DialogSearchWorker> guardedWorker(worker);
    connect(worker, &Aero7DialogSearchWorker::batch, this,
        [this, generation, guardedWorker](const QStringList &paths, const QStringList &names, const QList<bool> &directories) {
            if (generation == m_generation) {
                QMimeDatabase mimeTypes;
                for (qsizetype i = 0; i < paths.size(); ++i) {
                    // Use bundled icons without another filesystem stat in the GUI.
                    QString iconName = QStringLiteral("folder");
                    if (!directories[i]) {
                        iconName = mimeTypes.mimeTypeForFile(names[i], QMimeDatabase::MatchExtension).genericIconName();
                        if (iconName != QLatin1String("image-x-generic")
                            && iconName != QLatin1String("audio-x-generic")
                            && iconName != QLatin1String("video-x-generic"))
                            iconName = QStringLiteral("text-x-generic");
                    }
                    auto *item = new QStandardItem(Aero7Icons::icon(iconName), names[i]);
                    item->setData(paths[i], Qt::UserRole + 10);
                    item->setToolTip(paths[i]);
                    item->setEditable(false);
                    m_model->appendRow(item);
                }
                m_status->setText(QStringLiteral("Searching… %1 items").arg(m_model->rowCount()));
            }
            if (guardedWorker) guardedWorker->acknowledgeBatch();
        });
    connect(worker, &Aero7DialogSearchWorker::completed, this, [this, generation](int unavailable) {
        if (generation != m_generation) return;
        setProperty("busy", false);
        const QString items = m_model->rowCount() == 1 ? QStringLiteral("1 item")
            : QStringLiteral("%1 items").arg(m_model->rowCount());
        m_status->setText(unavailable
            ? QStringLiteral("%1 — %2 %3 unavailable").arg(items).arg(unavailable)
                .arg(unavailable == 1 ? QStringLiteral("folder") : QStringLiteral("folders"))
            : items);
    });
    connect(worker, &QThread::finished, this, [this, worker] {
        if (m_worker == worker) m_worker = nullptr;
        startPending();
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}
