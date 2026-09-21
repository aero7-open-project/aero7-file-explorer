/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aero7libraries.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

int main(int argc, char **argv)
{
    QTemporaryDir profile;
    if (!profile.isValid())
        return 1;
    qputenv("HOME", profile.path().toUtf8());
    qputenv("XDG_CONFIG_HOME", (profile.path() + "/config").toUtf8());
    qputenv("XDG_DATA_HOME", (profile.path() + "/data").toUtf8());
    QCoreApplication app(argc, argv);
    const bool existing = app.arguments().contains(QStringLiteral("existing"));
    if (existing) {
        const QString location = profile.path() + "/My Library";
        QDir().mkpath(location);
        QDir().mkpath(profile.path() + "/config/aero7");
        const QJsonObject library{{"id", "new-library"}, {"name", "My Library"},
                                  {"locations", QJsonArray{location}},
                                  {"saveLocation", location}};
        QFile config(profile.path() + "/config/aero7/libraries.json");
        if (!config.open(QIODevice::WriteOnly)
            || config.write(QJsonDocument(QJsonObject{{"libraries", QJsonArray{library}}}).toJson()) < 0)
            return 2;
    }

    const auto libraries = Aero7Libraries::instance().libraries();
    QStringList ids;
    for (const auto &library : libraries)
        ids.append(library.id);
    const QStringList expected = existing
        ? QStringList{QStringLiteral("new-library")}
        : QStringList{QStringLiteral("documents"), QStringLiteral("music"),
                      QStringLiteral("pictures"), QStringLiteral("videos")};
    if (ids != expected || QDir(profile.path() + "/New Library").exists())
        return 3;
    if (existing && libraries.constFirst().name != QStringLiteral("My Library"))
        return 4;
    return 0;
}
