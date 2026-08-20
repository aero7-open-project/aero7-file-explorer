/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QStorageInfo>
#include <QWidget>

// An Explorer-native Computer surface.  The historical file name is retained
// to keep packaging patches small, but this is intentionally not a dialog.
class Aero7ComputerView final : public QWidget
{
    Q_OBJECT
public:
    explicit Aero7ComputerView(QWidget *parent = nullptr);

    void refresh();
    static bool isUserVisibleStorage(const QStorageInfo &storage);

Q_SIGNALS:
    void openRequested(const QString &rootPath);

private:
    QWidget *m_content = nullptr;
};
