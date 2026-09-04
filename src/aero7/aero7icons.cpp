/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aero7icons.h"

#include <QDebug>
#include <QFile>
#include <QSize>

namespace
{
QString normalized(QString value)
{
    value = value.toLower();
    value.replace(QLatin1Char('_'), QLatin1Char('-'));
    return value;
}

QIcon exactPackIcon(const QString &name)
{
    QIcon icon;
    for (const int size : {8, 16, 22, 24, 32, 36, 48, 64, 72, 96, 128, 192, 256}) {
        const QString path = QStringLiteral(":/aero7/pack/%1/%2.png").arg(size).arg(name);
        if (QFile::exists(path)) icon.addFile(path, QSize(size, size));
    }
    return icon;
}

QString mappedName(const QString &raw)
{
    const QString name = normalized(raw);
    if (name.contains(QStringLiteral("back")) || name.contains(QStringLiteral("undo"))) return QStringLiteral("go-previous");
    if (name.contains(QStringLiteral("forward"))) return QStringLiteral("go-next");
    if (name == QStringLiteral("up")) return QStringLiteral("go-up");
    if (name.contains(QStringLiteral("search")) || name.contains(QStringLiteral("find"))) return QStringLiteral("edit-find");
    if (name.contains(QStringLiteral("refresh"))) return QStringLiteral("view-refresh");
    if (name.contains(QStringLiteral("folder")) || name.contains(QStringLiteral("open"))) return QStringLiteral("folder-open");
    if (name.contains(QStringLiteral("help")) || name.contains(QStringLiteral("question"))) return QStringLiteral("help-contents");
    if (name.contains(QStringLiteral("delete")) || name.contains(QStringLiteral("trash"))) return QStringLiteral("edit-delete");
    if (name.contains(QStringLiteral("close")) || name.contains(QStringLiteral("cancel")) || name.contains(QStringLiteral("stop"))) return QStringLiteral("dialog-cancel");
    if (name.contains(QStringLiteral("copy")) || name.contains(QStringLiteral("duplicate"))) return QStringLiteral("edit-copy");
    if (name.contains(QStringLiteral("cut"))) return QStringLiteral("edit-cut");
    if (name.contains(QStringLiteral("paste"))) return QStringLiteral("edit-paste");
    if (name.contains(QStringLiteral("rename"))) return QStringLiteral("document-properties");
    if (name.contains(QStringLiteral("propert")) || name.contains(QStringLiteral("configure")) || name.contains(QStringLiteral("setting"))) return QStringLiteral("preferences-system");
    if (name.contains(QStringLiteral("warning")) || name.contains(QStringLiteral("admin"))) return QStringLiteral("dialog-warning");
    if (name.contains(QStringLiteral("error"))) return QStringLiteral("dialog-error");
    if (name.contains(QStringLiteral("information")) || name.contains(QStringLiteral("description"))) return QStringLiteral("dialog-information");
    if (name.contains(QStringLiteral("terminal"))) return QStringLiteral("utilities-terminal");
    if (name.contains(QStringLiteral("computer"))) return QStringLiteral("computer");
    if (name.contains(QStringLiteral("view")) || name.contains(QStringLiteral("panel")) || name.contains(QStringLiteral("sort"))) return QStringLiteral("view-list-details");
    if (name.contains(QStringLiteral("bookmark")) || name.contains(QStringLiteral("star")) || name.contains(QStringLiteral("tag"))) return QStringLiteral("bookmarks");
    if (name.contains(QStringLiteral("lock")) || name.contains(QStringLiteral("readonly"))) return QStringLiteral("security-high");
    if (name.contains(QStringLiteral("new")) || name.contains(QStringLiteral("add"))) return QStringLiteral("document-new");
    if (name.contains(QStringLiteral("save"))) return QStringLiteral("document-save");
    if (name.contains(QStringLiteral("play"))) return QStringLiteral("media-playback-start");
    if (name.contains(QStringLiteral("zoom"))) return QStringLiteral("zoom-original");
    return QStringLiteral("system-file-manager");
}
}

QIcon Aero7Icons::icon(const QString &name)
{
    QIcon icon = exactPackIcon(normalized(name));
    if (icon.isNull()) icon = exactPackIcon(mappedName(name));
    if (icon.isNull()) icon = exactPackIcon(QStringLiteral("system-file-manager"));
    if (icon.isNull()) qWarning().noquote() << "[Aero7 Icons] Missing pack icon:" << name;
    return icon;
}

QIcon Aero7Icons::resource(const QString &path, const QString &fallbackName)
{
    const QIcon result(path);
    if (!result.isNull()) return result;
    qWarning().noquote() << "[Aero7 Icons] Missing resource:" << path;
    return icon(fallbackName);
}
