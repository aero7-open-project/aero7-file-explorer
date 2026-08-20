/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aero7libraries.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>

#include <algorithm>
#include <utility>

namespace {
QString defaultPath(QStandardPaths::StandardLocation location,
                    const QString &fallback)
{
    const QStringList paths = QStandardPaths::standardLocations(location);
    return paths.isEmpty() ? QDir::home().filePath(fallback) : paths.constFirst();
}

QString safeName(const QString &name)
{
    QString value = name;
    value.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")),
                  QStringLiteral("_"));
    return value.trimmed().isEmpty() ? QStringLiteral("Library") : value.trimmed();
}
}

Aero7Libraries &Aero7Libraries::instance()
{
    static Aero7Libraries service;
    return service;
}

Aero7Libraries::Aero7Libraries()
{
    const QString configRoot = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    const QString dataRoot = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    m_configPath = QDir(configRoot).filePath(QStringLiteral("aero7/libraries.json"));
    m_materializedRoot = QDir(dataRoot).filePath(QStringLiteral("Aero7/Libraries"));
    QString ignored;
    bool needsWrite = false;
    if (!load(&ignored)) {
        m_libraries = defaults();
        needsWrite = true;
    }

    // The Windows 7 reference contains a user-created "New Library" between
    // Music and Pictures. Seed the same functional library for existing Aero7
    // profiles as a one-time migration.
    const bool hasNewLibrary = std::any_of(m_libraries.cbegin(), m_libraries.cend(),
                                           [](const Aero7Library &library) {
        return library.id == QLatin1String("new-library");
    });
    if (!hasNewLibrary) {
        const QString location = QDir::home().filePath(QStringLiteral("New Library"));
        QDir().mkpath(location);
        m_libraries.insert(qMin(2, m_libraries.size()),
                           {QStringLiteral("new-library"), QStringLiteral("New Library"),
                            {location}, location, QStringLiteral("General Items"), true});
        needsWrite = true;
    }
    if (needsWrite)
        write(&ignored);
    refresh(&ignored);
}

QList<Aero7Library> Aero7Libraries::defaults()
{
    return {
        {QStringLiteral("documents"), QStringLiteral("Documents"),
         {defaultPath(QStandardPaths::DocumentsLocation, QStringLiteral("Documents"))},
         defaultPath(QStandardPaths::DocumentsLocation, QStringLiteral("Documents")),
         QStringLiteral("Documents"), true},
        {QStringLiteral("music"), QStringLiteral("Music"),
         {defaultPath(QStandardPaths::MusicLocation, QStringLiteral("Music"))},
         defaultPath(QStandardPaths::MusicLocation, QStringLiteral("Music")),
         QStringLiteral("Music"), true},
        {QStringLiteral("pictures"), QStringLiteral("Pictures"),
         {defaultPath(QStandardPaths::PicturesLocation, QStringLiteral("Pictures"))},
         defaultPath(QStandardPaths::PicturesLocation, QStringLiteral("Pictures")),
         QStringLiteral("Pictures"), true},
        {QStringLiteral("videos"), QStringLiteral("Videos"),
         {defaultPath(QStandardPaths::MoviesLocation, QStringLiteral("Videos"))},
         defaultPath(QStandardPaths::MoviesLocation, QStringLiteral("Videos")),
         QStringLiteral("Videos"), true},
    };
}

QString Aero7Libraries::normalizedLocation(const QString &path)
{
    if (path.trimmed().isEmpty())
        return {};
    QString expanded = path.trimmed();
    if (expanded == QLatin1String("~"))
        expanded = QDir::homePath();
    else if (expanded.startsWith(QLatin1String("~/")))
        expanded = QDir::home().filePath(expanded.mid(2));
    return QDir::cleanPath(QFileInfo(expanded).absoluteFilePath());
}

bool Aero7Libraries::load(QString *error)
{
    QFile file(m_configPath);
    if (!file.exists())
        return false;
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error)
            *error = QStringLiteral("Invalid Libraries file: %1").arg(parseError.errorString());
        return false;
    }

    QList<Aero7Library> loaded;
    const QJsonArray libraries = document.object().value(QStringLiteral("libraries")).toArray();
    for (const QJsonValue &value : libraries) {
        const QJsonObject object = value.toObject();
        Aero7Library library;
        library.id = object.value(QStringLiteral("id")).toString().trimmed().toLower();
        library.name = object.value(QStringLiteral("name")).toString().trimmed();
        library.saveLocation = normalizedLocation(object.value(QStringLiteral("saveLocation")).toString());
        library.optimizeFor = object.value(QStringLiteral("optimizeFor")).toString(QStringLiteral("General Items"));
        library.shownInNavigationPane = object.value(QStringLiteral("shownInNavigationPane")).toBool(true);
        for (const QJsonValue &location : object.value(QStringLiteral("locations")).toArray()) {
            const QString normalized = normalizedLocation(location.toString());
            if (!normalized.isEmpty() && !library.locations.contains(normalized))
                library.locations.append(normalized);
        }
        if (library.id.isEmpty() || library.name.isEmpty() || library.locations.isEmpty())
            continue;
        if (!library.locations.contains(library.saveLocation))
            library.saveLocation = library.locations.constFirst();
        loaded.append(library);
    }
    if (loaded.isEmpty())
        return false;
    m_libraries = loaded;
    return true;
}

bool Aero7Libraries::write(QString *error) const
{
    QJsonArray libraries;
    for (const Aero7Library &library : m_libraries) {
        QJsonArray locations;
        for (const QString &location : library.locations)
            locations.append(location);
        libraries.append(QJsonObject{
            {QStringLiteral("id"), library.id},
            {QStringLiteral("name"), library.name},
            {QStringLiteral("locations"), locations},
            {QStringLiteral("saveLocation"), library.saveLocation},
            {QStringLiteral("optimizeFor"), library.optimizeFor},
            {QStringLiteral("shownInNavigationPane"), library.shownInNavigationPane},
        });
    }
    QJsonObject root{{QStringLiteral("version"), 1},
                     {QStringLiteral("libraries"), libraries}};
    QDir().mkpath(QFileInfo(m_configPath).absolutePath());
    QSaveFile file(m_configPath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

QList<Aero7Library> Aero7Libraries::libraries() const
{
    return m_libraries;
}

Aero7Library Aero7Libraries::library(const QString &id) const
{
    for (const Aero7Library &library : m_libraries)
        if (library.id.compare(id, Qt::CaseInsensitive) == 0)
            return library;
    return {};
}

Aero7Library Aero7Libraries::defaultLibrary(const QString &id) const
{
    for (const Aero7Library &library : defaults())
        if (library.id.compare(id, Qt::CaseInsensitive) == 0)
            return library;
    return {};
}

bool Aero7Libraries::saveLibrary(const Aero7Library &input, QString *error)
{
    Aero7Library validated = input;
    validated.id = validated.id.trimmed().toLower();
    validated.name = validated.name.trimmed();
    QStringList locations;
    for (const QString &location : std::as_const(validated.locations)) {
        const QString normalized = normalizedLocation(location);
        if (!normalized.isEmpty() && !locations.contains(normalized))
            locations.append(normalized);
    }
    validated.locations = locations;
    validated.saveLocation = normalizedLocation(validated.saveLocation);
    if (validated.id.isEmpty() || validated.name.isEmpty() || locations.isEmpty()) {
        if (error)
            *error = QStringLiteral("A Library needs a name and at least one folder.");
        return false;
    }
    if (!locations.contains(validated.saveLocation))
        validated.saveLocation = locations.constFirst();

    bool replaced = false;
    for (Aero7Library &library : m_libraries) {
        if (library.id == validated.id) {
            library = validated;
            replaced = true;
            break;
        }
    }
    if (!replaced)
        m_libraries.append(validated);
    if (!write(error))
        return false;
    return refresh(error);
}

bool Aero7Libraries::restoreDefaults(const QString &id, QString *error)
{
    for (const Aero7Library &library : defaults())
        if (library.id == id)
            return saveLibrary(library, error);
    if (error)
        *error = QStringLiteral("No default definition exists for this Library.");
    return false;
}

QString Aero7Libraries::materializedRoot() const
{
    return m_materializedRoot;
}

QString Aero7Libraries::materializedPath(const QString &id) const
{
    const Aero7Library value = library(id);
    return value.id.isEmpty() ? QString() : QDir(m_materializedRoot).filePath(safeName(value.name));
}

QString Aero7Libraries::libraryIdForPath(const QString &path) const
{
    const QString candidate = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    for (const Aero7Library &library : m_libraries) {
        const QString root = QDir::cleanPath(materializedPath(library.id));
        if (candidate == root || candidate.startsWith(root + QDir::separator()))
            return library.id;
    }
    return {};
}

QString Aero7Libraries::saveLocationForPath(const QString &path) const
{
    const Aero7Library value = library(libraryIdForPath(path));
    return value.saveLocation;
}

QStringList Aero7Libraries::searchRoots(const QString &id) const
{
    QStringList roots;
    for (const QString &path : library(id).locations)
        if (QFileInfo(path).isDir())
            roots.append(path);
    return roots;
}

QString Aero7Libraries::materializeSearch(const QString &id,
                                          const QString &query,
                                          QString *error) const
{
    const QString trimmed = query.trimmed();
    if (trimmed.isEmpty())
        return materializedPath(id);
    const QString cacheRoot = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    const QString resultRoot = QDir(cacheRoot).filePath(
        QStringLiteral("aero7/library-search/%1").arg(id));
    QDir results(resultRoot);
    if (results.exists() && !results.removeRecursively()) {
        if (error) *error = QStringLiteral("Old search results could not be cleared.");
        return {};
    }
    if (!QDir().mkpath(resultRoot)) {
        if (error) *error = QStringLiteral("Search results could not be created.");
        return {};
    }
    QSet<QString> names;
    for (const QString &root : searchRoots(id)) {
        QDirIterator iterator(root, QDir::AllEntries | QDir::NoDotAndDotDot,
                              QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            const QString source = iterator.next();
            const QFileInfo info = iterator.fileInfo();
            if (!info.fileName().contains(trimmed, Qt::CaseInsensitive))
                continue;
            QString name = info.fileName();
            int suffix = 2;
            while (names.contains(name))
                name = QStringLiteral("%1 (%2)").arg(info.fileName()).arg(suffix++);
            names.insert(name);
            QFile::link(source, QDir(resultRoot).filePath(name));
        }
    }
    return resultRoot;
}

bool Aero7Libraries::refresh(QString *error) const
{
    if (!QDir().mkpath(m_materializedRoot)) {
        if (error)
            *error = QStringLiteral("Could not create the Libraries folder.");
        return false;
    }

    for (const Aero7Library &library : m_libraries) {
        const QString targetRoot = materializedPath(library.id);
        QDir().mkpath(targetRoot);
        QDir target(targetRoot);
        const QFileInfoList stale = target.entryInfoList(
            QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
        for (const QFileInfo &entry : stale) {
            if (entry.isSymLink() || entry.fileName().startsWith(QLatin1String("Unavailable - ")))
                QFile::remove(entry.absoluteFilePath());
        }

        QSet<QString> names;
        int unavailable = 0;
        for (const QString &location : library.locations) {
            const QDir source(location);
            if (!source.exists()) {
                const QString markerName = QStringLiteral("Unavailable - %1 (%2).desktop")
                                               .arg(QFileInfo(location).fileName())
                                               .arg(++unavailable);
                QFile marker(target.filePath(markerName));
                if (marker.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                    marker.write("[Desktop Entry]\nType=Link\nIcon=dialog-warning\n");
                    marker.write(QStringLiteral("Name=Unavailable — %1\nURL=file://%2\n")
                                     .arg(QFileInfo(location).fileName(), location)
                                     .toUtf8());
                }
                continue;
            }
            const QFileInfoList entries = source.entryInfoList(
                QDir::AllEntries | QDir::NoDotAndDotDot,
                QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
            for (const QFileInfo &entry : entries) {
                QString name = entry.fileName();
                const QString base = entry.completeBaseName();
                const QString suffix = entry.completeSuffix();
                int copy = 2;
                while (names.contains(name)) {
                    name = suffix.isEmpty()
                        ? QStringLiteral("%1 (%2)").arg(base).arg(copy++)
                        : QStringLiteral("%1 (%2).%3").arg(base).arg(copy++).arg(suffix);
                }
                names.insert(name);
                QFile::link(entry.absoluteFilePath(), target.filePath(name));
            }
        }
    }
    return true;
}
