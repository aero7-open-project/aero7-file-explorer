/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aero7libraries.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QString error;
    if (!Aero7Libraries::instance().refresh(&error))
        return 1;

    QString desktop = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    if (desktop.isEmpty())
        desktop = QDir::home().filePath(QStringLiteral("Desktop"));
    if (!QDir().mkpath(desktop))
        return 2;
    const QString linkPath = QDir(desktop).filePath(QStringLiteral("Recycle Bin.desktop"));
    if (!QFileInfo::exists(linkPath)) {
        QSaveFile link(linkPath);
        if (!link.open(QIODevice::WriteOnly))
            return 3;
        link.write("[Desktop Entry]\n"
                   "Type=Link\n"
                   "Name=Recycle Bin\n"
                   "Icon=user-trash\n"
                   "URL=trash:/\n");
        if (!link.commit())
            return 4;
        QFile::setPermissions(linkPath, QFile::ReadOwner | QFile::WriteOwner
                                          | QFile::ReadGroup | QFile::ReadOther);
    }
    return 0;
}
