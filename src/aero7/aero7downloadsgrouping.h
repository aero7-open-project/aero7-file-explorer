// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QDate>
#include <QDir>
#include <QLocale>
#include <QStandardPaths>
#include <QUrl>

namespace Aero7Downloads {

enum class DateGroup {
    Today,
    Yesterday,
    EarlierThisWeek,
    LastWeek,
    EarlierThisMonth,
    LastMonth,
    EarlierThisYear,
    LongTimeAgo,
};

inline bool isDownloadsLocation(const QUrl &url)
{
    return url.isLocalFile()
        && QDir::cleanPath(url.toLocalFile())
            == QDir::cleanPath(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
}

inline DateGroup dateGroup(const QDate &fileDate, const QDate &today,
                           Qt::DayOfWeek firstDayOfWeek = Qt::Monday)
{
    if (!fileDate.isValid()) return DateGroup::LongTimeAgo;
    if (fileDate >= today) return DateGroup::Today;
    if (fileDate == today.addDays(-1)) return DateGroup::Yesterday;

    const int daysSinceWeekStart = (7 + today.dayOfWeek() - firstDayOfWeek) % 7;
    const QDate thisWeekStart = today.addDays(-daysSinceWeekStart);
    if (fileDate >= thisWeekStart) return DateGroup::EarlierThisWeek;
    if (fileDate >= thisWeekStart.addDays(-7)) return DateGroup::LastWeek;

    const QDate thisMonthStart(today.year(), today.month(), 1);
    if (fileDate >= thisMonthStart) return DateGroup::EarlierThisMonth;
    const QDate lastMonthStart = thisMonthStart.addMonths(-1);
    if (fileDate >= lastMonthStart) return DateGroup::LastMonth;
    if (fileDate.year() == today.year()) return DateGroup::EarlierThisYear;
    return DateGroup::LongTimeAgo;
}

} // namespace Aero7Downloads
