/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../aero7/aero7storage.h"
#include "../aero7/aero7mountwatcher.h"
#include "../aero7/aero7devicevisibility.h"
#include <QPointer>
#include <QTemporaryDir>
#include <QTest>

class Aero7StorageTest : public QObject
{
    Q_OBJECT
    static bool snapshot(const QString &path, const QByteArray &contents)
    {
        QFile file(path);
        return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
            && file.write(contents) == contents.size();
    }
private Q_SLOTS:
    void removableBreadcrumbs_data()
    {
        QTest::addColumn<QString>("path");
        QTest::addColumn<QStringList>("expected");
        QTest::newRow("root") << QString("/run/media/test/USB") << QStringList{"USB (D:)"};
        QTest::newRow("child") << QString("/run/media/test/USB/Photos/Trips") << QStringList{"USB (D:)", "Photos", "Trips"};
        QTest::newRow("normalized") << QString("/run/media/test/USB/Photos/../Trips/") << QStringList{"USB (D:)", "Trips"};
        QTest::newRow("nested-mount") << QString("/run/media/test/USB/nested/docs") << QStringList{"Nested (E:)", "docs"};
        QTest::newRow("prefix-sibling") << QString("/run/media/test/USB-backup/docs") << QStringList{};
        QTest::newRow("other-user") << QString("/run/media/other/USB/docs") << QStringList{};
        QTest::newRow("mount-container") << QString("/run/media/test") << QStringList{};
        QTest::newRow("system-root") << QString("/") << QStringList{};
        QTest::newRow("home") << QString("/home/test/docs") << QStringList{};
    }

    void removableBreadcrumbs()
    {
        QFETCH(QString, path);
        QFETCH(QStringList, expected);
        const QList<Aero7Storage::Entry> entries{
            {{}, "/", "Local Disk (C:)", {}},
            {{}, "/run/media/test/USB", "USB (D:)", {}},
            {{}, "/run/media/test/USB/nested", "Nested (E:)", {}}};
        QCOMPARE(Aero7Storage::removableBreadcrumbs(path, entries), expected);
        QVERIFY(Aero7Storage::removableBreadcrumbs(path, {}).isEmpty());
    }

    void removedDriveRecovery_data()
    {
        QTest::addColumn<QString>("removed");
        QTest::addColumn<QString>("ancestor");
        QTest::addColumn<QString>("expected");
        const auto row = [](const char *name, const QString &removed, const QString &ancestor, const QString &expected) {
            QTest::newRow(name) << removed << ancestor << expected;
        };
        row("actual-udisks-unmount", "/run/media/aero7test/AERO7_QA", "/run/media/aero7test", "/home/aero7test");
        row("runtime-user-container-removed", "/run/media/aero7test/USB", "/run/media", "/home/aero7test");
        row("runtime-media-container-removed", "/run/media/aero7test/USB", "/run", "/home/aero7test");
        row("media-unmount", "/media/aero7test/USB", "/media/aero7test", "/home/aero7test");
        row("no-mount-container", "/media/aero7test/USB", "/", "/home/aero7test");
        row("folder-deleted-drive-still-mounted", "/run/media/aero7test/USB/docs", "/run/media/aero7test/USB", "/run/media/aero7test/USB");
        row("normal-user-folder", "/home/aero7test/docs/deleted", "/home/aero7test/docs", "/home/aero7test/docs");
        row("not-a-user-mount-prefix", "/run/media/aero7test-other/USB", "/run/media/aero7test-other", "/run/media/aero7test-other");
        row("root", "/", "/", "/");
    }

    void removedDriveRecovery()
    {
        QFETCH(QString, removed);
        QFETCH(QString, ancestor);
        QFETCH(QString, expected);
        QCOMPARE(Aero7Storage::recoveryPath(removed, ancestor, QStringLiteral("aero7test"), QStringLiteral("/home/aero7test")), expected);
    }

    void nativeIgnorePolicy_data()
    {
        QTest::addColumn<bool>("hasAccess");
        QTest::addColumn<bool>("accessible");
        QTest::addColumn<bool>("accessIgnored");
        QTest::addColumn<bool>("hasVolume");
        QTest::addColumn<bool>("volumeIgnored");
        QTest::addColumn<bool>("expected");
        QTest::newRow("actual-unmounted-usb-empty-path") << true << false << true << true << false << false;
        QTest::newRow("mounted-user-volume") << true << true << false << true << false << false;
        QTest::newRow("mounted-system-path") << true << true << true << true << false << true;
        QTest::newRow("unmounted-hintignore-or-hide") << true << false << true << true << true << true;
        QTest::newRow("mounted-hintignore-or-hide") << true << true << false << true << true << true;
        QTest::newRow("missing-storage-access") << false << false << false << true << false << true;
    }

    void nativeIgnorePolicy()
    {
        QFETCH(bool, hasAccess);
        QFETCH(bool, accessible);
        QFETCH(bool, accessIgnored);
        QFETCH(bool, hasVolume);
        QFETCH(bool, volumeIgnored);
        QFETCH(bool, expected);
        QCOMPARE(Aero7Storage::deviceIgnored(hasAccess, accessible, accessIgnored, hasVolume, volumeIgnored), expected);
    }

    void nativeDeviceDiscovery_data()
    {
        QTest::addColumn<bool>("accessible");
        QTest::addColumn<bool>("visibleMount");
        QTest::addColumn<bool>("ignored");
        QTest::addColumn<bool>("filesystem");
        QTest::addColumn<bool>("removable");
        QTest::addColumn<bool>("expected");
        QTest::newRow("unmounted-usb-filesystem") << false << false << false << true << true << true;
        QTest::newRow("mounted-user-usb") << true << true << false << true << true << true;
        QTest::newRow("mounted-other-user-usb") << true << false << false << true << true << false;
        QTest::newRow("mounted-internal-path") << true << false << false << true << false << false;
        QTest::newRow("unmounted-internal-partition") << false << false << false << true << false << false;
        QTest::newRow("ignored-recovery-volume") << false << false << true << true << true << false;
        QTest::newRow("ignored-mounted-volume") << true << true << true << true << true << false;
        QTest::newRow("unmounted-swap") << false << false << false << false << true << false;
        QTest::newRow("missing-native-access") << false << false << true << false << false << false;
    }

    void nativeDeviceDiscovery()
    {
        QFETCH(bool, accessible);
        QFETCH(bool, visibleMount);
        QFETCH(bool, ignored);
        QFETCH(bool, filesystem);
        QFETCH(bool, removable);
        QFETCH(bool, expected);
        QCOMPARE(Aero7Storage::deviceVisible(accessible, visibleMount, ignored, filesystem, removable), expected);
    }

    void mountAndUnmountNotifyOnlyOnChanges()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        const QString path = folder.filePath("mountinfo");
        QVERIFY(snapshot(path, "root\n"));
        QObject owner;
        int changed = 0;
        new Aero7Storage::MountWatcher(&owner, [&] { ++changed; }, path, 20);
        QTest::qWait(80);
        QCOMPARE(changed, 0);
        QVERIFY(snapshot(path, "root\nusb\n"));
        QTRY_COMPARE(changed, 1);
        QTest::qWait(80);
        QCOMPARE(changed, 1);
        QVERIFY(snapshot(path, "root\n"));
        QTRY_COMPARE(changed, 2);
    }

    void readFailureRetainsLastGoodSnapshot()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        const QString path = folder.filePath("mountinfo");
        QVERIFY(snapshot(path, "root\n"));
        QObject owner;
        int changed = 0;
        new Aero7Storage::MountWatcher(&owner, [&] { ++changed; }, path, 20);
        QVERIFY(QFile::remove(path));
        QTest::qWait(80);
        QCOMPARE(changed, 0);
        QVERIFY(snapshot(path, "root\n"));
        QTest::qWait(80);
        QCOMPARE(changed, 0);
        QVERIFY(snapshot(path, "root\nusb\n"));
        QTRY_COMPARE(changed, 1);
    }

    void initiallyUnavailableSnapshotRecovers()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        const QString path = folder.filePath("mountinfo");
        QObject owner;
        int changed = 0;
        new Aero7Storage::MountWatcher(&owner, [&] { ++changed; }, path, 20);
        QTest::qWait(80);
        QCOMPARE(changed, 0);
        QVERIFY(snapshot(path, "root\n"));
        QTRY_COMPARE(changed, 1);
    }

    void destroyingOwnerCancelsPolling()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        const QString path = folder.filePath("mountinfo");
        QVERIFY(snapshot(path, "root\n"));
        auto *owner = new QObject;
        int changed = 0;
        QPointer<QObject> watcher = new Aero7Storage::MountWatcher(owner, [&] { ++changed; }, path, 20);
        delete owner;
        QVERIFY(watcher.isNull());
        QVERIFY(snapshot(path, "root\nusb\n"));
        QTest::qWait(80);
        QCOMPARE(changed, 0);
    }

    void consumerCanDestroyOwnerDuringNotification()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        const QString path = folder.filePath("mountinfo");
        QVERIFY(snapshot(path, "root\n"));
        QPointer<QObject> owner = new QObject;
        new Aero7Storage::MountWatcher(owner, [&] { delete owner.data(); }, path, 20);
        QVERIFY(snapshot(path, "root\nusb\n"));
        QTRY_VERIFY(owner.isNull());
    }

    void capacityUnits_data()
    {
        QTest::addColumn<qint64>("bytes");
        QTest::addColumn<QString>("expected");
        QTest::newRow("unavailable") << qint64(-1) << QStringLiteral("Unavailable");
        QTest::newRow("empty") << qint64(0) << QStringLiteral("0 bytes");
        QTest::newRow("one-byte") << qint64(1) << QStringLiteral("1 byte");
        QTest::newRow("bytes") << qint64(512) << QStringLiteral("512 bytes");
        QTest::newRow("kilobytes") << qint64(1536) << QStringLiteral("1.5 KB");
        QTest::newRow("small-volume") << qint64(16 * 1024 * 1024) << QStringLiteral("16 MB");
        QTest::newRow("sub-gigabyte") << qint64(512 * 1024 * 1024) << QStringLiteral("512 MB");
        QTest::newRow("one-gigabyte") << qint64(1024LL * 1024 * 1024) << QStringLiteral("1.0 GB");
        QTest::newRow("system-volume") << qint64(62LL * 1024 * 1024 * 1024) << QStringLiteral("62 GB");
        QTest::newRow("terabyte") << qint64(2LL * 1024 * 1024 * 1024 * 1024) << QStringLiteral("2.0 TB");
    }

    void capacityUnits()
    {
        QFETCH(qint64, bytes);
        QFETCH(QString, expected);
        QCOMPARE(Aero7Storage::sizeText(bytes), expected);
    }
};

QTEST_GUILESS_MAIN(Aero7StorageTest)
#include "aero7storagetest.moc"
