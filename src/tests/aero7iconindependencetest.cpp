/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aero7icons.h"

#include <QApplication>
#include <QHash>
#include <QIcon>
#include <QImage>
#include <QStringList>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    const QStringList names = {
        QStringLiteral("back"), QStringLiteral("forward"), QStringLiteral("search"),
        QStringLiteral("view-list-details"), QStringLiteral("folder-new"),
        QStringLiteral("edit-copy"), QStringLiteral("edit-cut"),
        QStringLiteral("edit-delete"), QStringLiteral("document-properties"),
        QStringLiteral("warning"), QStringLiteral("computer"), QStringLiteral("media-eject"),
    };
    QHash<QString, QImage> baseline;
    QIcon::setThemeName(QStringLiteral("Aero7-test-theme"));
    for (const QString &name : names) baseline.insert(name, Aero7Icons::icon(name).pixmap(32, 32).toImage());
    const QImage appIcon = Aero7Icons::icon(QStringLiteral("system-file-manager")).pixmap(64, 64).toImage();
    if (appIcon.isNull()) return 1;
    // Storage actions must use the actual, unchanged pack asset, not the
    // generic File Explorer fallback that also happens to be theme-stable.
    const QImage ejectAsset(QStringLiteral(":/aero7/pack/32/media-eject.png"));
    if (ejectAsset.isNull()
        || baseline.value(QStringLiteral("media-eject")).convertToFormat(QImage::Format_ARGB32_Premultiplied)
            != ejectAsset.convertToFormat(QImage::Format_ARGB32_Premultiplied)) return 5;

    for (const QString &theme : {QStringLiteral("breeze"), QStringLiteral("breeze-dark"),
                                 QStringLiteral("missing-aero7-test-theme")}) {
        QIcon::setThemeName(theme);
        for (const QString &name : names)
            if (Aero7Icons::icon(name).pixmap(32, 32).toImage() != baseline.value(name)) return 2;
        if (Aero7Icons::icon(QStringLiteral("system-file-manager")).pixmap(64, 64).toImage() != appIcon) return 3;
    }
    if (Aero7Icons::resource(QStringLiteral(":/missing/aero7.svg"), QStringLiteral("warning")).isNull()) return 4;
    return 0;
}
