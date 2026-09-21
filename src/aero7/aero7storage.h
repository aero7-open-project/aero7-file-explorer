/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QDir>
#include <QStorageInfo>
#include <QStringList>
#include <algorithm>

// Private shared presentation policy. No placeholder folders or invented devices.
namespace Aero7Storage
{
inline QString sizeText(qint64 bytes)
{
    if (bytes < 0) return QStringLiteral("Unavailable");
    if (bytes < 1024)
        return bytes == 1 ? QStringLiteral("1 byte") : QStringLiteral("%1 bytes").arg(bytes);
    // Windows-style unit labels with binary scaling, including small volumes.
    // Never round a nonempty filesystem down to the misleading "0.0 GB".
    static const QStringList units{QStringLiteral("KB"), QStringLiteral("MB"),
                                   QStringLiteral("GB"), QStringLiteral("TB"), QStringLiteral("PB")};
    double value = bytes / 1024.0;
    int unit = 0;
    while (value >= 1024.0 && unit + 1 < units.size()) {
        value /= 1024.0;
        ++unit;
    }
    return QStringLiteral("%1 %2").arg(value, 0, 'f', value < 10.0 ? 1 : 0).arg(units.at(unit));
}

inline bool visibleRoot(const QString &path, const QString &user)
{
    const QString root = QDir::cleanPath(path);
    if (root == QLatin1String("/")) return true;
    if (user.isEmpty() || user.contains(QLatin1Char('/'))) return false;
    return root.startsWith(QStringLiteral("/run/media/%1/").arg(user))
        || root.startsWith(QStringLiteral("/media/%1/").arg(user));
}

inline bool visible(const QStorageInfo &storage)
{
    return storage.isValid() && storage.isReady() && storage.bytesTotal() > 0
        && visibleRoot(storage.rootPath(), qEnvironmentVariable("USER"));
}

inline QString recoveryPath(const QString &removedPath, const QString &nearestExisting,
                            const QString &user, const QString &home)
{
    const QString ancestor = QDir::cleanPath(nearestExisting);
    // A deleted folder on an available drive can use its parent. An unmounted
    // drive cannot: its surviving /run/media/<user> container is infrastructure,
    // not the user's home or an Explorer disk location.
    if (QDir::cleanPath(removedPath) != QLatin1String("/") && visibleRoot(removedPath, user)
        && (ancestor == QLatin1String("/") || !visibleRoot(ancestor, user)))
        return home;
    return nearestExisting;
}

inline QString displayName(const QString &volumeLabel, bool systemDisk, int ordinal)
{
    if (systemDisk) return QStringLiteral("Local Disk (C:)");
    QString label = volumeLabel.trimmed();
    // QStorageInfo::displayName may fall back to a raw mount path. Use only
    // the filesystem label, and never disguise an arbitrary device as a CD.
    if (label.isEmpty() || label.contains(QLatin1Char('/')))
        label = QStringLiteral("Removable Disk");
    return ordinal >= 0 && ordinal < 23
        ? QStringLiteral("%1 (%2:)").arg(label, QString(QChar('D' + ordinal)))
        : label; // More mounts must not produce punctuation as drive letters.
}

struct Entry {
    QStorageInfo storage;
    QString root;
    QString name;
    QString icon;
};

inline QStringList removableBreadcrumbs(const QString &path, const QList<Entry> &entries)
{
    const QString cleanPath = QDir::cleanPath(path);
    const Entry *match = nullptr;
    // Use the most specific mounted root. Prefix siblings such as USB-backup
    // are not descendants of USB, and the system root must not relabel home.
    for (const auto &entry : entries) {
        if (entry.root.isEmpty() || entry.root == QLatin1String("/")) continue;
        if (cleanPath != entry.root && !cleanPath.startsWith(entry.root + QLatin1Char('/'))) continue;
        if (!match || entry.root.size() > match->root.size()) match = &entry;
    }
    if (!match) return {};
    QStringList labels{match->name};
    const QString relative = QDir(match->root).relativeFilePath(cleanPath);
    if (relative != QLatin1String(".")) labels.append(relative.split(QLatin1Char('/'), Qt::SkipEmptyParts));
    return labels;
}

inline QList<Entry> mounted()
{
    QList<QStorageInfo> volumes;
    for (const auto &volume : QStorageInfo::mountedVolumes())
        if (visible(volume)) volumes.append(volume);
    std::sort(volumes.begin(), volumes.end(), [](const auto &left, const auto &right) {
        return left.rootPath() < right.rootPath();
    });
    QList<Entry> result;
    int ordinal = 0;
    for (const auto &volume : volumes) {
        const QString root = QDir::cleanPath(volume.rootPath());
        if (!result.isEmpty() && result.constLast().root == root) continue;
        const bool system = root == QLatin1String("/");
        result.append({volume, root, displayName(volume.name(), system, system ? 0 : ordinal++),
                       system ? QStringLiteral("drive-harddisk-root")
                              : QStringLiteral("drive-removable-media")});
    }
    return result;
}
}
