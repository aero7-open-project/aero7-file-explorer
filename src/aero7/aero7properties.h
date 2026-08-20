/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QList>
#include <QUrl>

class QWidget;

namespace Aero7Properties {
void show(const QList<QUrl> &urls, QWidget *parent = nullptr);
void showTrash(QWidget *parent = nullptr);
void showLibrary(const QString &id, QWidget *parent = nullptr);
}
