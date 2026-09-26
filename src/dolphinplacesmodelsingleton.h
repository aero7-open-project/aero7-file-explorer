/*
 * SPDX-FileCopyrightText: 2018 Kai Uwe Broulik <kde@privat.broulik.de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DOLPHINPLACESMODELSINGLETON_H
#define DOLPHINPLACESMODELSINGLETON_H

#include <QScopedPointer>
#include <QString>
#include <QHash>
#include <QSet>
#include <QUrl>
#include <QVector>

#include <KFilePlacesModel>

/**
 * @brief Dolphin's special-cased KFilePlacesModel
 *
 * It returns the trash's icon based on whether
 * it is full or not.
 */
class DolphinPlacesModel : public KFilePlacesModel
{
    Q_OBJECT

public:
    struct Favorite {
        QString name;
        QUrl url;
        QString icon;
    };

    explicit DolphinPlacesModel(QObject *parent = nullptr);
    ~DolphinPlacesModel() override;

    bool panelsLocked() const;
    void setPanelsLocked(bool locked);
    const QVector<Favorite> &favorites() const { return m_favorites; }
    bool isFavorite(const QUrl &url) const;
    bool addFavorite(const QUrl &url);
    bool removeFavorite(const QUrl &url);
    bool renameFavorite(const QUrl &url, const QString &name);
    void restoreFavorites();
    QModelIndex favoriteIndex(const QUrl &url) const;

    QStringList mimeTypes() const override;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) override;

protected:
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

private Q_SLOTS:
    void slotTrashEmptinessChanged(bool isEmpty);
    void refreshStoragePlaces();

private:
    bool isTrash(const QModelIndex &index) const;
    static QVector<Favorite> defaultFavorites();
    void saveFavorites() const;
    void ensureFavorites();

    bool m_isEmpty = false;
    bool m_panelsLocked = true; // common-case, panels are locked
    QHash<QString, QString> m_storageNames;
    QSet<QString> m_managedDriveRoots;
    QVector<Favorite> m_favorites;
};

/**
 * @brief Provides a global KFilePlacesModel instance.
 */
class DolphinPlacesModelSingleton
{
public:
    static DolphinPlacesModelSingleton &instance();

    DolphinPlacesModel *placesModel() const;

    DolphinPlacesModelSingleton(const DolphinPlacesModelSingleton &) = delete;
    DolphinPlacesModelSingleton &operator=(const DolphinPlacesModelSingleton &) = delete;

private:
    DolphinPlacesModelSingleton();

    QScopedPointer<DolphinPlacesModel> m_placesModel;
};

#endif // DOLPHINPLACESMODELSINGLETON_H
