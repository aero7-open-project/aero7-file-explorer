/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aero7commondialog.h"
#include "aero7computerdialog.h"
#include "aero7libraries.h"
#include "aero7storage.h"
#include "aero7dialoglocation.h"

#include <QApplication>
#include <QAction>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileSystemModel>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QListView>
#include <QLayout>
#include <QMessageBox>
#include <QMenu>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <QtTest>
#include <memory>

class CommonDialogTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void concurrentSearchCancellationIsIndependent_data()
    {
        QTest::addColumn<bool>("sameApplication");
        QTest::addColumn<bool>("destroyCaller");
        QTest::newRow("same-app-cancel") << true << false;
        QTest::newRow("same-app-destroy") << true << true;
        QTest::newRow("different-app-cancel") << false << false;
        QTest::newRow("different-app-destroy") << false << true;
    }

    void concurrentSearchCancellationIsIndependent()
    {
        QFETCH(bool, sameApplication);
        QFETCH(bool, destroyCaller);
        QTemporaryDir firstRoot, secondRoot;
        QVERIFY(firstRoot.isValid() && secondRoot.isValid());
        for (int i = 0; i < 512; ++i) {
            QFile first(firstRoot.filePath(QString("first-%1.txt").arg(i)));
            QFile second(secondRoot.filePath(QString("second-%1.txt").arg(i)));
            QVERIFY(first.open(QIODevice::WriteOnly));
            QVERIFY(second.open(QIODevice::WriteOnly));
        }
        const QString firstId = "concurrent-search-first";
        const QString secondId = sameApplication ? firstId : QString("concurrent-search-second");
        auto first = std::make_unique<Aero7CommonDialog>(Aero7CommonDialog::Mode::OpenFile, firstId);
        Aero7CommonDialog second(Aero7CommonDialog::Mode::OpenFile, secondId);
        first->setInitialDirectory(firstRoot.path());
        second.setInitialDirectory(secondRoot.path());
        auto *secondView = second.findChild<QListView *>();
        auto *secondSearch = second.findChild<QObject *>("dialogSearch");
        QVERIFY(secondView && secondSearch);
        first->show();
        second.show();
        QVERIFY(QTest::qWaitForWindowExposed(first.get()));
        QVERIFY(QTest::qWaitForWindowExposed(&second));
        QVERIFY(QMetaObject::invokeMethod(first.get(), "runSearch", Q_ARG(QString, "first-")));
        QVERIFY(QMetaObject::invokeMethod(&second, "runSearch", Q_ARG(QString, "second-")));
        // Both requests are pending concurrently. Closing one caller must not
        // cancel the other's debounce, worker or result model, even for one app ID.
        QVERIFY(first->findChild<QObject *>("dialogSearch")->property("busy").toBool());
        QVERIFY(secondSearch->property("busy").toBool());
        if (destroyCaller) first.reset();
        else first->reject();
        QTRY_VERIFY_WITH_TIMEOUT(!secondSearch->property("busy").toBool(), 5000);
        QCOMPARE(secondView->model()->rowCount(), 512);
        for (int row = 0; row < secondView->model()->rowCount(); ++row) {
            const QModelIndex index = secondView->model()->index(row, 0);
            QVERIFY(index.data().toString().startsWith("second-"));
            QVERIFY(index.data(Qt::UserRole + 10).toString().startsWith(secondRoot.path() + '/'));
        }
        // A newly created request for the vanished/cancelled caller must work
        // without receiving stale results, or altering the still-open dialog.
        Aero7CommonDialog reopened(Aero7CommonDialog::Mode::OpenFile, firstId);
        reopened.setInitialDirectory(firstRoot.path());
        QVERIFY(QMetaObject::invokeMethod(&reopened, "runSearch", Q_ARG(QString, "first-")));
        auto *reopenedSearch = reopened.findChild<QObject *>("dialogSearch");
        QTRY_VERIFY_WITH_TIMEOUT(!reopenedSearch->property("busy").toBool(), 5000);
        auto *reopenedView = reopened.findChild<QListView *>();
        QCOMPARE(reopenedView->model()->rowCount(), 512);
        QVERIFY(reopenedView->model()->index(0, 0).data().toString().startsWith("first-"));
        QCOMPARE(secondView->model()->rowCount(), 512);
        secondView->setCurrentIndex(secondView->model()->index(0, 0));
        const QString selected = secondView->model()->index(0, 0).data(Qt::UserRole + 10).toString();
        QVERIFY(QMetaObject::invokeMethod(&second, "accept"));
        QCOMPARE(second.result(), QDialog::Accepted);
        QCOMPARE(second.selectedFiles(), QStringList{selected});
        QVERIFY(reopened.selectedFiles().isEmpty());
        reopened.reject();
    }

    void concurrentSaveResultsAreIndependent_data()
    {
        QTest::addColumn<bool>("sameApplication");
        QTest::addColumn<bool>("reverseOrder");
        QTest::newRow("same-app-first-second") << true << false;
        QTest::newRow("same-app-second-first") << true << true;
        QTest::newRow("different-app-first-second") << false << false;
        QTest::newRow("different-app-second-first") << false << true;
    }

    void concurrentSaveResultsAreIndependent()
    {
        QFETCH(bool, sameApplication);
        QFETCH(bool, reverseOrder);
        QTemporaryDir firstRoot, secondRoot;
        QVERIFY(firstRoot.isValid() && secondRoot.isValid());
        Aero7CommonDialog first(Aero7CommonDialog::Mode::SaveFile, "concurrent-save-first");
        Aero7CommonDialog second(Aero7CommonDialog::Mode::SaveFile,
            sameApplication ? "concurrent-save-first" : "concurrent-save-second");
        first.setInitialDirectory(firstRoot.path());
        second.setInitialDirectory(secondRoot.path());
        first.setNameFilters({"PNG (*.png)"});
        second.setNameFilters({"Text (*.txt)"});
        first.setSuggestedFileName("first");
        second.setSuggestedFileName("second");
        first.show();
        second.show();
        QVERIFY(QTest::qWaitForWindowExposed(&first));
        QVERIFY(QTest::qWaitForWindowExposed(&second));
        Aero7CommonDialog *early = reverseOrder ? &second : &first;
        Aero7CommonDialog *late = reverseOrder ? &first : &second;
        const QString retainedName = late->findChild<QLineEdit *>("fileName")->text();
        const QString retainedFilter = late->selectedNameFilter();
        QVERIFY(QMetaObject::invokeMethod(early, "accept"));
        QCOMPARE(early->result(), QDialog::Accepted);
        QCOMPARE(late->result(), QDialog::Rejected);
        QVERIFY(late->isVisible());
        QVERIFY(late->selectedFiles().isEmpty());
        QCOMPARE(late->findChild<QLineEdit *>("fileName")->text(), retainedName);
        QCOMPARE(late->selectedNameFilter(), retainedFilter);
        QVERIFY(QMetaObject::invokeMethod(late, "accept"));
        QCOMPARE(first.selectedFiles(), QStringList{firstRoot.filePath("first.png")});
        QCOMPARE(second.selectedFiles(), QStringList{secondRoot.filePath("second.txt")});
        // Dialog acceptance returns paths; it must not create or overwrite files.
        QVERIFY(!QFile::exists(firstRoot.filePath("first.png")));
        QVERIFY(!QFile::exists(secondRoot.filePath("second.txt")));
    }

    void initialKeyboardFocus_data()
    {
        QTest::addColumn<int>("mode");
        QTest::addColumn<QString>("suggested");
        QTest::newRow("save-empty") << int(Aero7CommonDialog::Mode::SaveFile) << QString();
        QTest::newRow("save-suggested") << int(Aero7CommonDialog::Mode::SaveFile) << QString("old.png");
        QTest::newRow("open") << int(Aero7CommonDialog::Mode::OpenFile) << QString();
        QTest::newRow("open-multiple") << int(Aero7CommonDialog::Mode::OpenFiles) << QString();
        QTest::newRow("folder") << int(Aero7CommonDialog::Mode::ChooseFolder) << QString();
    }

    void initialKeyboardFocus()
    {
        QFETCH(int, mode);
        QFETCH(QString, suggested);
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        Aero7CommonDialog dialog(static_cast<Aero7CommonDialog::Mode>(mode), "initial-focus");
        dialog.setInitialDirectory(folder.path());
        dialog.setSuggestedFileName(suggested);
        auto *name = dialog.findChild<QLineEdit *>("fileName");
        auto *view = dialog.findChild<QListView *>();
        QVERIFY(name);
        QVERIFY(view);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        QVERIFY(QTest::qWaitForWindowActive(&dialog));
        QWidget *expected = mode == int(Aero7CommonDialog::Mode::ChooseFolder)
            ? static_cast<QWidget *>(view) : static_cast<QWidget *>(name);
        QCOMPARE(QApplication::focusWidget(), expected);
        if (expected == name) {
            QCOMPARE(name->selectedText(), suggested);
            // Deliver to the actual focused widget, without clicking the field.
            QTest::keyClicks(QApplication::focusWidget(), "new.png");
            QCOMPARE(name->text(), QString("new.png"));
        }
        auto *search = dialog.findChild<QLineEdit *>("search");
        QVERIFY(search);
        search->setFocus();
        dialog.setInitialDirectory(folder.path());
        QCoreApplication::processEvents();
        QCOMPARE(QApplication::focusWidget(), static_cast<QWidget *>(search));
        dialog.reject();
    }

    void breadcrumbOverflowAndLargeFonts_data()
    {
        QTest::addColumn<int>("points");
        QTest::newRow("normal") << 9;
        QTest::newRow("large") << 18;
        QTest::newRow("very-large") << 28;
    }

    void breadcrumbOverflowAndLargeFonts()
    {
        QFETCH(int, points);
        QWidget window;
        auto *layout = new QVBoxLayout(&window);
        QFont font = window.font();
        font.setPointSize(points);
        window.setFont(font);
        QString navigated;
        auto *edit = new QLineEdit;
        auto *bar = new Aero7DialogLocation::Bar(edit, &window,
            [&](const QString &path) { navigated = path; });
        layout->addWidget(bar);
        const QString path = "/tmp/Projects/Release/Artwork/Reviews/September/"
            + QString(180, QLatin1Char('W'));
        bar->setLocation(path);
        window.resize(360, 160);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *scroll = bar->findChild<QScrollArea *>();
        QVERIFY(scroll);
        QToolButton *last = nullptr;
        for (auto *button : bar->findChildren<QToolButton *>())
            if (button->property("destination").toString() == path) last = button;
        QVERIFY(last);
        // Large fonts must not be clipped by a fixed 27-pixel address bar.
        QTRY_VERIFY(scroll->viewport()->height() >= last->sizeHint().height());
        // The current component, including its dropdown, fits the viewport.
        QTRY_VERIFY(last->width() <= scroll->viewport()->width());
        QTRY_VERIFY(scroll->viewport()->rect().contains(
            QRect(last->mapTo(scroll->viewport(), QPoint()), last->size())));
        for (auto *button : scroll->findChildren<QToolButton *>()) {
            if (button->isVisible())
                QTRY_VERIFY(scroll->viewport()->rect().contains(
                    QRect(button->mapTo(scroll->viewport(), QPoint()), button->size())));
        }
        QCOMPARE(last->accessibleName(), QString(180, QLatin1Char('W')));
        auto *overflow = bar->findChild<QToolButton *>("locationAncestors");
        QVERIFY(overflow);
        QTRY_VERIFY(overflow->isVisible());
        QVERIFY(overflow->menu());
        QVERIFY(QMetaObject::invokeMethod(overflow->menu(), "aboutToShow"));
        QAction *ancestor = nullptr;
        for (auto *action : overflow->menu()->actions())
            if (action->data().toString() == "/tmp/Projects") ancestor = action;
        QVERIFY(ancestor);
        ancestor->trigger();
        QCOMPARE(navigated, QString("/tmp/Projects"));
        bar->setLocation("/");
        QTRY_VERIFY(!overflow->isVisible());
        QVERIFY(edit->isHidden());
    }

    void breadcrumbLibraryAndMountedPaths()
    {
        auto &libraries = Aero7Libraries::instance();
        const QString root = libraries.materializedPath("documents");
        const auto library = Aero7DialogLocation::crumbs(root + "/Report & Notes", {});
        QCOMPARE(library.size(), 3);
        QCOMPARE(library[0].label, QString("Libraries"));
        QCOMPARE(library[0].path, libraries.materializedRoot());
        QCOMPARE(library[1].label, libraries.library("documents").name);
        QCOMPARE(library[2].path, root + "/Report & Notes");
        const QList<Aero7Storage::Entry> mounts{{{}, "/run/media/test/USB", "Work (D:)", {}},
            {{}, "/run/media/test/USB/nested", "Nested (E:)", {}}};
        const auto mounted = Aero7DialogLocation::crumbs("/run/media/test/USB/nested/Reports", mounts);
        QCOMPARE(mounted.size(), 2);
        QCOMPARE(mounted[0].label, QString("Nested (E:)"));
        QCOMPARE(mounted[1].path, QString("/run/media/test/USB/nested/Reports"));
        const auto sibling = Aero7DialogLocation::crumbs("/run/media/test/USB-backup", mounts);
        QCOMPARE(sibling[0].label, QString("Local Disk (C:)"));
        QCOMPARE(Aero7DialogLocation::crumbs("/", {}).size(), 1);
    }

    void breadcrumbNavigationAndAddressCancel()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        QVERIFY(QDir(folder.path()).mkdir("Child & Notes"));
        Aero7CommonDialog dialog(Aero7CommonDialog::Mode::SaveFile, "breadcrumb-navigation");
        dialog.setInitialDirectory(folder.filePath("Child & Notes"));
        dialog.setSuggestedFileName("keep.png");
        auto *bar = dialog.findChild<QWidget *>("locationBreadcrumbs");
        auto *path = dialog.findChild<QLineEdit *>("location");
        QVERIFY(bar);
        QVERIFY(path);
        QSignalSpy accepted(&dialog, &QDialog::accepted);
        QSignalSpy rejected(&dialog, &QDialog::rejected);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        QVERIFY(QTest::qWaitForWindowActive(&dialog));
        QVERIFY(!path->isVisible());
        QToolButton *parent = nullptr;
        for (auto *button : bar->findChildren<QToolButton *>())
            if (button->property("destination").toString() == folder.path()) parent = button;
        QVERIFY(parent);
        parent->click();
        QCOMPARE(path->text(), folder.path());
        QCOMPARE(accepted.count(), 0);
        QTest::keyClick(&dialog, Qt::Key_L, Qt::ControlModifier);
        QTRY_VERIFY(path->isVisible());
        path->setText("/not-a-real-location");
        QTest::keyClick(path, Qt::Key_Escape);
        QVERIFY(!path->isVisible());
        QCOMPARE(path->text(), folder.path());
        QCOMPARE(rejected.count(), 0);
        QCOMPARE(dialog.findChild<QLineEdit *>("fileName")->text(), QString("keep.png"));
    }

    void unavailableDeviceDoesNotNavigateOrAccept_data()
    {
        QTest::addColumn<int>("input");
        QTest::newRow("single-click") << 0;
        QTest::newRow("enter") << 1;
    }

    void unavailableDeviceDoesNotNavigateOrAccept()
    {
        QFETCH(int, input);
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        Aero7CommonDialog dialog(Aero7CommonDialog::Mode::SaveFile, "unavailable-device");
        dialog.setInitialDirectory(folder.path());
        dialog.setSuggestedFileName("keep-this-name.png");
        auto *tree = dialog.findChild<QTreeWidget *>();
        auto *path = dialog.findChild<QLineEdit *>("location");
        auto *devices = dialog.findChild<QObject *>("aero7StorageDevices");
        QVERIFY(tree);
        QVERIFY(path);
        QVERIFY(devices);
        QSignalSpy opened(devices, SIGNAL(opened(QString)));
        QSignalSpy accepted(&dialog, &QDialog::accepted);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        // A stale device identity must not be interpreted as an empty path,
        // the working directory, or a request to accept the default button.
        auto *stale = new QTreeWidgetItem(tree, {"Removed QA device"});
        stale->setData(0, Qt::UserRole + 12, "aero7-test-nonexistent-device");
        tree->setCurrentItem(stale);
        tree->scrollToItem(stale);
        bool warned = false;
        QTimer dismiss;
        connect(&dismiss, &QTimer::timeout, &dialog, [&] {
            if (auto *message = dialog.findChild<QMessageBox *>()) {
                warned = message->text().contains("no longer available");
                message->accept();
            }
        });
        dismiss.start(10);
        if (input == 1) QTest::keyClick(tree, Qt::Key_Return);
        else QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, tree->visualItemRect(stale).center());
        QVERIFY(warned);
        QCOMPARE(opened.count(), 0);
        QCOMPARE(accepted.count(), 0);
        QCOMPARE(path->text(), folder.path());
        QCOMPARE(dialog.findChild<QLineEdit *>("fileName")->text(), QString("keep-this-name.png"));
        QVERIFY(dialog.selectedFiles().isEmpty());
    }

    void navigationDoesNotAcceptSaveDialog_data()
    {
        QTest::addColumn<int>("input");
        QTest::newRow("single-click") << 0;
        QTest::newRow("enter") << 1;
        QTest::newRow("address-enter") << 2;
    }

    void navigationDoesNotAcceptSaveDialog()
    {
        QFETCH(int, input);
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        Aero7CommonDialog dialog(Aero7CommonDialog::Mode::SaveFile, "navigation-not-save");
        dialog.setInitialDirectory(folder.path());
        dialog.setSuggestedFileName("navigation-fixture.png");
        auto *tree = dialog.findChild<QTreeWidget *>();
        auto *path = dialog.findChild<QLineEdit *>("location");
        QVERIFY(tree);
        QVERIFY(path);
        QTreeWidgetItem *root = nullptr;
        for (int i = 0; i < tree->topLevelItemCount(); ++i)
            for (int j = 0; j < tree->topLevelItem(i)->childCount(); ++j) {
                auto *candidate = tree->topLevelItem(i)->child(j);
                if (candidate->data(0, Qt::UserRole + 10).toString() == "/") root = candidate;
            }
        QVERIFY(root);
        QSignalSpy accepted(&dialog, &QDialog::accepted);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        tree->scrollToItem(root);
        if (input == 2) {
            path->setText("/");
            path->setFocus();
            QTest::keyClick(path, Qt::Key_Return);
        } else if (input == 1) {
            tree->setCurrentItem(root);
            tree->setFocus();
            QTest::keyClick(tree, Qt::Key_Return);
        } else {
            QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, tree->visualItemRect(root).center());
        }
        QCOMPARE(path->text(), QString("/"));
        QCOMPARE(accepted.count(), 0);
        QVERIFY(dialog.selectedFiles().isEmpty());
        QCOMPARE(dialog.findChild<QLineEdit *>("fileName")->text(), QString("navigation-fixture.png"));
    }

    void unavailableLocationHidesCachedFiles_data()
    {
        QTest::addColumn<int>("mode");
        QTest::newRow("save") << int(Aero7CommonDialog::Mode::SaveFile);
        QTest::newRow("open") << int(Aero7CommonDialog::Mode::OpenFile);
        QTest::newRow("folder") << int(Aero7CommonDialog::Mode::ChooseFolder);
    }

    void unavailableLocationHidesCachedFiles()
    {
        QFETCH(int, mode);
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        QFile file(folder.filePath("cached.txt"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        Aero7CommonDialog dialog(static_cast<Aero7CommonDialog::Mode>(mode), "unavailable-cached");
        dialog.setInitialDirectory(folder.path());
        dialog.setSuggestedFileName("keep.png");
        auto *area = dialog.findChild<QStackedWidget *>("fileArea");
        auto *view = dialog.findChild<QListView *>();
        auto *search = dialog.findChild<QLineEdit *>("search");
        QVERIFY(area);
        QVERIFY(view);
        QVERIFY(search);
        QCOMPARE(area->currentWidget(), view);
        // Keep the old model's rows available to reproduce the stale cache.
        // A real device transition is exercised separately in the VM.
        dialog.setProperty("_aero7BrowseDevice", QByteArray("removed-device"));
        QVERIFY(QMetaObject::invokeMethod(&dialog, "refreshStorageNavigation"));
        QCOMPARE(area->currentWidget()->objectName(), QString("unavailableLocation"));
        QVERIFY(!search->isEnabled());
        QVERIFY(view->selectionModel()->selectedIndexes().isEmpty());
        QCOMPARE(dialog.findChild<QLineEdit *>("fileName")->text(), QString("keep.png"));
        bool warned = false;
        QTimer::singleShot(0, &dialog, [&] {
            auto *message = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(message);
            warned = message->text().contains("no longer available");
            message->accept();
        });
        QVERIFY(QMetaObject::invokeMethod(&dialog, "accept"));
        QVERIFY(warned);
        QVERIFY(dialog.selectedFiles().isEmpty());
        dialog.setInitialDirectory(folder.path());
        QCOMPARE(area->currentWidget(), view);
        QVERIFY(search->isEnabled());
        QCOMPARE(dialog.findChild<QLineEdit *>("fileName")->text(), QString("keep.png"));
        QVERIFY(file.exists());
        QVERIFY(!QFileInfo::exists(folder.filePath("keep.png")));
    }

    void unavailableSaveLocation_data()
    {
        QTest::addColumn<bool>("directoryRemoved");
        QTest::newRow("removed-mount-point") << true;
        QTest::newRow("mount-point-on-different-device") << false;
    }

    void unavailableSaveLocation()
    {
        QFETCH(bool, directoryRemoved);
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        QVERIFY(QDir(folder.path()).mkdir("destination"));
        const QString destination = folder.filePath("destination");
        Aero7CommonDialog dialog(Aero7CommonDialog::Mode::SaveFile, "unavailable-save");
        dialog.setInitialDirectory(destination);
        dialog.setSuggestedFileName("keep.png");
        QPushButton *save = nullptr;
        QPushButton *newFolder = nullptr;
        for (auto *button : dialog.findChildren<QPushButton *>()) {
            if (button->text() == "Save") save = button;
            if (button->text() == "New folder") newFolder = button;
        }
        QVERIFY(save);
        QVERIFY(newFolder);
        QVERIFY(save->isEnabled());
        if (directoryRemoved) {
            QVERIFY(QDir(folder.path()).rmdir("destination"));
        } else {
            // Simulate the recorded device no longer backing this still-existing
            // directory. Real mount transitions are covered separately in the VM.
            dialog.setProperty("_aero7SaveDevice", QByteArray("removed-device"));
        }
        QVERIFY(QMetaObject::invokeMethod(&dialog, "refreshStorageNavigation"));
        QVERIFY(!save->isEnabled());
        int messages = 0;
        const auto dismiss = [&] {
            // A platform theme may expose both its native message box and the
            // Qt wrapper as top-level widgets. Dismiss the active user-facing
            // modal once; enumerating both counts the same warning twice.
            auto *message = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(message);
            QVERIFY(message->text().contains("no longer available"));
            ++messages;
            message->accept();
        };
        QTimer::singleShot(0, &dialog, dismiss);
        QVERIFY(QMetaObject::invokeMethod(&dialog, "accept"));
        QCOMPARE(messages, 1);
        QVERIFY(dialog.selectedFiles().isEmpty());
        QCOMPARE(dialog.findChild<QLineEdit *>("fileName")->text(), QString("keep.png"));
        QTimer::singleShot(0, &dialog, dismiss);
        newFolder->click();
        QCOMPARE(messages, 2);
        QVERIFY(!QFileInfo::exists(QDir(destination).filePath("New folder")));
        QVERIFY(!QFileInfo::exists(QDir(destination).filePath("keep.png")));
        if (directoryRemoved) QVERIFY(QDir(folder.path()).mkdir("destination"));
        dialog.setInitialDirectory(destination);
        QVERIFY(save->isEnabled());
    }

    void storageRefreshDoesNotRestartSearchOrClearSelection()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        QFile file(folder.filePath("match.png"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        Aero7CommonDialog dialog(Aero7CommonDialog::Mode::SaveFile, "mount-refresh-search");
        dialog.setInitialDirectory(folder.path());
        auto *searchField = dialog.findChild<QLineEdit *>("search");
        auto *view = dialog.findChild<QListView *>();
        QVERIFY(searchField);
        QVERIFY(view);
        searchField->setText("match");
        auto *search = dialog.findChild<QObject *>("dialogSearch");
        QVERIFY(search);
        QTRY_VERIFY(!search->property("busy").toBool());
        QCOMPARE(view->model()->rowCount(), 1);
        const auto *model = view->model();
        const auto *selection = view->selectionModel();
        view->selectionModel()->select(model->index(0, 0), QItemSelectionModel::ClearAndSelect);
        dialog.setSuggestedFileName("keep.png");
        QVERIFY(QMetaObject::invokeMethod(&dialog, "refreshStorageNavigation"));
        QCOMPARE(view->model(), model);
        QCOMPARE(view->selectionModel(), selection);
        QCOMPARE(view->selectionModel()->selectedIndexes().size(), 1);
        QCOMPARE(searchField->text(), QString("match"));
        QCOMPARE(dialog.findChild<QLineEdit *>("fileName")->text(), QString("keep.png"));
        QVERIFY(!search->property("busy").toBool());
    }

    void storageViewsOwnTheirMountWatchers()
    {
        Aero7ComputerView computer;
        Aero7CommonDialog dialog(Aero7CommonDialog::Mode::OpenFile, "mount-watchers");
        for (QObject *surface : {static_cast<QObject *>(&computer), static_cast<QObject *>(&dialog)}) {
            auto *watcher = surface->findChild<QObject *>("aero7MountWatcher");
            QVERIFY(watcher);
            auto *timer = watcher->findChild<QTimer *>();
            QVERIFY(timer);
            QVERIFY(timer->isActive());
            QCOMPARE(timer->interval(), 1000);
        }
    }

    void storageRefreshPreservesDialogContext()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        QVERIFY(QDir(folder.path()).mkdir("nested"));
        Aero7CommonDialog dialog(Aero7CommonDialog::Mode::SaveFile, "mount-refresh-context");
        dialog.setInitialDirectory(folder.path());
        QVERIFY(QMetaObject::invokeMethod(&dialog, "setDirectory", Q_ARG(QString, folder.filePath("nested"))));
        dialog.setNameFilters({"Images (*.png)", "All files (*)"});
        dialog.selectNameFilter("Images (*.png)");
        QCOMPARE(dialog.selectedNameFilter(), QString("Images (*.png)"));
        dialog.setSuggestedFileName("keep.png");
        auto *navigation = dialog.findChild<QTreeWidget *>();
        auto *fileModel = dialog.findChild<QFileSystemModel *>();
        QVERIFY(navigation);
        QVERIFY(fileModel);
        QTreeWidgetItem *computer = nullptr;
        for (int i = 0; i < navigation->topLevelItemCount(); ++i)
            if (navigation->topLevelItem(i)->text(0) == "Computer") computer = navigation->topLevelItem(i);
        QVERIFY(computer);
        QVERIFY(computer->childCount() > 0);
        auto *root = computer->child(0);
        auto *favorites = navigation->topLevelItem(0);
        computer->setExpanded(false);
        navigation->setCurrentItem(root);
        auto *stale = new QTreeWidgetItem(computer, {"Disconnected test volume"});
        stale->setData(0, Qt::UserRole + 10, "/not-a-mounted-volume");
        const int count = computer->childCount();
        QSignalSpy filterChanges(&dialog, &Aero7CommonDialog::filterChanged);
        QVERIFY(QMetaObject::invokeMethod(&dialog, "refreshStorageNavigation"));
        QCOMPARE(computer->childCount(), count - 1);
        QCOMPARE(navigation->currentItem(), root);
        QCOMPARE(navigation->topLevelItem(0), favorites);
        QVERIFY(!computer->isExpanded());
        QCOMPARE(dialog.findChild<QLineEdit *>("fileName")->text(), QString("keep.png"));
        QCOMPARE(dialog.selectedNameFilter(), QString("Images (*.png)"));
        QCOMPARE(filterChanges.count(), 0);
        QCOMPARE(fileModel->rootPath(), folder.filePath("nested"));
        QVERIFY(QMetaObject::invokeMethod(&dialog, "navigateBack"));
        QCOMPARE(fileModel->rootPath(), folder.path());
        QVERIFY(QMetaObject::invokeMethod(&dialog, "navigateForward"));
        QCOMPARE(fileModel->rootPath(), folder.filePath("nested"));
    }

    void storageRefreshClearsOnlyRemovedDriveSelection()
    {
        Aero7CommonDialog dialog(Aero7CommonDialog::Mode::SaveFile, "mount-refresh-selection");
        dialog.setSuggestedFileName("keep.png");
        auto *navigation = dialog.findChild<QTreeWidget *>();
        QVERIFY(navigation);
        QTreeWidgetItem *computer = nullptr;
        for (int i = 0; i < navigation->topLevelItemCount(); ++i)
            if (navigation->topLevelItem(i)->text(0) == "Computer") computer = navigation->topLevelItem(i);
        QVERIFY(computer);
        auto *stale = new QTreeWidgetItem(computer, {"Disconnected test volume"});
        stale->setData(0, Qt::UserRole + 10, "/not-a-mounted-volume");
        navigation->setCurrentItem(stale);
        QVERIFY(QMetaObject::invokeMethod(&dialog, "refreshStorageNavigation"));
        QVERIFY(!navigation->currentItem());
        auto *favorite = navigation->topLevelItem(0)->child(0);
        navigation->setCurrentItem(favorite);
        QVERIFY(QMetaObject::invokeMethod(&dialog, "refreshStorageNavigation"));
        QCOMPARE(navigation->currentItem(), favorite);
        QCOMPARE(dialog.findChild<QLineEdit *>("fileName")->text(), QString("keep.png"));
    }

    void driveNamesDoNotLeakMountPathsOrInventLetters()
    {
        QCOMPARE(Aero7Storage::displayName("ignored", true, 0), QString("Local Disk (C:)"));
        QCOMPARE(Aero7Storage::displayName(" Backup ", false, 0), QString("Backup (D:)"));
        QCOMPARE(Aero7Storage::displayName("", false, 1), QString("Removable Disk (E:)"));
        QCOMPARE(Aero7Storage::displayName("/run/media/test/disk", false, 0), QString("Removable Disk (D:)"));
        QCOMPARE(Aero7Storage::displayName("Backup", false, 22), QString("Backup (Z:)"));
        QCOMPARE(Aero7Storage::displayName("Backup", false, 23), QString("Backup"));
    }

    void visibleDrivesExcludeOtherUsersAndInternalMounts()
    {
        QVERIFY(Aero7Storage::visibleRoot("/", "test"));
        QVERIFY(Aero7Storage::visibleRoot("/run/media/test/disk", "test"));
        QVERIFY(Aero7Storage::visibleRoot("/media/test/disk", "test"));
        for (const auto &path : {"/boot", "/proc", "/sys", "/run", "/home", "/mnt/qa",
                                 "/run/media/other/disk", "/run/media/test-extra/disk",
                                 "/run/media/test/../other/disk", "/media/test"})
            QVERIFY2(!Aero7Storage::visibleRoot(QString::fromLatin1(path), "test"), path);
        QVERIFY(!Aero7Storage::visibleRoot("/run/media//disk", ""));
        QVERIFY(!Aero7Storage::visible(QStorageInfo()));
    }

    void onlyExplicitAero7StartupMountsAreVisible()
    {
        const QByteArray fstab =
            "# UUID=ignored /mnt/aero7-comment ext4 defaults,x-aero7-managed 0 0\n"
            "UUID=data /mnt/aero7-geeked_ass_drive ext4 defaults,nofail,x-gvfs-show,x-aero7-managed 0 0\n"
            "UUID=other /mnt/ordinary ext4 defaults,x-aero7-managed 0 0\n"
            "UUID=hidden /mnt/aero7-hidden ext4 defaults 0 0\n"
            "UUID=escape /mnt/aero7-../private ext4 defaults,x-aero7-managed 0 0\n"
            "/dev/sdb1 /mnt/aero7-device ext4 defaults,x-aero7-managed 0 0\n";
        const QSet<QString> managed = Aero7Storage::managedStartupMountRoots(fstab);
        QCOMPARE(managed, QSet<QString>{QStringLiteral("/mnt/aero7-geeked_ass_drive")});
        QVERIFY(Aero7Storage::visibleRoot("/mnt/aero7-geeked_ass_drive", "test", managed));
        QVERIFY(!Aero7Storage::visibleRoot("/mnt/aero7-hidden", "test", managed));
        QVERIFY(!Aero7Storage::visibleRoot("/mnt/ordinary", "test", managed));
    }

    void computerSystemDriveOpensRoot()
    {
        Aero7ComputerView computer;
        QSignalSpy opened(&computer, &Aero7ComputerView::openRequested);
        QPushButton *systemDrive = nullptr;
        for (auto *button : computer.findChildren<QPushButton *>())
            if (button->text() == "Local Disk (C:)") systemDrive = button;
        QVERIFY(systemDrive);
        systemDrive->click();
        QCOMPARE(opened.size(), 1);
        QCOMPARE(opened.constFirst().constFirst().toString(), QString("/"));
    }

    void computerDoesNotInventMountedDevices()
    {
        Aero7ComputerView computer;
        int expected = 0;
        for (const auto &storage : QStorageInfo::mountedVolumes())
            if (Aero7ComputerView::isUserVisibleStorage(storage)) ++expected;
        for (int pass = 0; pass < 2; ++pass) {
            int mounted = 0;
            QSet<QString> deviceIds;
            for (auto *button : computer.findChildren<QPushButton *>("computerDrive")) {
                const QString id = button->property("deviceId").toString();
                if (id.isEmpty()) { ++mounted; continue; }
                QVERIFY(button->property("storageRoot").toString().isEmpty());
                QVERIFY(!deviceIds.contains(id));
                deviceIds.insert(id);
                QVERIFY(!button->parentWidget()->findChild<QWidget *>("driveCapacity"));
            }
            QCOMPARE(mounted, expected);
            computer.refresh();
        }
    }

    void searchDeduplicatesOverlappingRootsAndReportsUnavailableLocations()
    {
        auto &libraries = Aero7Libraries::instance();
        const Aero7Library original = libraries.library("documents");
        const auto restore = qScopeGuard([&] { libraries.saveLibrary(original); });
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        QVERIFY(QDir(folder.path()).mkdir("nested"));
        QFile file(folder.filePath("nested/match.png"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        Aero7Library library = original;
        library.locations = {folder.path(), folder.filePath("nested"), folder.filePath("unmounted")};
        library.saveLocation = folder.path();
        QString error;
        QVERIFY2(libraries.saveLibrary(library, &error), qPrintable(error));
        Aero7CommonDialog dialog(Aero7CommonDialog::Mode::OpenFile, "search-roots");
        dialog.setInitialDirectory(libraries.materializedPath("documents"));
        QVERIFY(QMetaObject::invokeMethod(&dialog, "runSearch", Q_ARG(QString, "match")));
        auto *search = dialog.findChild<QObject *>("dialogSearch");
        QVERIFY(search);
        QTRY_VERIFY(!search->property("busy").toBool());
        QCOMPARE(dialog.findChild<QListView *>()->model()->rowCount(), 1);
        QVERIFY(dialog.findChild<QLabel *>("searchStatus")->text().contains("1 folder unavailable"));
    }

    void repeatedSearchNavigationDoesNotAccumulateSelectionModels()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        Aero7CommonDialog dialog(Aero7CommonDialog::Mode::OpenFile, "search-selection-models");
        auto *view = dialog.findChild<QListView *>();
        dialog.setInitialDirectory(folder.path());
        const auto before = view->findChildren<QItemSelectionModel *>().size();
        for (int i = 0; i < 20; ++i) {
            QVERIFY(QMetaObject::invokeMethod(&dialog, "runSearch", Q_ARG(QString, "match")));
            dialog.setInitialDirectory(folder.path());
        }
        QCOMPARE(view->findChildren<QItemSelectionModel *>().size(), before);
    }

    void searchInsideLibraryFolderStaysInsideThatFolder()
    {
        auto &libraries = Aero7Libraries::instance();
        const QString realRoot = libraries.library("documents").saveLocation;
        QVERIFY(QDir(realRoot).mkdir("scoped-search"));
        for (const QString &relative : {QString("scope-outside.png"), QString("scoped-search/scope-inside.png")}) {
            QFile file(QDir(realRoot).filePath(relative));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        QString error;
        QVERIFY2(libraries.refresh(&error), qPrintable(error));
        Aero7CommonDialog dialog(Aero7CommonDialog::Mode::OpenFile, "search-scope");
        dialog.setInitialDirectory(QDir(libraries.materializedPath("documents")).filePath("scoped-search"));
        QVERIFY(QMetaObject::invokeMethod(&dialog, "runSearch", Q_ARG(QString, "scope-")));
        auto *view = dialog.findChild<QListView *>();
        QTRY_COMPARE(view->model()->rowCount(), 1);
        QCOMPARE(view->model()->index(0, 0).data().toString(), QString("scope-inside.png"));
        QCOMPARE(view->model()->index(0, 0).data(Qt::UserRole + 10).toString(),
                 QDir(realRoot).filePath("scoped-search/scope-inside.png"));
    }

    void organizeMenuHasWorkingLayoutSelectionAndHiddenActions()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        for (const QString &name : {QString("match.txt"), QString(".match-hidden.txt")}) {
            QFile file(folder.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        Aero7CommonDialog dialog(Aero7CommonDialog::Mode::OpenFiles, "organize");
        dialog.setInitialDirectory(folder.path());
        auto *organize = dialog.findChild<QPushButton *>("organize");
        QVERIFY(organize && organize->menu());
        auto *hidden = dialog.findChild<QAction *>("showHidden");
        QVERIFY(hidden);
        QVERIFY(QMetaObject::invokeMethod(&dialog, "runSearch", Q_ARG(QString, "match")));
        auto *view = dialog.findChild<QListView *>();
        QTRY_COMPARE(view->model()->rowCount(), 1);
        hidden->setChecked(true);
        QTRY_COMPARE(view->model()->rowCount(), 2);
        QAction *selectAll = nullptr, *list = nullptr;
        for (auto *action : organize->menu()->actions()) {
            if (action->text() == "Select all") selectAll = action;
            if (action->menu())
                for (auto *layout : action->menu()->actions())
                    if (layout->text() == "List") list = layout;
        }
        QVERIFY(selectAll && list);
        selectAll->trigger();
        QCOMPARE(view->selectionModel()->selectedIndexes().size(), 2);
        list->trigger();
        QCOMPARE(view->viewMode(), QListView::ListMode);
        QVERIFY(!(view->model()->flags(view->model()->index(0, 0)) & Qt::ItemIsEditable));
        hidden->setChecked(false);
        QTRY_COMPARE(view->model()->rowCount(), 1);
    }

    void cancellingActiveSearchDoesNotDeliverStaleRows()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        for (int i = 0; i < 1800; ++i) {
            QFile file(folder.filePath(QString("match-%1.txt").arg(i)));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        auto *dialog = new Aero7CommonDialog(Aero7CommonDialog::Mode::OpenFile, "search-active");
        dialog->setInitialDirectory(folder.path());
        auto *view = dialog->findChild<QListView *>();
        QVERIFY(QMetaObject::invokeMethod(dialog, "runSearch", Q_ARG(QString, "match")));
        QTRY_VERIFY(view->model()->rowCount() > 0);
        QVERIFY(QMetaObject::invokeMethod(dialog, "runSearch", Q_ARG(QString, "no-such-match")));
        auto *search = dialog->findChild<QObject *>("dialogSearch");
        QVERIFY(search);
        QTRY_VERIFY(!search->property("busy").toBool());
        QCOMPARE(view->model()->rowCount(), 0);
        QVERIFY(QMetaObject::invokeMethod(dialog, "runSearch", Q_ARG(QString, "match")));
        QTRY_VERIFY(view->model()->rowCount() > 0);
        delete dialog;
        // Queued result delivery must be safely disconnected after destruction.
        QTest::qWait(200);
    }

    void searchDoesNotFollowDirectorySymlinkLoops()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        QVERIFY(QDir(folder.path()).mkdir("nested"));
        QVERIFY(QFile::link(folder.path(), folder.filePath("nested/back")));
        QFile file(folder.filePath("match.txt"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        Aero7CommonDialog dialog(Aero7CommonDialog::Mode::OpenFile, "search-loop");
        dialog.setInitialDirectory(folder.path());
        QVERIFY(QMetaObject::invokeMethod(&dialog, "runSearch", Q_ARG(QString, "match")));
        auto *search = dialog.findChild<QObject *>("dialogSearch");
        QVERIFY(search);
        QTRY_VERIFY(!search->property("busy").toBool());
        QCOMPARE(dialog.findChild<QListView *>()->model()->rowCount(), 1);
    }

    void searchRespectsFileTypesAndTypeChanges()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        for (const QString &name : {QString("match.png"), QString("match.txt")}) {
            QFile file(folder.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        QVERIFY(QDir(folder.path()).mkdir("match folder"));
        Aero7CommonDialog dialog(Aero7CommonDialog::Mode::OpenFile, "search-types");
        dialog.setInitialDirectory(folder.path());
        dialog.setNameFilters({"PNG (*.png)", "Text (*.txt)"});
        auto *view = dialog.findChild<QListView *>();
        QVERIFY(QMetaObject::invokeMethod(&dialog, "runSearch", Q_ARG(QString, "match")));
        QTRY_COMPARE(view->model()->rowCount(), 2);
        QStringList names;
        for (int i = 0; i < view->model()->rowCount(); ++i) names << view->model()->index(i, 0).data().toString();
        QVERIFY(names.contains("match.png"));
        QVERIFY(names.contains("match folder"));
        QVERIFY(!names.contains("match.txt"));
        dialog.selectNameFilter("Text (*.txt)");
        QTRY_VERIFY(view->model()->rowCount() == 2
            && (view->model()->index(0, 0).data().toString() == "match.txt"
                || view->model()->index(1, 0).data().toString() == "match.txt"));
    }

    void searchDoesNotSilentlyTruncateOrBlockDispatch()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        for (int i = 0; i < 1105; ++i) {
            QFile file(folder.filePath(QString("match-%1.txt").arg(i)));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        Aero7CommonDialog dialog(Aero7CommonDialog::Mode::OpenFile, "search-many");
        dialog.setInitialDirectory(folder.path());
        auto *view = dialog.findChild<QListView *>();
        QVERIFY(QMetaObject::invokeMethod(&dialog, "runSearch", Q_ARG(QString, "match")));
        // Dispatch must return before traversal/results; enumeration is off-thread.
        QCOMPARE(view->model()->rowCount(), 0);
        QTRY_COMPARE_WITH_TIMEOUT(view->model()->rowCount(), 1105, 10000);
    }

    void choosingFoldersDoesNotSearchFiles()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        QVERIFY(QDir(folder.path()).mkdir("match directory"));
        QFile file(folder.filePath("match.txt"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        Aero7CommonDialog dialog(Aero7CommonDialog::Mode::ChooseFolder, "search-folders");
        dialog.setInitialDirectory(folder.path());
        QVERIFY(QMetaObject::invokeMethod(&dialog, "runSearch", Q_ARG(QString, "match")));
        auto *view = dialog.findChild<QListView *>();
        QTRY_COMPARE(view->model()->rowCount(), 1);
        QCOMPARE(view->model()->index(0, 0).data().toString(), QString("match directory"));
    }

    void changingSearchAndNavigationDiscardsOldResults()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        for (const QString &name : {QString("old.txt"), QString("new.txt")}) {
            QFile file(folder.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        Aero7CommonDialog dialog(Aero7CommonDialog::Mode::OpenFile, "search-cancel");
        dialog.setInitialDirectory(folder.path());
        QVERIFY(QMetaObject::invokeMethod(&dialog, "runSearch", Q_ARG(QString, "old")));
        QVERIFY(QMetaObject::invokeMethod(&dialog, "runSearch", Q_ARG(QString, "new")));
        auto *view = dialog.findChild<QListView *>();
        QTRY_COMPARE(view->model()->rowCount(), 1);
        QCOMPARE(view->model()->index(0, 0).data().toString(), QString("new.txt"));
        QVERIFY(QMetaObject::invokeMethod(&dialog, "runSearch", Q_ARG(QString, "old")));
        dialog.setInitialDirectory(folder.path());
        QTest::qWait(300);
        QCOMPARE(view->model(), dialog.findChild<QFileSystemModel *>());
    }

    void longImageFiltersDoNotForceScreenWideDialogs()
    {
        Aero7CommonDialog dialog(Aero7CommonDialog::Mode::OpenFile, "long-filter");
        dialog.setNameFilters({"All Supported Files (" + QString("*.png *.jpg *.webp ").repeated(25) + ")"});
        dialog.show();
        QCoreApplication::processEvents();
        QVERIFY2(dialog.width() <= 900, qPrintable(QString("Dialog forced to %1 pixels").arg(dialog.width())));
    }

    void nativeFilterApiNotifiesOnlyEffectiveChanges()
    {
        Aero7CommonDialog dialog(Aero7CommonDialog::Mode::SaveFile, "native-filter");
        QSignalSpy changed(&dialog, &Aero7CommonDialog::filterChanged);
        dialog.setNameFilters({"PNG (*.png)", "JPEG (*.jpg *.jpeg)"});
        QCOMPARE(changed.count(), 1);
        QCOMPARE(dialog.selectedNameFilter(), QString("PNG (*.png)"));
        dialog.selectNameFilter("JPEG (*.jpg *.jpeg)");
        QCOMPARE(changed.count(), 2);
        QCOMPARE(changed.last().first().toString(), dialog.selectedNameFilter());
        dialog.selectNameFilter("JPEG (*.jpg *.jpeg)");
        dialog.selectNameFilter("unavailable");
        dialog.setNameFilters({"PNG (*.png)", "JPEG (*.jpg *.jpeg)"});
        QCOMPARE(changed.count(), 2);
        dialog.setNameFilters({"PNG (*.png)"});
        QCOMPARE(changed.count(), 3);
        QCOMPARE(dialog.selectedNameFilter(), QString("PNG (*.png)"));
    }

    void customOptionsSurviveAcceptAndAreOwnedByDialog()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        QPointer<QLineEdit> options;
        {
            Aero7CommonDialog dialog(Aero7CommonDialog::Mode::SaveFile, "native-options");
            options = new QLineEdit("quality=90");
            dialog.setCustomWidget(options);
            QCOMPARE(options->parentWidget(), &dialog);
            QCOMPARE(dialog.layout()->itemAt(dialog.layout()->count() - 2)->widget(), options.data());
            dialog.setInitialDirectory(folder.path());
            dialog.setSuggestedFileName("native.png");
            QVERIFY(QMetaObject::invokeMethod(&dialog, "accept", Qt::DirectConnection));
            QCOMPARE(dialog.result(), int(QDialog::Accepted));
            QVERIFY(options);
            QCOMPARE(options->text(), QString("quality=90"));
        }
        QVERIFY(options.isNull());
    }

    void customOptionsReplacementAndCancellation()
    {
        QPointer<QWidget> replacement;
        {
            Aero7CommonDialog dialog(Aero7CommonDialog::Mode::SaveFile, "native-replacement");
            QPointer<QWidget> original = new QWidget;
            dialog.setCustomWidget(original);
            dialog.setCustomWidget(original);
            QVERIFY(original);
            replacement = new QWidget;
            dialog.setCustomWidget(replacement);
            QVERIFY(original.isNull());
            dialog.reject();
            QVERIFY(replacement);
            QVERIFY(dialog.selectedFiles().isEmpty());
            dialog.setCustomWidget(nullptr);
            QVERIFY(replacement.isNull());
            // Do not adopt the dialog or one of its ancestors.
            dialog.setCustomWidget(&dialog);
            QCOMPARE(dialog.parentWidget(), nullptr);
        }
    }

    void imageFiltersKeepDirectoriesVisible()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        QVERIFY(QDir(folder.path()).mkdir("subfolder"));
        QFile text(folder.filePath("notes.txt"));
        QVERIFY(text.open(QIODevice::WriteOnly));
        text.close();
        Aero7CommonDialog dialog(Aero7CommonDialog::Mode::OpenFile, "visible-folders");
        dialog.setNameFilters({"JPEG (*.jpg *.jpeg)"});
        dialog.setInitialDirectory(folder.path());
        auto model = dialog.findChild<QFileSystemModel *>();
        QVERIFY(model);
        const QModelIndex root = model->index(folder.path());
        QTRY_COMPARE_WITH_TIMEOUT(model->rowCount(root), 1, 2000);
        QCOMPARE(model->fileName(model->index(0, 0, root)), QString("subfolder"));
    }

    void selectedFormatDeterminesMissingSuffix()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        Aero7CommonDialog dialog(Aero7CommonDialog::Mode::SaveFile, "suffix");
        dialog.setInitialDirectory(folder.path());
        dialog.setDefaultSuffix("png");
        dialog.setNameFilters({"PNG (*.png)", "JPEG (*.jpg *.jpeg)"});
        dialog.findChild<QComboBox *>("fileType")->setCurrentIndex(1);
        dialog.setSuggestedFileName("drawing");
        QVERIFY(QMetaObject::invokeMethod(&dialog, "accept", Qt::DirectConnection));
        QCOMPARE(dialog.selectedFiles(), QStringList({folder.filePath("drawing.jpg")}));
    }

    void typedExistingPathCanBeOpened()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        QFile image(folder.filePath("a drawing.png"));
        QVERIFY(image.open(QIODevice::WriteOnly));
        image.close();
        Aero7CommonDialog dialog(Aero7CommonDialog::Mode::OpenFile, "typed");
        dialog.setInitialDirectory(folder.path());
        dialog.setSuggestedFileName("a drawing.png");
        QPushButton *open = nullptr;
        for (auto button : dialog.findChildren<QPushButton *>())
            if (button->text() == "Open") open = button;
        QVERIFY(open);
        QVERIFY(open->isEnabled());
        QVERIFY(QMetaObject::invokeMethod(&dialog, "accept", Qt::DirectConnection));
        QCOMPARE(dialog.selectedFiles(), QStringList({image.fileName()}));
    }

    void editingNameSupersedesOldSelection()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        for (const QString name : {QString("first.png"), QString("second.png")}) {
            QFile image(folder.filePath(name));
            QVERIFY(image.open(QIODevice::WriteOnly));
        }
        Aero7CommonDialog dialog(Aero7CommonDialog::Mode::OpenFile, "edited");
        dialog.setInitialDirectory(folder.path());
        auto model = dialog.findChild<QFileSystemModel *>();
        auto view = dialog.findChild<QListView *>();
        auto field = dialog.findChild<QLineEdit *>("fileName");
        QVERIFY(model && view && field);
        const QModelIndex first = model->index(folder.filePath("first.png"));
        QVERIFY(first.isValid());
        view->selectionModel()->select(first, QItemSelectionModel::ClearAndSelect);
        QCOMPARE(field->text(), QString("first.png"));
        field->selectAll();
        QTest::keyClicks(field, "second.png");
        QVERIFY(view->selectionModel()->selectedIndexes().isEmpty());
        QVERIFY(QMetaObject::invokeMethod(&dialog, "accept", Qt::DirectConnection));
        QCOMPARE(dialog.selectedFiles(), QStringList({folder.filePath("second.png")}));
    }

    void directoryNameNavigatesInsteadOfSaving()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        QVERIFY(QDir(folder.path()).mkdir("subfolder"));
        Aero7CommonDialog dialog(Aero7CommonDialog::Mode::SaveFile, "directory");
        dialog.setInitialDirectory(folder.path());
        dialog.setDefaultSuffix("png");
        dialog.setSuggestedFileName("subfolder");
        QVERIFY(QMetaObject::invokeMethod(&dialog, "accept", Qt::DirectConnection));
        QCOMPARE(dialog.result(), QDialog::Rejected);
        QVERIFY(dialog.selectedFiles().isEmpty());
        QCOMPARE(dialog.findChild<QFileSystemModel *>()->rootPath(), folder.filePath("subfolder"));
    }

    void explicitSuffixIsNotRewritten()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        Aero7CommonDialog dialog(Aero7CommonDialog::Mode::SaveFile, "explicit-suffix");
        dialog.setInitialDirectory(folder.path());
        dialog.setNameFilters({"JPEG (*.jpg *.jpeg)"});
        dialog.setSuggestedFileName("drawing.jpeg");
        QVERIFY(QMetaObject::invokeMethod(&dialog, "accept", Qt::DirectConnection));
        QCOMPARE(dialog.selectedFiles(), QStringList({folder.filePath("drawing.jpeg")}));
    }

    void newLibraryFolderUsesRealSaveLocation()
    {
        auto &libraries = Aero7Libraries::instance();
        const QString root = libraries.materializedPath("documents");
        const QString destination = libraries.library("documents").saveLocation;
        Aero7CommonDialog dialog(Aero7CommonDialog::Mode::SaveFile, "new-folder");
        dialog.setInitialDirectory(root);
        QPushButton *create = nullptr;
        for (auto button : dialog.findChildren<QPushButton *>())
            if (button->text() == "New folder") create = button;
        QVERIFY(create);
        create->click();
        QVERIFY(QFileInfo::exists(QDir(destination).filePath("New folder")));
        QVERIFY(QFileInfo(QDir(root).filePath("New folder")).isSymLink());
    }

    void nestedLibrarySaveKeepsSelectedFolder()
    {
        auto &libraries = Aero7Libraries::instance();
        const QString destination = libraries.library("documents").saveLocation;
        QVERIFY(QDir().mkpath(QDir(destination).filePath("nested")));
        QVERIFY(libraries.refresh());
        const QString nested = QDir(libraries.materializedPath("documents")).filePath("nested");
        Aero7CommonDialog dialog(Aero7CommonDialog::Mode::SaveFile, "nested");
        dialog.setInitialDirectory(nested);
        dialog.setSuggestedFileName("drawing.png");
        QVERIFY(QMetaObject::invokeMethod(&dialog, "accept", Qt::DirectConnection));
        QCOMPARE(dialog.selectedFiles(), QStringList({QDir(destination).filePath("nested/drawing.png")}));
    }

    void computerUsesExplorerStoragePolicy()
    {
        Aero7CommonDialog dialog(Aero7CommonDialog::Mode::OpenFile, "storage");
        auto navigation = dialog.findChild<QTreeWidget *>();
        QVERIFY(navigation);
        QTreeWidgetItem *computer = nullptr;
        for (int i = 0; i < navigation->topLevelItemCount(); ++i)
            if (navigation->topLevelItem(i)->text(0) == "Computer")
                computer = navigation->topLevelItem(i);
        QVERIFY(computer);
        QStringList expected;
        for (const auto &storage : QStorageInfo::mountedVolumes())
            if (Aero7ComputerView::isUserVisibleStorage(storage))
                expected.append(storage.rootPath());
        QStringList actual;
        for (int i = 0; i < computer->childCount(); ++i) {
            const auto item = computer->child(i);
            const QString path = item->data(0, Qt::UserRole + 10).toString();
            if (!item->data(0, Qt::UserRole + 12).toString().isEmpty()) {
                QVERIFY(path.isEmpty());
                QVERIFY(!item->text(0).isEmpty());
                continue;
            }
            actual.append(path);
            if (path == "/") QCOMPARE(item->text(0), QString("Local Disk (C:)"));
        }
        expected.sort();
        actual.sort();
        QCOMPARE(actual, expected);
    }

    void saveButtonTracksName()
    {
        Aero7CommonDialog dialog(Aero7CommonDialog::Mode::SaveFile, "typing");
        const auto field = dialog.findChild<QLineEdit *>("fileName");
        QVERIFY(field);
        QPushButton *save = nullptr;
        for (auto button : dialog.findChildren<QPushButton *>())
            if (button->text() == "Save") save = button;
        QVERIFY(save);
        QVERIFY(!save->isEnabled());
        field->setText("drawing.png");
        QVERIFY(save->isEnabled());
        field->clear();
        QVERIFY(!save->isEnabled());
        dialog.setSuggestedFileName("suggested.png");
        QVERIFY(save->isEnabled());
    }

    void emptyNameCannotBeAccepted()
    {
        Aero7CommonDialog dialog(Aero7CommonDialog::Mode::SaveFile, "empty");
        dialog.setDefaultSuffix("png");
        QVERIFY(QMetaObject::invokeMethod(&dialog, "accept", Qt::DirectConnection));
        QVERIFY(dialog.selectedFiles().isEmpty());
        QCOMPARE(dialog.result(), QDialog::Rejected);
    }

    void restoresFilterAfterCallerSuppliesList()
    {
        QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                           "Aero7", "CommonItemDialog");
        settings.setValue("Applications/filters/fileType", "JPEG (*.jpg)");
        settings.sync();
        Aero7CommonDialog dialog(Aero7CommonDialog::Mode::SaveFile, "filters");
        dialog.setNameFilters({"PNG (*.png)", "JPEG (*.jpg)"});
        auto filter = dialog.findChild<QComboBox *>("fileType");
        QVERIFY(filter);
        QCOMPARE(filter->currentText(), QString("JPEG (*.jpg)"));
        dialog.setNameFilters({"PNG (*.png)"});
        QCOMPARE(filter->currentText(), QString("PNG (*.png)"));
    }

    void filtersUseFinalPatternGroup()
    {
        Aero7CommonDialog dialog(Aero7CommonDialog::Mode::OpenFile, "patterns");
        dialog.setNameFilters({"Images (editable) (*.png *.jpg)", "Text (*.txt)"});
        auto model = dialog.findChild<QFileSystemModel *>();
        auto filter = dialog.findChild<QComboBox *>("fileType");
        QVERIFY(model);
        QVERIFY(filter);
        QCOMPARE(model->nameFilters(), QStringList({"*.png", "*.jpg"}));
        filter->setCurrentIndex(1);
        QCOMPARE(model->nameFilters(), QStringList({"*.txt"}));
    }

    void saveSelectionDoesNotWriteImage()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        Aero7CommonDialog dialog(Aero7CommonDialog::Mode::SaveFile, "save");
        dialog.setInitialDirectory(folder.path());
        dialog.setDefaultSuffix("png");
        dialog.setSuggestedFileName("drawing");
        QVERIFY(QMetaObject::invokeMethod(&dialog, "accept", Qt::DirectConnection));
        QCOMPARE(dialog.result(), QDialog::Accepted);
        QCOMPARE(dialog.selectedFiles(), QStringList({folder.filePath("drawing.png")}));
        QVERIFY(!QFile::exists(folder.filePath("drawing.png")));
    }

    void decliningOverwritePreservesExistingFile()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        QFile existing(folder.filePath("drawing.png"));
        QVERIFY(existing.open(QIODevice::WriteOnly));
        QCOMPARE(existing.write("keep this file"), qint64(14));
        existing.close();
        Aero7CommonDialog dialog(Aero7CommonDialog::Mode::SaveFile, "overwrite");
        dialog.setInitialDirectory(folder.path());
        dialog.setSuggestedFileName("drawing.png");
        bool prompted = false;
        QTimer::singleShot(0, &dialog, [&] {
            for (auto widget : QApplication::topLevelWidgets()) {
                if (auto message = qobject_cast<QMessageBox *>(widget)) {
                    prompted = true;
                    message->done(QMessageBox::No);
                }
            }
        });
        QVERIFY(QMetaObject::invokeMethod(&dialog, "accept", Qt::DirectConnection));
        QVERIFY(prompted);
        QCOMPARE(dialog.result(), QDialog::Rejected);
        QVERIFY(dialog.selectedFiles().isEmpty());
        QVERIFY(existing.open(QIODevice::ReadOnly));
        QCOMPARE(existing.readAll(), QByteArray("keep this file"));
    }
};

int main(int argc, char **argv)
{
    QTemporaryDir profile;
    if (!profile.isValid()) return 2;
    qputenv("XDG_CONFIG_HOME", (profile.path() + "/config").toUtf8());
    qputenv("XDG_DATA_HOME", (profile.path() + "/data").toUtf8());
    qputenv("XDG_CACHE_HOME", (profile.path() + "/cache").toUtf8());
    QApplication app(argc, argv);
    const QString folder = profile.path() + "/documents";
    QDir().mkpath(folder);
    QDir().mkpath(profile.path() + "/config/aero7");
    // Use one isolated library; never materialize the host's user directories.
    const QJsonObject library{{"id", "documents"}, {"name", "Documents"},
                              {"locations", QJsonArray{folder}}, {"saveLocation", folder}};
    QFile config(profile.path() + "/config/aero7/libraries.json");
    if (!config.open(QIODevice::WriteOnly)
        || config.write(QJsonDocument(QJsonObject{{"libraries", QJsonArray{library}}}).toJson()) < 0)
        return 2;
    config.close();
    CommonDialogTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "aero7commondialogtest.moc"
