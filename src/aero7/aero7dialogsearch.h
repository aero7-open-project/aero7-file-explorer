/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
// Private implementation: do not change the exported dialog's object layout.
#include <QObject>
#include <QPointer>
#include <QSemaphore>
#include <QStringList>
#include <QThread>
#include <QTimer>

class QLabel;
class QStandardItemModel;

class Aero7DialogSearchWorker final : public QThread
{
    Q_OBJECT
public:
    QStringList roots, patterns;
    QString query;
    bool foldersOnly = false, showHidden = false;
    void acknowledgeBatch() { m_ack.release(); }
Q_SIGNALS:
    void batch(const QStringList &paths, const QStringList &names, const QList<bool> &directories);
    void completed(int unavailable);
private:
    void run() override;
    QSemaphore m_ack;
};

class Aero7DialogSearch final : public QObject
{
    Q_OBJECT
public:
    Aero7DialogSearch(QStandardItemModel *model, QLabel *status, QObject *parent);
    ~Aero7DialogSearch() override;
    void submit(const QStringList &roots, const QString &query, const QStringList &patterns,
                bool foldersOnly, bool showHidden);
    void cancel();
private:
    void startPending();
    QStandardItemModel *m_model;
    QLabel *m_status;
    QTimer m_debounce;
    QPointer<Aero7DialogSearchWorker> m_worker;
    QStringList m_roots, m_patterns;
    QString m_query;
    quint64 m_generation = 0;
    bool m_pending = false, m_foldersOnly = false, m_showHidden = false;
};
