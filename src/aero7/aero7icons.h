/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QIcon>
#include <QString>

namespace Aero7Icons
{
QIcon icon(const QString &name);
QIcon resource(const QString &path, const QString &fallbackName);
}
