/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QDialog>
#include <QElapsedTimer>
#include <QList>
#include <QUrl>

class QLabel;
class QProgressBar;
class QFileInfo;

class Aero7FileOperationDialog final : public QDialog
{
    Q_OBJECT

public:
    enum class Operation { Copy, Move };
    static bool run(Operation operation, const QList<QUrl> &sources,
                    const QUrl &destination, QWidget *parent = nullptr);
    static bool confirmPermanentDelete(const QList<QUrl> &urls,
                                       QWidget *parent = nullptr);
    static bool deletePermanently(const QList<QUrl> &urls,
                                  QWidget *parent = nullptr);
    static QList<QUrl> renameItems(const QList<QUrl> &urls,
                                   QWidget *parent = nullptr);

private:
    enum class Decision { Replace, Skip, KeepBoth, Cancel };
    enum class ErrorAction { Retry, Skip, Cancel };

    Aero7FileOperationDialog(Operation operation, QStringList sources,
                             QString destination, QWidget *parent);
    void start();
    bool processItem(const QString &source, const QString &destination);
    bool copyFile(const QString &source, const QString &destination);
    bool copyDirectory(const QString &source, const QString &destination);
    qint64 measure(const QString &path, int *files) const;
    QString keepBothName(const QString &path) const;
    Decision resolveConflict(const QFileInfo &source, const QFileInfo &destination);
    ErrorAction resolveError(const QString &title, const QString &message);
    void updateProgress(const QString &current, qint64 increment);

    Operation m_operation;
    QStringList m_sources;
    QString m_destination;
    bool m_cancelled = false;
    bool m_applyDecision = false;
    Decision m_savedDecision = Decision::Skip;
    qint64 m_totalBytes = 0;
    qint64 m_doneBytes = 0;
    int m_totalFiles = 0;
    int m_doneFiles = 0;
    QElapsedTimer m_timer;
    QLabel *m_sourceLabel = nullptr;
    QLabel *m_destinationLabel = nullptr;
    QLabel *m_currentLabel = nullptr;
    QLabel *m_remainingLabel = nullptr;
    QLabel *m_speedLabel = nullptr;
    QProgressBar *m_progress = nullptr;
};
