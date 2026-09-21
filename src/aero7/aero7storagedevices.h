/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QList>

class KFilePlacesModel;

// Private, owner-scoped native device access for Computer and common dialogs.
// No new members or dependencies are exposed in the public dialog ABI.
class Aero7StorageDevices final : public QObject
{
    Q_OBJECT
public:
    struct Device { QString id; QString name; QString icon; };
    explicit Aero7StorageDevices(QObject *parent);
    QList<Device> unmounted() const;
    void openDevice(const QString &id);
    void cancelPending();

Q_SIGNALS:
    void changed();
    void opened(const QString &root);
    void failed(const QString &message);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    KFilePlacesModel *m_places;
    QPointer<QObject> m_pending;
};
