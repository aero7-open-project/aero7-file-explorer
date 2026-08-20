/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QList>
#include <QString>
#include <QStringList>

struct Aero7Library {
    QString id;
    QString name;
    QStringList locations;
    QString saveLocation;
    QString optimizeFor;
    bool shownInNavigationPane = true;
};

class Aero7Libraries final
{
public:
    static Aero7Libraries &instance();

    QList<Aero7Library> libraries() const;
    Aero7Library library(const QString &id) const;
    Aero7Library defaultLibrary(const QString &id) const;
    bool saveLibrary(const Aero7Library &library, QString *error = nullptr);
    bool restoreDefaults(const QString &id, QString *error = nullptr);

    QString materializedRoot() const;
    QString materializedPath(const QString &id) const;
    QString saveLocationForPath(const QString &path) const;
    QString libraryIdForPath(const QString &path) const;
    QStringList searchRoots(const QString &id) const;
    QString materializeSearch(const QString &id, const QString &query,
                              QString *error = nullptr) const;

    bool refresh(QString *error = nullptr) const;

private:
    Aero7Libraries();
    static QList<Aero7Library> defaults();
    bool load(QString *error = nullptr);
    bool write(QString *error = nullptr) const;
    static QString normalizedLocation(const QString &path);

    QList<Aero7Library> m_libraries;
    QString m_configPath;
    QString m_materializedRoot;
};
