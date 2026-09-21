/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QFile>
#include <QObject>
#include <QTimer>
#include <functional>
#include <optional>
#include <utility>

namespace Aero7Storage
{
// procfs mount changes do not reliably produce ordinary file-watcher events.
// Poll the small kernel snapshot, not every filesystem's capacity, and notify
// only when the mount table changes. This also sees mounts outside Solid/KIO.
// A failed read retains the last good snapshot rather than removing devices.
class MountWatcher final : public QObject
{
public:
    explicit MountWatcher(QObject *parent, std::function<void()> changed,
                          QString source = QStringLiteral("/proc/self/mountinfo"),
                          int interval = 1000)
        : QObject(parent), m_source(std::move(source)), m_changed(std::move(changed))
    {
        setObjectName(QStringLiteral("aero7MountWatcher"));
        m_snapshot = readSnapshot();
        auto *timer = new QTimer(this);
        timer->setInterval(qMax(1, interval));
        connect(timer, &QTimer::timeout, this, [this] { poll(); });
        timer->start();
    }

private:
    std::optional<QByteArray> readSnapshot() const
    {
        QFile file(m_source);
        if (!file.open(QIODevice::ReadOnly)) return std::nullopt;
        const QByteArray snapshot = file.readAll();
        if (file.error() != QFileDevice::NoError) return std::nullopt;
        return snapshot;
    }

    void poll()
    {
        const auto snapshot = readSnapshot();
        if (!snapshot || snapshot == m_snapshot) return;
        m_snapshot = snapshot;
        // The consumer may destroy its window (and this watcher) while handling
        // the notification. Do not access members after calling the copy.
        const auto changed = m_changed;
        changed();
    }

    const QString m_source;
    const std::function<void()> m_changed;
    std::optional<QByteArray> m_snapshot;
};
}
