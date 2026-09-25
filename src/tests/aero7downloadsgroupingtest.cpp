// SPDX-License-Identifier: GPL-2.0-or-later
#include "aero7/aero7downloadsgrouping.h"

#include <QTest>

class Aero7DownloadsGroupingTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void dateBuckets()
    {
        using namespace Aero7Downloads;
        const QDate today(2026, 9, 25); // Friday
        QCOMPARE(dateGroup(today, today), DateGroup::Today);
        QCOMPARE(dateGroup(today.addDays(-1), today), DateGroup::Yesterday);
        QCOMPARE(dateGroup(QDate(2026, 9, 21), today), DateGroup::EarlierThisWeek);
        QCOMPARE(dateGroup(QDate(2026, 9, 20), today), DateGroup::LastWeek);
        QCOMPARE(dateGroup(QDate(2026, 9, 14), today), DateGroup::LastWeek);
        QCOMPARE(dateGroup(QDate(2026, 9, 13), today), DateGroup::EarlierThisMonth);
        QCOMPARE(dateGroup(QDate(2026, 8, 31), today), DateGroup::LastMonth);
        QCOMPARE(dateGroup(QDate(2026, 7, 31), today), DateGroup::EarlierThisYear);
        QCOMPARE(dateGroup(QDate(2025, 12, 31), today), DateGroup::LongTimeAgo);
        QCOMPARE(dateGroup(QDate(), today), DateGroup::LongTimeAgo);
    }

    void weekAcrossMonthBoundary()
    {
        using namespace Aero7Downloads;
        const QDate today(2026, 10, 1); // Thursday
        QCOMPARE(dateGroup(QDate(2026, 9, 30), today), DateGroup::Yesterday);
        QCOMPARE(dateGroup(QDate(2026, 9, 28), today), DateGroup::EarlierThisWeek);
        QCOMPARE(dateGroup(QDate(2026, 9, 27), today), DateGroup::LastWeek);
        QCOMPARE(dateGroup(QDate(2026, 9, 20), today), DateGroup::LastMonth);
    }

    void onlyActualDownloadsLocation()
    {
        using namespace Aero7Downloads;
        const QString downloads = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        QVERIFY(isDownloadsLocation(QUrl::fromLocalFile(downloads)));
        QVERIFY(!isDownloadsLocation(QUrl::fromLocalFile(downloads + QStringLiteral("/subfolder"))));
        QVERIFY(!isDownloadsLocation(QUrl::fromLocalFile(QDir::homePath())));
    }
};

QTEST_GUILESS_MAIN(Aero7DownloadsGroupingTest)
#include "aero7downloadsgroupingtest.moc"
