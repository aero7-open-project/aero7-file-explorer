/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "aero7commondialogs_export.h"

#include <QIcon>
#include <QString>

namespace Aero7Icons
{
AERO7COMMONDIALOGS_EXPORT QIcon icon(const QString &name);
AERO7COMMONDIALOGS_EXPORT QIcon resource(const QString &path, const QString &fallbackName);
}
