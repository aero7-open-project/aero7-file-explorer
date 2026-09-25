/*
 * SPDX-FileCopyrightText: 2017 Elvis Angelaccio <elvis.angelaccio@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "dolphinmainwindow.h"
#include "dolphin_generalsettings.h"
#include "dolphinnewfilemenu.h"
#include "dolphintabpage.h"
#include "dolphintabwidget.h"
#include "dolphinviewcontainer.h"
#include "dolphinwindowheader.h"
#include "dolphinurlnavigator.h"
#include "aero7libraries.h"
#include <KFilePlacesModel>
#include "kitemviews/kfileitemmodel.h"
#include "kitemviews/kfileitemmodelrolesupdater.h"
#include "kitemviews/kitemlistcontainer.h"
#include "kitemviews/kitemlistcontroller.h"
#include "kitemviews/kitemlistheader.h"
#include "kitemviews/kitemlistselectionmanager.h"
#include "kitemviews/kitemlistwidget.h"
#include "settings/viewmodes/viewmodesettings.h"
#include "statusbar/dolphinstatusbar.h"
#include "panels/places/placespanel.h"
#include "testdir.h"
#include "views/dolphinitemlistview.h"
#include "views/viewproperties.h"
#include "views/zoomlevelinfo.h"

#include <KActionCollection>
#include <KConfig>
#include <KConfigGui>
#include <KFileItem>
#include <KUrlComboBox>
#include <KSqueezedTextLabel>

#include <QAccessible>
#include <QAbstractButton>
#include <QApplication>
#include <QDomDocument>
#include <QDockWidget>
#include <QFileSystemWatcher>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QKeySequence>
#include <QScopedPointer>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QStandardItemModel>
#include <QStackedWidget>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTest>
#include <QToolBar>
#include <QToolButton>

#include <set>
#include <unordered_set>

class DolphinMainWindowTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void testSyncDesktopAndPhoneUi();
    void testAero7ActionLayoutResource();
    void testClosingTabsWithSearchBoxVisible();
    void testActiveViewAfterClosingSplitView_data();
    void testActiveViewAfterClosingSplitView();
    void testUpdateWindowTitleAfterClosingSplitView();
    void testUpdateWindowTitleAfterChangingSplitView();
    void testOpenInNewTabTitle();
    void testNewFileMenuEnabled_data();
    void testNewFileMenuEnabled();
    void testCreateFileAction();
    void testCreateFileActionRequiresWritePermission();
    void testWindowTitle_data();
    void testWindowTitle();
    void testFocusLocationBar();
    void testAero7ExplorerChromeContract();
    void testAero7ClipboardEventBeforeOpeningFolder();
    void testAero7LibraryPlaceActivation();
    void testAero7SystemDriveActivation();
    void testAero7BreadcrumbSurvivesPlacesRefresh();
    void testAero7NoInventedCdPlace();
    void testAero7PlacesMenuRejectsInvalidOrForeignIndex();
    void testAero7PlacesMenuUsesNativeStorageCapabilities();
    void testAero7PlacesMenuDoesNotTearDownBookmarks();
    void testAero7StorageRecoveryKeepsOtherObservers();
    void testAero7StorageRecoveryUsesCompletedDrive();
    void testAero7FailedStorageRequestIsConsumed_data();
    void testAero7FailedStorageRequestIsConsumed();
    void testAero7DestroyedStorageRequestIsDiscarded();
    void testAero7ComputerNavigatorUsesResolvablePlace();
    void testAero7SidebarHasNoMissingLibraryGap();
    void testAero7SidebarDefaultLabelsFit();
    void testAero7ComputerBackForwardHistory();
    void testAero7ComputerLaunchSelectsItsPlace();
    void testAero7ComputerTabRestoresSurface();
    void testAero7ComputerTabsRemainMouseAccessible();
    void testAero7ComputerSplitPaneRemainsVisible();
    void testAero7SplitBreadcrumbFollowsActiveTab();
    void testAero7NormalDetailsModeIsIdempotent();
    void testAero7FolderDetailsSurviveTabAndSplitChanges();
    void testAero7WindowFitsAvailableScreen();
    void testAero7ComputerDetailsNotClipped_data();
    void testAero7ComputerDetailsNotClipped();
    void testFocusPlacesPanel();
    void testPlacesPanelWidthResistance();
    void testGoActions();
    void testOpenFiles();
    void testAccessibilityTree();
    void testAutoSaveSession();
    void testInlineRename();
    void testThumbnailAfterRename();
    void testViewModeAfterDynamicView();
    void testActivationAndTabTitleAfterRenameOpeningFolder();
    void testActiveViewAfterTabSwitchWithSplitView();
    void cleanupTestCase();

private:
    QScopedPointer<DolphinMainWindow> m_mainWindow;
};

void DolphinMainWindowTest::initTestCase()
{
    // main() provides a fresh process-local XDG profile. Qt's shared .qttest
    // profile can retain libraries from an earlier run and invalidate fixtures.
    // Use fullWidth statusbar during testing, to test out most of the features.
    GeneralSettings *settings = GeneralSettings::self();
    settings->setShowStatusBar(GeneralSettings::EnumShowStatusBar::FullWidth);
    settings->setShowZoomSlider(true);
    settings->save();

    // to save us from kxmlgui / KLocalized warning
    KLocalizedString::setApplicationDomain({"dolphin"});
}

void DolphinMainWindowTest::init()
{
    m_mainWindow.reset(new DolphinMainWindow());
}

void DolphinMainWindowTest::testAero7StorageRecoveryKeepsOtherObservers()
{
    QTemporaryDir drive;
    QVERIFY(drive.isValid());
    m_mainWindow->openDirectories({QUrl::fromLocalFile(drive.path())}, false);
    auto *panel = m_mainWindow->m_placesPanel;
    QSignalSpy completions(panel, &PlacesPanel::storageTearDownSuccessful);
    m_mainWindow->slotStorageTearDownExternallyRequested(drive.path());
    Q_EMIT panel->storageTearDownSuccessful(drive.path());
    QCOMPARE(m_mainWindow->activeViewContainer()->url(), QUrl::fromLocalFile(QDir::homePath()));
    Q_EMIT panel->storageTearDownSuccessful(drive.path());
    QCOMPARE(completions.count(), 2);
}

void DolphinMainWindowTest::testAero7StorageRecoveryUsesCompletedDrive()
{
    QTemporaryDir firstDrive;
    QTemporaryDir secondDrive;
    QVERIFY(firstDrive.isValid());
    QVERIFY(secondDrive.isValid());
    m_mainWindow->openDirectories({QUrl::fromLocalFile(secondDrive.path())}, false);
    // No host mounts: exercise only the window's completion dispatch.
    m_mainWindow->slotStorageTearDownExternallyRequested(firstDrive.path());
    m_mainWindow->slotStorageTearDownExternallyRequested(secondDrive.path());
    Q_EMIT m_mainWindow->m_placesPanel->storageTearDownSuccessful(secondDrive.path());
    QCOMPARE(m_mainWindow->activeViewContainer()->url(), QUrl::fromLocalFile(QDir::homePath()));
}

void DolphinMainWindowTest::testAero7FailedStorageRequestIsConsumed_data()
{
    QTest::addColumn<int>("failure");
    QTest::newRow("busy") << int(Solid::DeviceBusy);
    QTest::newRow("denied") << int(Solid::UnauthorizedOperation);
    QTest::newRow("cancelled") << int(Solid::UserCanceled);
}

void DolphinMainWindowTest::testAero7FailedStorageRequestIsConsumed()
{
    QFETCH(int, failure);
    QTemporaryDir firstDrive;
    QTemporaryDir secondDrive;
    QVERIFY(firstDrive.isValid());
    QVERIFY(secondDrive.isValid());
    const QUrl firstUrl = QUrl::fromLocalFile(firstDrive.path());
    m_mainWindow->openDirectories({firstUrl}, false);
    auto *panel = m_mainWindow->m_placesPanel;
    QSignalSpy completions(panel, &PlacesPanel::storageTearDownSuccessful);
    // Tokens stand in for two native access objects; no mount APIs are called.
    QObject firstAccess;
    QObject secondAccess;
    panel->m_tearDownPaths.insert(&firstAccess, firstDrive.path());
    panel->m_tearDownPaths.insert(&secondAccess, secondDrive.path());
    panel->completeTearDown(&firstAccess, Solid::ErrorType(failure));
    QVERIFY(!panel->m_tearDownPaths.contains(&firstAccess));
    QVERIFY(panel->m_tearDownPaths.contains(&secondAccess));
    QCOMPARE(completions.count(), 0);
    QCOMPARE(m_mainWindow->activeViewContainer()->url(), firstUrl);
    panel->completeTearDown(&secondAccess, Solid::NoError);
    QCOMPARE(completions.count(), 1);
    QCOMPARE(completions.at(0).at(0).toString(), secondDrive.path());
    QCOMPARE(m_mainWindow->activeViewContainer()->url(), firstUrl);
    panel->completeTearDown(&firstAccess, Solid::NoError);
    QCOMPARE(completions.count(), 1); // A stale completion has no request.
    panel->m_tearDownPaths.insert(&firstAccess, firstDrive.path());
    panel->completeTearDown(&firstAccess, Solid::NoError);
    QCOMPARE(completions.count(), 2);
    QCOMPARE(m_mainWindow->activeViewContainer()->url(), QUrl::fromLocalFile(QDir::homePath()));
    QVERIFY(panel->m_tearDownPaths.isEmpty());
}

void DolphinMainWindowTest::testAero7DestroyedStorageRequestIsDiscarded()
{
    auto *panel = m_mainWindow->m_placesPanel;
    QSignalSpy completions(panel, &PlacesPanel::storageTearDownSuccessful);
    QObject oldAccess;
    QObject replacementAccess;
    panel->m_tearDownPaths.insert(&oldAccess, QStringLiteral("/run/media/test/old"));
    panel->m_tearDownPaths.insert(&replacementAccess, QStringLiteral("/run/media/test/new"));
    panel->slotStorageAccessDestroyed(&oldAccess);
    QVERIFY(!panel->m_tearDownPaths.contains(&oldAccess));
    QVERIFY(panel->m_tearDownPaths.contains(&replacementAccess));
    panel->completeTearDown(&oldAccess, Solid::NoError);
    QCOMPARE(completions.count(), 0);
    panel->completeTearDown(&replacementAccess, Solid::NoError);
    QCOMPARE(completions.count(), 1);
    QCOMPARE(completions.at(0).at(0).toString(), QStringLiteral("/run/media/test/new"));
}

void DolphinMainWindowTest::testAero7ClipboardEventBeforeOpeningFolder()
{
    // Wayland can deliver a clipboard event as soon as a newly shown window
    // receives focus, before openDirectories() has supplied its first view.
    QVERIFY(!m_mainWindow->activeViewContainer());
    auto *paste = m_mainWindow->actionCollection()->action(KStandardAction::name(KStandardAction::Paste));
    QVERIFY(paste);
    m_mainWindow->updatePasteAction();
    QVERIFY(!paste->isEnabled());
    m_mainWindow->openDirectories({QUrl::fromLocalFile(QDir::homePath())}, false);
    QVERIFY(m_mainWindow->activeViewContainer());
    m_mainWindow->updatePasteAction();
    QCOMPARE(paste->isEnabled(), m_mainWindow->activeViewContainer()->view()->pasteInfo().first);
}

/**
 * It is too easy to forget that most changes in dolphinui.rc should be mirrored in dolphinuiforphones.rc. This test makes sure that these two files stay
 * mostly identical. Differences between those files need to be explicitly added as exceptions to this test. So if you land here after changing either
 * dolphinui.rc or dolphinuiforphones.rc, then resolve this test failure either by making the exact same change to the other ui.rc file, or by adding the
 * changed object to the `exceptions` variable below.
 */
void DolphinMainWindowTest::testAero7ActionLayoutResource()
{
    QCOMPARE(m_mainWindow->xmlFile(), QStringLiteral(":/kxmlgui5/dolphin/dolphinui.rc"));
    QVERIFY(!m_mainWindow->domDocument().documentElement().isNull());
    QCOMPARE(m_mainWindow->domDocument().documentElement().tagName(), QStringLiteral("gui"));
}

void DolphinMainWindowTest::testSyncDesktopAndPhoneUi()
{
    std::unordered_set<QString> exceptions{{QStringLiteral("version"), QStringLiteral("ToolBar")}};

    QDomDocument desktopUi;
    QFile desktopUiXmlFile(":/kxmlgui5/dolphin/dolphinui.rc");
    QVERIFY2(desktopUiXmlFile.open(QIODevice::ReadOnly), qPrintable(QStringLiteral("couldn't open %1").arg(desktopUiXmlFile.fileName())));
    desktopUi.setContent(&desktopUiXmlFile);
    desktopUiXmlFile.close();

    QDomDocument phoneUi;
    QFile phoneUiXmlFile(":/kxmlgui5/dolphin/dolphinuiforphones.rc");
    QVERIFY2(phoneUiXmlFile.open(QIODevice::ReadOnly), qPrintable(QStringLiteral("couldn't open %1").arg(phoneUiXmlFile.fileName())));
    phoneUi.setContent(&phoneUiXmlFile);
    phoneUiXmlFile.close();

    QDomElement desktopUiElement = desktopUi.documentElement();
    QDomElement phoneUiElement = phoneUi.documentElement();

    auto nextUiElement = [&exceptions](QDomElement uiElement) -> QDomElement {
        QDomNode nextUiNode{uiElement};
        do {
            // If the current node is an exception, we skip its children as well.
            if (exceptions.count(nextUiNode.nodeName()) == 0) {
                auto firstChild{nextUiNode.firstChild()};
                if (!firstChild.isNull()) {
                    nextUiNode = firstChild;
                    continue;
                }
            }
            auto nextSibling{nextUiNode.nextSibling()};
            if (!nextSibling.isNull()) {
                nextUiNode = nextSibling;
                continue;
            }
            auto parent{nextUiNode.parentNode()};
            while (true) {
                if (parent.isNull()) {
                    return QDomElement();
                }
                auto nextParentSibling{parent.nextSibling()};
                if (!nextParentSibling.isNull()) {
                    nextUiNode = nextParentSibling;
                    break;
                }
                parent = parent.parentNode();
            }
        } while (
            !nextUiNode.isNull()
            && (nextUiNode.toElement().isNull() || exceptions.count(nextUiNode.nodeName()))); // We loop until we either give up finding an element or find one.
        if (nextUiNode.isNull()) {
            return QDomElement();
        }
        return nextUiNode.toElement();
    };

    int totalComparisonsCount{0};
    do {
        QVERIFY2(desktopUiElement.tagName() == phoneUiElement.tagName(),
                 qPrintable(QStringLiteral("Node mismatch: dolphinui.rc/%1::%2 and dolphinuiforphones.rc/%3::%4")
                                .arg(desktopUiElement.parentNode().toElement().tagName(),
                                     desktopUiElement.tagName(),
                                     phoneUiElement.parentNode().toElement().tagName(),
                                     phoneUiElement.tagName())));
        QCOMPARE(desktopUiElement.text(), phoneUiElement.text());
        const auto desktopUiElementAttributes = desktopUiElement.attributes();
        const auto phoneUiElementAttributes = phoneUiElement.attributes();
        for (int i = 0; i < desktopUiElementAttributes.count(); i++) {
            QVERIFY2(phoneUiElementAttributes.count() >= i,
                     qPrintable(QStringLiteral("Attribute mismatch: dolphinui.rc/%1::%2 has more attributes than dolphinuiforphones.rc/%3::%4")
                                    .arg(desktopUiElement.parentNode().toElement().tagName(),
                                         desktopUiElement.tagName(),
                                         phoneUiElement.parentNode().toElement().tagName(),
                                         phoneUiElement.tagName())));
            if (exceptions.count(desktopUiElementAttributes.item(i).nodeName())) {
                continue;
            }
            QCOMPARE(desktopUiElementAttributes.item(i).nodeName(), phoneUiElementAttributes.item(i).nodeName());
            QCOMPARE(desktopUiElementAttributes.item(i).nodeValue(), phoneUiElementAttributes.item(i).nodeValue());
            totalComparisonsCount++;
        }
        QVERIFY2(desktopUiElementAttributes.count() == phoneUiElementAttributes.count(),
                 qPrintable(QStringLiteral("Attribute mismatch: dolphinui.rc/%1::%2 has fewer attributes than dolphinuiforphones.rc/%3::%4. %5 < %6")
                                .arg(desktopUiElement.parentNode().toElement().tagName(),
                                     desktopUiElement.tagName(),
                                     phoneUiElement.parentNode().toElement().tagName(),
                                     phoneUiElement.tagName())
                                .arg(phoneUiElementAttributes.count(), desktopUiElementAttributes.count())));

        desktopUiElement = nextUiElement(desktopUiElement);
        phoneUiElement = nextUiElement(phoneUiElement);
        totalComparisonsCount++;
    } while (!desktopUiElement.isNull() || !phoneUiElement.isNull());
    QVERIFY2(totalComparisonsCount > 200, qPrintable(QStringLiteral("There were only %1 comparisons. Did the test run correctly?").arg(totalComparisonsCount)));
}

// See https://bugs.kde.org/show_bug.cgi?id=379135
void DolphinMainWindowTest::testClosingTabsWithSearchBoxVisible()
{
    m_mainWindow->openDirectories({QUrl::fromLocalFile(QDir::homePath())}, false);
    m_mainWindow->show();
    // Without this call the searchbox doesn't get FocusIn events.
    QVERIFY(QTest::qWaitForWindowExposed(m_mainWindow.data()));
    QVERIFY(m_mainWindow->isVisible());

    auto tabWidget = m_mainWindow->findChild<DolphinTabWidget *>("tabWidget");
    QVERIFY(tabWidget);

    // Show search box on first tab.
    tabWidget->currentTabPage()->activeViewContainer()->setSearchBarVisible(true);

    tabWidget->openNewActivatedTab(QUrl::fromLocalFile(QDir::homePath()));
    QCOMPARE(tabWidget->count(), 2);

    // Triggers the crash in bug #379135.
    tabWidget->closeTab();
    QCOMPARE(tabWidget->count(), 1);
}

void DolphinMainWindowTest::testActiveViewAfterClosingSplitView_data()
{
    QTest::addColumn<bool>("closeLeftView");

    QTest::newRow("close left view") << true;
    QTest::newRow("close right view") << false;
}

void DolphinMainWindowTest::testActiveViewAfterClosingSplitView()
{
    m_mainWindow->openDirectories({QUrl::fromLocalFile(QDir::homePath())}, false);
    m_mainWindow->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_mainWindow.data()));
    QVERIFY(m_mainWindow->isVisible());

    auto tabWidget = m_mainWindow->findChild<DolphinTabWidget *>("tabWidget");
    QVERIFY(tabWidget);
    QVERIFY(tabWidget->currentTabPage()->primaryViewContainer());
    QVERIFY(!tabWidget->currentTabPage()->secondaryViewContainer());

    // Open split view.
    m_mainWindow->actionCollection()->action(QStringLiteral("split_view"))->trigger();
    QVERIFY(tabWidget->currentTabPage()->splitViewEnabled());
    QVERIFY(tabWidget->currentTabPage()->secondaryViewContainer());

    // Make sure the right view is the active one.
    auto leftViewContainer = tabWidget->currentTabPage()->primaryViewContainer();
    auto rightViewContainer = tabWidget->currentTabPage()->secondaryViewContainer();
    QVERIFY(!leftViewContainer->isActive());
    QVERIFY(rightViewContainer->isActive());

    QFETCH(bool, closeLeftView);
    if (closeLeftView) {
        // Activate left view.
        leftViewContainer->setActive(true);
        QVERIFY(leftViewContainer->isActive());
        QVERIFY(!rightViewContainer->isActive());

        // Close left view. The secondary view (which was on the right) will become the primary one and must be active.
        m_mainWindow->actionCollection()->action(QStringLiteral("split_view"))->trigger();
        QVERIFY(!leftViewContainer->isActive());
        QVERIFY(rightViewContainer->isActive());
        QCOMPARE(rightViewContainer, tabWidget->currentTabPage()->activeViewContainer());
    } else {
        // Close right view. The left view will become active.
        m_mainWindow->actionCollection()->action(QStringLiteral("split_view"))->trigger();
        QVERIFY(leftViewContainer->isActive());
        QVERIFY(!rightViewContainer->isActive());
        QCOMPARE(leftViewContainer, tabWidget->currentTabPage()->activeViewContainer());
    }
}

// Test case for bug #385111
void DolphinMainWindowTest::testUpdateWindowTitleAfterClosingSplitView()
{
    m_mainWindow->openDirectories({QUrl::fromLocalFile(QDir::homePath())}, false);
    m_mainWindow->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_mainWindow.data()));
    QVERIFY(m_mainWindow->isVisible());

    auto tabWidget = m_mainWindow->findChild<DolphinTabWidget *>("tabWidget");
    QVERIFY(tabWidget);
    QVERIFY(tabWidget->currentTabPage()->primaryViewContainer());
    QVERIFY(!tabWidget->currentTabPage()->secondaryViewContainer());

    // Open split view.
    m_mainWindow->actionCollection()->action(QStringLiteral("split_view"))->trigger();
    QVERIFY(tabWidget->currentTabPage()->splitViewEnabled());
    QVERIFY(tabWidget->currentTabPage()->secondaryViewContainer());

    // Make sure the right view is the active one.
    auto leftViewContainer = tabWidget->currentTabPage()->primaryViewContainer();
    auto rightViewContainer = tabWidget->currentTabPage()->secondaryViewContainer();
    QVERIFY(!leftViewContainer->isActive());
    QVERIFY(rightViewContainer->isActive());

    // Activate left view.
    leftViewContainer->setActive(true);
    QVERIFY(leftViewContainer->isActive());
    QVERIFY(!rightViewContainer->isActive());

    // Close split view. The secondary view (which was on the right) will become the primary one and must be active.
    m_mainWindow->actionCollection()->action(QStringLiteral("split_view"))->trigger();
    QVERIFY(!leftViewContainer->isActive());
    QVERIFY(rightViewContainer->isActive());
    QCOMPARE(rightViewContainer, tabWidget->currentTabPage()->activeViewContainer());

    // Change URL and make sure we emit the currentUrlChanged signal (which triggers the window title update).
    QSignalSpy currentUrlChangedSpy(tabWidget, &DolphinTabWidget::currentUrlChanged);
    tabWidget->currentTabPage()->activeViewContainer()->setUrl(QUrl::fromLocalFile(QDir::rootPath()));
    QCOMPARE(currentUrlChangedSpy.count(), 1);
}

// Test case for bug #402641
void DolphinMainWindowTest::testUpdateWindowTitleAfterChangingSplitView()
{
    m_mainWindow->openDirectories({QUrl::fromLocalFile(QDir::homePath())}, false);
    m_mainWindow->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_mainWindow.data()));
    QVERIFY(m_mainWindow->isVisible());

    auto tabWidget = m_mainWindow->findChild<DolphinTabWidget *>("tabWidget");
    QVERIFY(tabWidget);

    // Open split view.
    m_mainWindow->actionCollection()->action(QStringLiteral("split_view"))->trigger();
    QVERIFY(tabWidget->currentTabPage()->splitViewEnabled());

    auto leftViewContainer = tabWidget->currentTabPage()->primaryViewContainer();
    auto rightViewContainer = tabWidget->currentTabPage()->secondaryViewContainer();

    // The taskbar and window switcher must keep the Aero7 product identity
    // when either side of a split view changes location.
    const auto oldTitle = m_mainWindow->windowTitle();
    QCOMPARE(oldTitle, QStringLiteral("File Explorer"));

    // Changing the URL must not replace the product name with a raw location.
    rightViewContainer->setUrl(QUrl::fromLocalFile(QDir::rootPath()));
    QCOMPARE(m_mainWindow->windowTitle(), oldTitle);

    // Activating the other view also preserves the stable taskbar caption.
    leftViewContainer->setActive(true);
    QCOMPARE(m_mainWindow->windowTitle(), oldTitle);
}

// Test case for bug #397910
void DolphinMainWindowTest::testOpenInNewTabTitle()
{
    const QUrl homePathUrl{QUrl::fromLocalFile(QDir::homePath())};
    m_mainWindow->openDirectories({homePathUrl}, false);
    m_mainWindow->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_mainWindow.data()));
    QVERIFY(m_mainWindow->isVisible());

    auto tabWidget = m_mainWindow->findChild<DolphinTabWidget *>("tabWidget");
    QVERIFY(tabWidget);

    const QUrl tempPathUrl{QUrl::fromLocalFile(QDir::tempPath())};
    tabWidget->openNewTab(tempPathUrl);
    QCOMPARE(tabWidget->count(), 2);
    QVERIFY(tabWidget->tabText(0) != tabWidget->tabText(1));

    QVERIFY2(!tabWidget->tabIcon(0).isNull() && !tabWidget->tabIcon(1).isNull(), "Tabs are supposed to have icons.");
    QCOMPARE(KIO::iconNameForUrl(homePathUrl), tabWidget->tabIcon(0).name());
    QCOMPARE(KIO::iconNameForUrl(tempPathUrl), tabWidget->tabIcon(1).name());
}

void DolphinMainWindowTest::testNewFileMenuEnabled_data()
{
    QTest::addColumn<QUrl>("activeViewUrl");
    QTest::addColumn<bool>("expectedEnabled");

    QTest::newRow("home") << QUrl::fromLocalFile(QDir::homePath()) << true;
    QTest::newRow("root") << QUrl::fromLocalFile(QDir::rootPath()) << false;
    QTest::newRow("trash") << QUrl::fromUserInput(QStringLiteral("trash:/")) << false;
}

void DolphinMainWindowTest::testNewFileMenuEnabled()
{
    QFETCH(QUrl, activeViewUrl);
    m_mainWindow->openDirectories({activeViewUrl}, false);
    m_mainWindow->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_mainWindow.data()));
    QVERIFY(m_mainWindow->isVisible());

    auto newFileMenu = m_mainWindow->findChild<DolphinNewFileMenu *>("new_menu");
    QVERIFY(newFileMenu);

    QFETCH(bool, expectedEnabled);
    QTRY_COMPARE(newFileMenu->isEnabled(), expectedEnabled);
}

void DolphinMainWindowTest::testCreateFileAction()
{
    QScopedPointer<TestDir> testDir{new TestDir()};
    QString testDirUrl(QDir::cleanPath(testDir->url().toString()));
    m_mainWindow->openDirectories({testDirUrl}, false);
    m_mainWindow->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_mainWindow.data()));
    QVERIFY(m_mainWindow->isVisible());

    QCOMPARE(m_mainWindow->m_activeViewContainer->view()->items().count(), 0);

    auto createFileAction = m_mainWindow->actionCollection()->action(QStringLiteral("create_file"));
    QTRY_COMPARE(createFileAction->isEnabled(), true);

    createFileAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_N));

    QSignalSpy createFileActionSpy(createFileAction, &QAction::triggered);

    QTest::keyClick(QApplication::activeWindow(), Qt::Key_N, Qt::ControlModifier | Qt::AltModifier);

    QTRY_COMPARE(createFileActionSpy.count(), 1);

    QTRY_VERIFY(QApplication::activeModalWidget() != nullptr);

    auto newFileDialog = QApplication::activeModalWidget()->focusWidget();
    QTest::keyClick(newFileDialog, Qt::Key_X);
    QTest::keyClick(newFileDialog, Qt::Key_Y);
    QTest::keyClick(newFileDialog, Qt::Key_Z);
    QTest::keyClick(newFileDialog, Qt::Key_Enter);

    QTRY_COMPARE(m_mainWindow->m_activeViewContainer->view()->items().count(), 1);

    QFile file(testDir->url().toLocalFile() + "/xyz.txt");
    QVERIFY(file.exists());
    QCOMPARE(file.size(), 0);
}

void DolphinMainWindowTest::testCreateFileActionRequiresWritePermission()
{
    QScopedPointer<TestDir> testDir{new TestDir()};
    QString testDirUrl(QDir::cleanPath(testDir->url().toString()));
    auto testDirAsFile = QFile(testDir->url().toLocalFile());

    // make test dir read only
    QVERIFY(testDirAsFile.setPermissions(QFileDevice::ReadOwner));

    m_mainWindow->openDirectories({testDirUrl}, false);
    m_mainWindow->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_mainWindow.data()));
    QVERIFY(m_mainWindow->isVisible());

    QTRY_VERIFY_WITH_TIMEOUT(QApplication::activeWindow() != nullptr, 100);

    QCOMPARE(m_mainWindow->m_activeViewContainer->view()->items().count(), 0);

    auto createFileAction = m_mainWindow->actionCollection()->action(QStringLiteral("create_file"));
    QTRY_COMPARE(createFileAction->isEnabled(), false);

    createFileAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_N));
    QTest::keyClick(QApplication::activeWindow(), Qt::Key_N, Qt::ControlModifier | Qt::AltModifier);

    QTRY_COMPARE(QApplication::activeModalWidget(), nullptr);

    QTRY_COMPARE(m_mainWindow->m_activeViewContainer->view()->items().count(), 0);

    QTRY_COMPARE(createFileAction->isEnabled(), false);

    QVERIFY(m_mainWindow->isVisible());
}

void DolphinMainWindowTest::testWindowTitle_data()
{
    QTest::addColumn<QUrl>("activeViewUrl");
    QTest::addColumn<QString>("expectedWindowTitle");

    // TODO: this test should enforce the english locale.
    QTest::newRow("home") << QUrl::fromLocalFile(QDir::homePath()) << QStringLiteral("File Explorer");
    QTest::newRow("home with trailing slash") << QUrl::fromLocalFile(QStringLiteral("%1/").arg(QDir::homePath())) << QStringLiteral("File Explorer");
    QTest::newRow("trash") << QUrl::fromUserInput(QStringLiteral("trash:/")) << QStringLiteral("File Explorer");
}

void DolphinMainWindowTest::testWindowTitle()
{
    QFETCH(QUrl, activeViewUrl);
    m_mainWindow->openDirectories({activeViewUrl}, false);
    m_mainWindow->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_mainWindow.data()));
    QVERIFY(m_mainWindow->isVisible());

    QFETCH(QString, expectedWindowTitle);
    QCOMPARE(m_mainWindow->windowTitle(), expectedWindowTitle);
}

void DolphinMainWindowTest::testFocusLocationBar()
{
    const QUrl homePathUrl{QUrl::fromLocalFile(QDir::homePath())};
    m_mainWindow->openDirectories({homePathUrl}, false);
    m_mainWindow->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_mainWindow.data()));
    QVERIFY(m_mainWindow->isVisible());
    QTRY_VERIFY_WITH_TIMEOUT(QApplication::activeWindow() != nullptr, 100);

    QTest::keyClick(m_mainWindow.data(), Qt::Key_L, Qt::ControlModifier);
    QTRY_VERIFY(m_mainWindow->activeViewContainer()->urlNavigator()->isUrlEditable());
    QVERIFY(m_mainWindow->activeViewContainer()->urlNavigator()->editor()->lineEdit()->hasFocus());

    QAction *editableLocationAction = m_mainWindow->actionCollection()->action(QStringLiteral("editable_location"));
    editableLocationAction->trigger();
    QVERIFY(!m_mainWindow->activeViewContainer()->urlNavigator()->isUrlEditable());
    QVERIFY(m_mainWindow->activeViewContainer()->view()->hasFocus());

    DolphinUrlNavigator *navigator = m_mainWindow->activeViewContainer()->urlNavigator();
    QTest::mouseClick(navigator, Qt::LeftButton, Qt::NoModifier,
                      QPoint(navigator->width() - 4, navigator->height() / 2));
    QTRY_VERIFY(navigator->isUrlEditable());
    QLineEdit *pathEditor = navigator->editor()->lineEdit();
    QVERIFY(pathEditor->hasFocus());
    pathEditor->selectAll();
    const QString downloadsPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    QTest::keyClicks(pathEditor, downloadsPath);
    QCOMPARE(pathEditor->text(), downloadsPath);
    QTest::keyClick(pathEditor, Qt::Key_Return);
    QTRY_COMPARE(navigator->locationUrl(), QUrl::fromLocalFile(downloadsPath));
    QTRY_COMPARE(m_mainWindow->activeViewContainer()->urlNavigatorInternalWithHistory()->locationUrl(), QUrl::fromLocalFile(downloadsPath));
    QTRY_COMPARE(m_mainWindow->activeViewContainer()->url(), QUrl::fromLocalFile(downloadsPath));
}

void DolphinMainWindowTest::testAero7ExplorerChromeContract()
{
    m_mainWindow->openDirectories({QUrl::fromLocalFile(QDir::homePath())}, false);
    m_mainWindow->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_mainWindow.data()));

    QCOMPARE(m_mainWindow->m_winHeader->height(), 65);
    QToolBar *chrome = m_mainWindow->findChild<QToolBar *>(
        QStringLiteral("aero7ExplorerChrome"));
    QVERIFY(chrome);
    QCOMPARE(chrome->height(), 65);

    QToolButton *history = m_mainWindow->findChild<QToolButton *>(
        QStringLiteral("aero7AddressHistoryButton"));
    QToolButton *refresh = m_mainWindow->findChild<QToolButton *>(
        QStringLiteral("aero7RefreshButton"));
    QLabel *searchIcon = m_mainWindow->findChild<QLabel *>(
        QStringLiteral("aero7SearchIcon"));
    QVERIFY(history && history->isVisible());
    QVERIFY(refresh && refresh->isVisible());
    QVERIFY(searchIcon && !searchIcon->pixmap().isNull());
    QToolButton *organizeButton = nullptr;
    QToolButton *viewsButton = nullptr;
    QToolButton *newFolderButton = nullptr;
    for (QToolButton *button : m_mainWindow->m_winHeader->findChildren<QToolButton *>()) {
        if (button->text() == QLatin1String("Organize")) organizeButton = button;
        if (button->text() == QLatin1String("Views")) viewsButton = button;
        if (button->text() == QLatin1String("New folder")) newFolderButton = button;
    }
    QVERIFY(organizeButton && organizeButton->isVisible());
    QVERIFY(viewsButton && viewsButton->isVisible());
    QVERIFY(newFolderButton && newFolderButton->isVisible());
    QVERIFY(organizeButton->x() < viewsButton->x());
    QVERIFY(viewsButton->x() < newFolderButton->x());
    QCOMPARE(history->size(), QSize(20, 21));
    QCOMPARE(refresh->size(), QSize(24, 21));

    DolphinView *view = m_mainWindow->activeViewContainer()->view();
    KItemListView *itemListView = view->m_container->controller()->view();
    QTRY_VERIFY(itemListView->isHeaderVisible());
    KItemListHeader *header = itemListView->header();
    const auto verifyNormalFolderHeader = [header, itemListView]() {
        QCOMPARE(itemListView->visibleRoles(), (QList<QByteArray>{"text", "size", "type", "modificationtime"}));
        QCOMPARE(header->leftPadding(), 10.0);
        QCOMPARE(header->columnWidth("text"), 284.0);
        QCOMPARE(header->columnWidth("modificationtime"), 120.0);
        QCOMPARE(header->columnWidth("type"), 120.0);
        QCOMPARE(header->columnWidth("size"), 80.0);
    };
    // Assert startup geometry before invoking any view-mode action; this
    // catches constructor defaults and late asynchronous directory-layout
    // work that could otherwise override the Windows 7 column geometry.
    QTest::qWait(500);
    QDockWidget *placesDock = m_mainWindow->findChild<QDockWidget *>(
        QStringLiteral("placesDock"));
    QVERIFY(placesDock);
    QCOMPARE(placesDock->width(), m_mainWindow->m_placesPanel->sizeHint().width());
    QVERIFY(!header->automaticColumnResizing());
    verifyNormalFolderHeader();

    QSignalSpy refreshed(m_mainWindow.data(), &DolphinMainWindow::urlRefreshed);
    QTest::mouseClick(refresh, Qt::LeftButton);
    QTRY_COMPARE(refreshed.count(), 1);
    QCOMPARE(refreshed.constFirst().constFirst().toUrl(),
             QUrl::fromLocalFile(QDir::homePath()));

    QVERIFY(history->menu());
    history->menu()->popup(QPoint(0, 0));
    QTRY_VERIFY(!history->menu()->actions().isEmpty());
    history->menu()->hide();

    view->setVisibleRoles({"text", "path", "deletiontime", "size", "type"});
    view->setVisibleRoles({"text", "size", "type", "modificationtime"});
    verifyNormalFolderHeader();

    const QImage titleIcon = m_mainWindow->windowIcon().pixmap(16, 16).toImage();
    QVERIFY(!titleIcon.isNull());
    bool titleIconHasVisiblePixels = false;
    for (int y = 0; y < titleIcon.height() && !titleIconHasVisiblePixels; ++y) {
        for (int x = 0; x < titleIcon.width(); ++x) {
            if (titleIcon.pixelColor(x, y).alpha() != 0) {
                titleIconHasVisiblePixels = true;
                break;
            }
        }
    }
    QVERIFY2(titleIconHasVisiblePixels,
             "File Explorer must expose its Windows-style icon to the taskbar");
}

void DolphinMainWindowTest::testAero7LibraryPlaceActivation()
{
    m_mainWindow->openDirectories({QUrl::fromLocalFile(QDir::homePath())}, false);
    m_mainWindow->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_mainWindow.data()));

    PlacesPanel *placesPanel = m_mainWindow->m_placesPanel;
    QVERIFY(placesPanel && placesPanel->isVisible());
    placesPanel->viewport()->update();
    QTest::qWait(100);

    // Documents is the first child of the custom Libraries group.  A hidden
    // KDE ~/Documents place with the same display name may also be present;
    // clicking the painted row must activate Aero7's visible Library entry.
    QTest::mouseClick(placesPanel->viewport(), Qt::LeftButton, Qt::NoModifier,
                      QPoint(80, 146));
    const QUrl documentsLibrary = QUrl::fromLocalFile(
        Aero7Libraries::instance().materializedPath(QStringLiteral("documents")));
    QTRY_COMPARE(m_mainWindow->activeViewContainer()->url(), documentsLibrary);
}

void DolphinMainWindowTest::testAero7SystemDriveActivation()
{
    m_mainWindow->openDirectories({QUrl::fromLocalFile(QDir::homePath())}, false);
    m_mainWindow->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_mainWindow.data()));
    const QString path = QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
        .filePath(QStringLiteral("Aero7/Shell Places/Local Disk (C:)"));
    m_mainWindow->slotPlaceActivated(QUrl::fromLocalFile(path));
    QTRY_COMPARE(m_mainWindow->activeViewContainer()->url(), QUrl::fromLocalFile("/"));
    QTRY_VERIFY([&] {
        for (const auto *button : m_mainWindow->activeViewContainer()->urlNavigator()->findChildren<QAbstractButton *>()) {
            QString text = button->text();
            text.remove(QLatin1Char('&'));
            if (text == "Local Disk (C:)" && button->isVisible() && button->width() > 0)
                return true;
        }
        return false;
    }());
}

void DolphinMainWindowTest::testAero7BreadcrumbSurvivesPlacesRefresh()
{
    const QUrl root = QUrl::fromLocalFile(QStringLiteral("/"));
    m_mainWindow->openDirectories({root}, false);
    m_mainWindow->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_mainWindow.data()));
    auto *navigator = m_mainWindow->activeViewContainer()->urlNavigator();
    const auto labels = [navigator] {
        QStringList result;
        for (const auto *button : navigator->findChildren<QAbstractButton *>()) {
            if (QString::fromLatin1(button->metaObject()->className()).endsWith(QLatin1String("KUrlNavigatorButton"))
                && button->isVisible() && button->width() > 0) {
                QString text = button->text();
                text.remove(QLatin1Char('&'));
                result.append(text);
            }
        }
        return result;
    };
    const QStringList expected{QStringLiteral("Local Disk (C:)")};
    QTRY_COMPARE(labels(), expected);
    // Let the navigation-time repair finish before an unrelated model update.
    // A busy-drive state change does not change this URL or the mount table.
    QTest::qWait(150);
    auto *places = m_mainWindow->m_placesPanel->model();
    QVERIFY(places->rowCount() > 0);
    for (int refresh = 0; refresh < 3; ++refresh) {
        Q_EMIT places->dataChanged(places->index(0, 0), places->index(places->rowCount() - 1, 0), {Qt::DisplayRole});
        QTRY_COMPARE(labels(), expected);
        QTest::qWait(150);
        QCOMPARE(labels(), expected);
        QCOMPARE(navigator->locationUrl(), root);
    }
}

void DolphinMainWindowTest::testAero7NoInventedCdPlace()
{
    const auto *model = m_mainWindow->m_placesPanel->model();
    const QString oldPath = QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
        .filePath(QStringLiteral("Aero7/Shell Places/CD Drive (D:)"));
    for (int row = 0; row < model->rowCount(); ++row)
        QVERIFY(model->index(row, 0).data(KFilePlacesModel::UrlRole).toUrl()
                != QUrl::fromLocalFile(oldPath));
}

void DolphinMainWindowTest::testAero7PlacesMenuRejectsInvalidOrForeignIndex()
{
    auto *panel = m_mainWindow->m_placesPanel;
    QMenu menu;
    panel->populateAero7ContextMenu(menu, QModelIndex());
    QVERIFY(menu.isEmpty());
    QStandardItemModel foreign(1, 1);
    panel->populateAero7ContextMenu(menu, foreign.index(0, 0));
    QVERIFY(menu.isEmpty());
}

void DolphinMainWindowTest::testAero7PlacesMenuUsesNativeStorageCapabilities()
{
    auto *panel = m_mainWindow->m_placesPanel;
    auto *places = qobject_cast<KFilePlacesModel *>(panel->model());
    QVERIFY(places);
    int visiblePlaces = 0;
    for (int row = 0; row < places->rowCount(); ++row) {
        const auto index = places->index(row, 0);
        QMenu menu;
        panel->populateAero7ContextMenu(menu, index);
        if (places->isHidden(index)) {
            QVERIFY(menu.isEmpty());
            continue;
        }
        ++visiblePlaces;
        QVERIFY(menu.actions().size() >= 2);
        QCOMPARE(menu.actions().at(0)->text(), QStringLiteral("Open"));
        QCOMPARE(menu.actions().at(1)->text(), QStringLiteral("Open in new window"));
        auto *eject = menu.findChild<QAction *>(QStringLiteral("aero7EjectDrive"));
        auto *unmount = menu.findChild<QAction *>(QStringLiteral("aero7UnmountDrive"));
        const std::unique_ptr<QAction> expectedEject(places->ejectActionForIndex(index));
        const std::unique_ptr<QAction> expectedUnmount(places->teardownActionForIndex(index));
        QCOMPARE(bool(eject), places->isDevice(index) && bool(expectedEject));
        QCOMPARE(bool(unmount), places->isDevice(index) && bool(expectedUnmount));
        if (unmount) {
            QCOMPARE(unmount->parent(), &menu);
            QCOMPARE(unmount->isEnabled(), expectedUnmount->isEnabled() && places->isTeardownAllowed(index));
        }
        if (eject) QCOMPARE(eject->parent(), &menu);
        for (auto *action : menu.actions()) {
            const QString label = QString(action->text()).remove(QLatin1Char('&'));
            QVERIFY(!label.contains(QStringLiteral("Edit")));
            QVERIFY(!label.contains(QStringLiteral("Hide")));
            QVERIFY(!label.contains(QStringLiteral("Partition")));
        }
        // Deliberately never trigger storage actions against host devices.
    }
    QVERIFY(visiblePlaces > 0);
}

void DolphinMainWindowTest::testAero7PlacesMenuDoesNotTearDownBookmarks()
{
    m_mainWindow->openDirectories({QUrl::fromLocalFile(QDir::homePath())}, false);
    auto *panel = m_mainWindow->m_placesPanel;
    const auto index = panel->aero7IndexForName(QStringLiteral("Downloads"));
    QVERIFY(index.isValid());
    QMenu menu;
    panel->populateAero7ContextMenu(menu, index);
    QVERIFY(!menu.findChild<QAction *>(QStringLiteral("aero7UnmountDrive")));
    QVERIFY(!menu.findChild<QAction *>(QStringLiteral("aero7EjectDrive")));
    QSignalSpy teardown(panel, &PlacesPanel::storageTearDownRequested);
    QSignalSpy opened(panel, &PlacesPanel::placeActivated);
    menu.actions().first()->trigger();
    QCOMPARE(teardown.count(), 0);
    QCOMPARE(opened.count(), 1);
}

void DolphinMainWindowTest::testAero7ComputerNavigatorUsesResolvablePlace()
{
    const auto home = QUrl::fromLocalFile(QDir::homePath());
    m_mainWindow->openDirectories({home}, false);
    m_mainWindow->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_mainWindow.data()));
    m_mainWindow->showAero7Computer();
    auto *navigator = m_mainWindow->activeViewContainer()->urlNavigator();
    QVERIFY2(navigator->locationUrl().isLocalFile(), "Computer must not launch KIO jobs for an unregistered protocol");
    QVERIFY(QFileInfo::exists(navigator->locationUrl().toLocalFile()));
    QTest::qWait(300);
    QStringList labels;
    for (const auto *button : navigator->findChildren<QAbstractButton *>()) {
        if (button->isVisible() && button->width() > 0
            && QString::fromLatin1(button->metaObject()->className()).endsWith("KUrlNavigatorButton"))
            labels.append(QString(button->text()).remove('&'));
    }
    QCOMPARE(labels, QStringList{QStringLiteral("Computer")});
    m_mainWindow->goBack();
    QTRY_COMPARE(navigator->locationUrl(), home);
}

void DolphinMainWindowTest::testAero7SidebarHasNoMissingLibraryGap()
{
    m_mainWindow->openDirectories({QUrl::fromLocalFile(QDir::homePath())}, false);
    m_mainWindow->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_mainWindow.data()));
    auto *panel = m_mainWindow->m_placesPanel;
    panel->viewport()->update();
    QTest::qWait(100);
    // On a fresh profile Pictures immediately follows Documents and Music.
    // A nonexistent "New Library" must not consume a painted row.
    QTest::mouseClick(panel->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(80, 188));
    QTRY_COMPARE(m_mainWindow->activeViewContainer()->url(),
                 QUrl::fromLocalFile(Aero7Libraries::instance().materializedPath(QStringLiteral("pictures"))));
}

void DolphinMainWindowTest::testAero7SidebarDefaultLabelsFit()
{
    m_mainWindow->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_mainWindow.data()));
    auto *panel = m_mainWindow->m_placesPanel;
    QTest::qWait(100);
    for (const auto &label : {QStringLiteral("Local Disk (C:)"), QStringLiteral("Recent Places")})
        QVERIFY2(panel->viewport()->width() >= panel->fontMetrics().horizontalAdvance(label) + 54,
                 qPrintable(QStringLiteral("Default sidebar clips %1").arg(label)));
}

void DolphinMainWindowTest::testAero7ComputerBackForwardHistory()
{
    const auto root = QUrl::fromLocalFile(QStringLiteral("/"));
    const auto pictures = QUrl::fromLocalFile(Aero7Libraries::instance().materializedPath(QStringLiteral("pictures")));
    m_mainWindow->openDirectories({root}, false);
    m_mainWindow->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_mainWindow.data()));
    m_mainWindow->changeUrl(pictures);
    QTRY_COMPARE(m_mainWindow->activeViewContainer()->url(), pictures);
    QTest::qWait(100);
    m_mainWindow->showAero7Computer();
    QTRY_COMPARE(m_mainWindow->m_aero7ContentStack->currentIndex(), 1);
    m_mainWindow->goBack();
    QTRY_COMPARE(m_mainWindow->activeViewContainer()->url(), pictures);
    QCOMPARE(m_mainWindow->m_aero7ContentStack->currentIndex(), 0);
    m_mainWindow->goForward();
    QTRY_COMPARE(m_mainWindow->m_aero7ContentStack->currentIndex(), 1);
    QCOMPARE(m_mainWindow->m_placesPanel->currentIndex().data().toString(), QStringLiteral("Computer"));
    m_mainWindow->goBack();
    QTRY_COMPARE(m_mainWindow->activeViewContainer()->url(), pictures);
    m_mainWindow->goBack();
    QTRY_COMPARE(m_mainWindow->activeViewContainer()->url(), root);
    m_mainWindow->goForward();
    QTRY_COMPARE(m_mainWindow->activeViewContainer()->url(), pictures);
    m_mainWindow->goForward();
    QTRY_COMPARE(m_mainWindow->m_aero7ContentStack->currentIndex(), 1);
}

void DolphinMainWindowTest::testAero7ComputerLaunchSelectsItsPlace()
{
    m_mainWindow->openDirectories({QUrl(QStringLiteral("aero7computer:/"))}, false);
    m_mainWindow->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_mainWindow.data()));
    QTRY_COMPARE(m_mainWindow->m_aero7ContentStack->currentIndex(), 1);
    QTRY_COMPARE(m_mainWindow->m_placesPanel->currentIndex().data().toString(), QStringLiteral("Computer"));
    QVERIFY(m_mainWindow->activeViewContainer()->url().isLocalFile());
    QCOMPARE(m_mainWindow->activeViewContainer()->url().fileName(), QStringLiteral("Computer"));
}

void DolphinMainWindowTest::testAero7ComputerTabRestoresSurface()
{
    m_mainWindow->openDirectories({QUrl(QStringLiteral("aero7computer:/"))}, false);
    m_mainWindow->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_mainWindow.data()));
    QTRY_COMPARE(m_mainWindow->m_aero7ContentStack->currentIndex(), 1);
    const int computerTab = m_mainWindow->m_tabWidget->currentIndex();
    m_mainWindow->openNewTabAndActivate(QUrl::fromLocalFile(QDir::homePath()));
    const int homeTab = m_mainWindow->m_tabWidget->currentIndex();
    QVERIFY(homeTab != computerTab);
    QTRY_COMPARE(m_mainWindow->m_aero7ContentStack->currentIndex(), 0);
    m_mainWindow->m_tabWidget->setCurrentIndex(computerTab);
    QTRY_COMPARE(m_mainWindow->m_aero7ContentStack->currentIndex(), 1);
    QCOMPARE(m_mainWindow->m_placesPanel->currentIndex().data().toString(), QStringLiteral("Computer"));
    m_mainWindow->m_tabWidget->setCurrentIndex(homeTab);
    QTRY_COMPARE(m_mainWindow->m_aero7ContentStack->currentIndex(), 0);
    QCOMPARE(m_mainWindow->activeViewContainer()->url(), QUrl::fromLocalFile(QDir::homePath()));
}

void DolphinMainWindowTest::testAero7ComputerTabsRemainMouseAccessible()
{
    m_mainWindow->openDirectories({QUrl(QStringLiteral("aero7computer:/"))}, false);
    m_mainWindow->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_mainWindow.data()));
    QTRY_COMPARE(m_mainWindow->m_aero7ContentStack->currentIndex(), 1);
    auto *tabs = m_mainWindow->m_tabWidget;
    const int computerTab = tabs->currentIndex();
    m_mainWindow->openNewTabAndActivate(QUrl::fromLocalFile(QDir::homePath()));
    const int homeTab = tabs->currentIndex();
    auto *bar = tabs->tabBar();
    QTRY_VERIFY(bar->isVisible());
    QTest::mouseClick(bar, Qt::LeftButton, Qt::NoModifier, bar->tabRect(computerTab).center());
    QTRY_COMPARE(tabs->currentIndex(), computerTab);
    QTRY_COMPARE(m_mainWindow->m_aero7ContentStack->currentIndex(), 1);
    QTRY_VERIFY(bar->isVisible());
    QVERIFY(bar->visibleRegion().contains(bar->tabRect(homeTab).center()));
    QTest::mouseClick(bar, Qt::LeftButton, Qt::NoModifier, bar->tabRect(homeTab).center());
    QTRY_COMPARE(tabs->currentIndex(), homeTab);
    QTRY_COMPARE(m_mainWindow->m_aero7ContentStack->currentIndex(), 0);
    QTest::mouseClick(bar, Qt::LeftButton, Qt::NoModifier, bar->tabRect(computerTab).center());
    QTRY_COMPARE(m_mainWindow->m_aero7ContentStack->currentIndex(), 1);
    tabs->closeTab(computerTab);
    QTRY_COMPARE(tabs->count(), 1);
    QTRY_COMPARE(m_mainWindow->m_aero7ContentStack->currentIndex(), 0);
    QCOMPARE(m_mainWindow->activeViewContainer()->url(), QUrl::fromLocalFile(QDir::homePath()));
}

void DolphinMainWindowTest::testAero7ComputerSplitPaneRemainsVisible()
{
    m_mainWindow->openDirectories({QUrl(QStringLiteral("aero7computer:/"))}, false);
    m_mainWindow->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_mainWindow.data()));
    QTRY_COMPARE(m_mainWindow->m_aero7ContentStack->currentIndex(), 1);
    auto *page = m_mainWindow->m_tabWidget->currentTabPage();
    QPointer<QStackedWidget> computerStack = m_mainWindow->m_aero7ContentStack;
    page->setSplitViewEnabled(true, WithoutAnimation, QUrl::fromLocalFile(QDir::homePath()));
    QTRY_VERIFY(!page->primaryViewActive());
    QTRY_COMPARE(m_mainWindow->m_aero7ContentStack->currentIndex(), 0);
    QVERIFY(computerStack->isVisible());
    QCOMPARE(computerStack->currentIndex(), 1);
    QVERIFY(page->secondaryViewContainer()->view()->isVisible());
    QTRY_VERIFY(!page->primaryViewContainer()->statusBarWidget()->isVisible());
    QVERIFY(page->secondaryViewContainer()->statusBarWidget()->isVisible());
    page->primaryViewContainer()->statusBarWidget()->updateMode();
    QTRY_VERIFY(!page->primaryViewContainer()->statusBarWidget()->isVisible());
    page->primaryViewContainer()->setActive(true);
    QTRY_VERIFY(page->primaryViewActive());
    QCOMPARE(m_mainWindow->m_aero7ContentStack, computerStack.data());
    QVERIFY(page->secondaryViewContainer()->view()->isVisible());
    QTRY_VERIFY(!page->secondaryViewContainer()->statusBarWidget()->isVisible());
    QVERIFY(page->primaryViewContainer()->statusBarWidget()->isVisible());
    page->secondaryViewContainer()->statusBarWidget()->updateMode();
    QTRY_VERIFY(!page->secondaryViewContainer()->statusBarWidget()->isVisible());
    m_mainWindow->changeUrl(QUrl::fromLocalFile(QDir::rootPath()));
    QTRY_COMPARE(computerStack->currentIndex(), 0);
    QVERIFY(page->secondaryViewContainer()->view()->isVisible());
    QCOMPARE(page->secondaryViewContainer()->url(), QUrl::fromLocalFile(QDir::homePath()));
    QTRY_VERIFY(!page->secondaryViewContainer()->statusBarWidget()->isVisible());
    QVERIFY(page->primaryViewContainer()->statusBarWidget()->isVisible());
}

void DolphinMainWindowTest::testAero7SplitBreadcrumbFollowsActiveTab()
{
    m_mainWindow->openDirectories({QUrl(QStringLiteral("aero7computer:/"))}, false);
    m_mainWindow->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_mainWindow.data()));
    auto *tabs = m_mainWindow->m_tabWidget;
    auto *page = tabs->currentTabPage();
    auto *secondaryHole = m_mainWindow->findChild<QWidget *>(QStringLiteral("secondaryNavHole"));
    QVERIFY(secondaryHole);
    QVERIFY(!secondaryHole->isVisible());
    const int splitTab = tabs->currentIndex();
    page->setSplitViewEnabled(true, WithoutAnimation, QUrl::fromLocalFile(QDir::rootPath()));
    QTRY_VERIFY(secondaryHole->isVisible());
    page->setSplitViewEnabled(false, WithoutAnimation);
    QTRY_VERIFY(!secondaryHole->isVisible());
    page->setSplitViewEnabled(true, WithoutAnimation, QUrl::fromLocalFile(QDir::rootPath()));
    QTRY_VERIFY(secondaryHole->isVisible());
    tabs->openNewActivatedTab(QUrl(QStringLiteral("aero7computer:/")));
    QTRY_VERIFY(!secondaryHole->isVisible());
    tabs->setCurrentIndex(splitTab);
    QTRY_VERIFY(secondaryHole->isVisible());
    tabs->closeTab(splitTab);
    QTRY_VERIFY(!secondaryHole->isVisible());
}

void DolphinMainWindowTest::testAero7NormalDetailsModeIsIdempotent()
{
    DolphinStatusBar details(nullptr);
    details.setDefaultText(QStringLiteral("4 items"));
    auto *label = details.findChild<KSqueezedTextLabel *>();
    QVERIFY(label);
    QTRY_COMPARE(label->fullText(), QStringLiteral("4 items"));
    details.setComputerMode(false);
    QTRY_COMPARE(label->fullText(), QStringLiteral("4 items"));
    details.setHoveredItemText(QStringLiteral("photo.png"));
    QTRY_COMPARE(label->fullText(), QStringLiteral("photo.png"));
    details.setComputerMode(false);
    QTRY_COMPARE(label->fullText(), QStringLiteral("photo.png"));
    details.setHoveredItemText(QString());
    QTRY_COMPARE(label->fullText(), QStringLiteral("4 items"));
}

void DolphinMainWindowTest::testAero7FolderDetailsSurviveTabAndSplitChanges()
{
    TestDir first;
    TestDir second;
    first.createFile(QStringLiteral("one.txt"));
    second.createFiles({QStringLiteral("two.txt"), QStringLiteral("three.txt")});
    m_mainWindow->openDirectories({first.url()}, false);
    m_mainWindow->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_mainWindow.data()));
    const auto text = [](DolphinViewContainer *container) {
        auto *label = container->statusBarWidget()->findChild<KSqueezedTextLabel *>();
        return label ? label->fullText() : QString();
    };
    auto *tabs = m_mainWindow->m_tabWidget;
    auto *firstView = tabs->currentTabPage()->activeViewContainer();
    QTRY_COMPARE(text(firstView), QStringLiteral("1 item"));
    const QString firstText = text(firstView);
    const int firstTab = tabs->currentIndex();
    tabs->openNewActivatedTab(second.url());
    auto *secondView = tabs->currentTabPage()->activeViewContainer();
    QTRY_COMPARE(text(secondView), QStringLiteral("2 items"));
    const QString secondText = text(secondView);
    QVERIFY(firstText != secondText); // Distinct real directory counts.
    tabs->setCurrentIndex(firstTab);
    QTRY_COMPARE(text(firstView), firstText);
    auto *page = tabs->currentTabPage();
    page->setSplitViewEnabled(true, WithAnimation, second.url());
    QTRY_COMPARE(text(page->activeViewContainer()), secondText);
    page->primaryViewContainer()->setActive(true);
    QTRY_COMPARE(text(page->activeViewContainer()), firstText);
    page->secondaryViewContainer()->setActive(true);
    QTRY_COMPARE(text(page->activeViewContainer()), secondText);
    page->setSplitViewEnabled(false, WithAnimation);
    const QString remainingText = page->activeViewContainer()->url() == first.url() ? firstText : secondText;
    QTRY_COMPARE(text(page->activeViewContainer()), remainingText);
    tabs->openNewActivatedTab(QUrl(QStringLiteral("aero7computer:/")));
    tabs->setCurrentIndex(firstTab);
    QTRY_COMPARE(text(page->activeViewContainer()), remainingText);
}

void DolphinMainWindowTest::testAero7WindowFitsAvailableScreen()
{
    m_mainWindow->openDirectories({QUrl::fromLocalFile(QDir::homePath())}, false);
    m_mainWindow->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_mainWindow.data()));

    QScreen *windowScreen = m_mainWindow->screen();
    QVERIFY(windowScreen);
    const QRect available = windowScreen->availableGeometry();
    QVERIFY(available.isValid());

    // Model a SPICE client shrinking its virtual monitor while a normal
    // Explorer window still has the old, wider geometry.
    m_mainWindow->resize(available.width() + 200, m_mainWindow->height());
    m_mainWindow->constrainAero7WindowToScreen();
    QTRY_VERIFY(m_mainWindow->frameGeometry().width() <= available.width());
    QTRY_VERIFY(m_mainWindow->frameGeometry().height() <= available.height());

    // Switching between the integrated Computer surface and a normal folder
    // must not reintroduce the stale width.
    const int fittedWidth = m_mainWindow->width();
    m_mainWindow->showAero7Computer();
    QCOMPARE(m_mainWindow->width(), fittedWidth);
    m_mainWindow->m_winHeader->setComputerMode(false);
    QTRY_COMPARE(m_mainWindow->width(), fittedWidth);
}

void DolphinMainWindowTest::testAero7ComputerDetailsNotClipped_data()
{
    QTest::addColumn<int>("pointSize");
    QTest::newRow("normal") << 9;
    QTest::newRow("large") << 12;
    QTest::newRow("accessibility") << 18;
}

void DolphinMainWindowTest::testAero7ComputerDetailsNotClipped()
{
    QFETCH(int, pointSize);
    QFont font = m_mainWindow->font();
    font.setPointSize(pointSize);
    m_mainWindow->setFont(font);
    m_mainWindow->openDirectories({QUrl::fromLocalFile(QDir::homePath())}, false);
    m_mainWindow->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_mainWindow.data()));
    m_mainWindow->showAero7Computer();
    auto *details = m_mainWindow->activeViewContainer()->statusBarWidget();
    KSqueezedTextLabel *label = nullptr;
    for (auto *candidate : details->findChildren<KSqueezedTextLabel *>()) {
        if (candidate->fullText().contains(QStringLiteral("\nProcessor:"))) {
            label = candidate;
            break;
        }
    }
    QVERIFY(label);
    const int twoLines = label->fontMetrics().height() + label->fontMetrics().lineSpacing();
    QTRY_VERIFY2(label->height() >= twoLines, "Computer's processor line is clipped by a one-line label height");
    QTRY_VERIFY(details->rect().contains(QRect(label->mapTo(details, QPoint(0, 0)), label->size())));
}

void DolphinMainWindowTest::testFocusPlacesPanel()
{
    m_mainWindow->openDirectories({QUrl::fromLocalFile(QDir::homePath())}, false);
    m_mainWindow->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_mainWindow.data()));
    QVERIFY(m_mainWindow->isVisible());
    QTRY_VERIFY_WITH_TIMEOUT(QApplication::activeWindow() != nullptr, 100);

    QWidget *placesPanel = reinterpret_cast<QWidget *>(m_mainWindow->m_placesPanel);
    QVERIFY2(QTest::qWaitFor(
                 [&]() {
                     return placesPanel && placesPanel->isVisible() && placesPanel->width() > 0 && placesPanel->height() > 0;
                 },
                 5000),
             "The test couldn't be initialised properly. The places panel should be visible.");

    QAction *focusPlacesPanelAction = m_mainWindow->actionCollection()->action(QStringLiteral("focus_places_panel"));
    QAction *showPlacesPanelAction = m_mainWindow->actionCollection()->action(QStringLiteral("show_places_panel"));

    focusPlacesPanelAction->trigger();
    QVERIFY(placesPanel->hasFocus());

    focusPlacesPanelAction->trigger();
    QVERIFY2(m_mainWindow->activeViewContainer()->isAncestorOf(QApplication::focusWidget()),
             "Triggering focus_places_panel while the panel already has focus should return the focus to the view.");

    focusPlacesPanelAction->trigger();
    QVERIFY(placesPanel->hasFocus());

    showPlacesPanelAction->trigger();
    QVERIFY(!placesPanel->isVisible());
    QVERIFY2(m_mainWindow->activeViewContainer()->isAncestorOf(QApplication::focusWidget()),
             "Hiding the Places panel while it has focus should return the focus to the view.");

    showPlacesPanelAction->trigger();
    QVERIFY(placesPanel->isVisible());
    QVERIFY2(placesPanel->hasFocus(), "Enabling the Places panel should move keyboard focus there.");

    /// Test that activating a place always moves focus to the view.
    QTest::keyClick(QApplication::focusWidget(), Qt::Key::Key_Enter);
    QVERIFY2(m_mainWindow->activeViewContainer()->isAncestorOf(QApplication::focusWidget()),
             "Activating a place should move focus to the view that loads that place.");

    focusPlacesPanelAction->trigger();
    QVERIFY(placesPanel->hasFocus());

    QTest::keyClick(QApplication::focusWidget(), Qt::Key::Key_Enter);
    QVERIFY2(m_mainWindow->activeViewContainer()->isAncestorOf(QApplication::focusWidget()),
             "Activating a place should move focus to the view even if the view already has that place loaded.");
}

/**
 * The places panel will resize itself if any of the other widgets requires too much horizontal space
 * but a user never wants the size of the places panel to change unless they resized it themselves explicitly.
 */
void DolphinMainWindowTest::testPlacesPanelWidthResistance()
{
    m_mainWindow->openDirectories({QUrl::fromLocalFile(QDir::homePath())}, false);
    m_mainWindow->show();
    m_mainWindow->resize(800, m_mainWindow->height()); // make sure the size is sufficient so a places panel resize shouldn't be necessary.
    QVERIFY(QTest::qWaitForWindowExposed(m_mainWindow.data()));
    QVERIFY(m_mainWindow->isVisible());

    QWidget *placesPanel = reinterpret_cast<QWidget *>(m_mainWindow->m_placesPanel);
    QVERIFY2(QTest::qWaitFor(
                 [&]() {
                     return placesPanel && placesPanel->isVisible() && placesPanel->width() > 0;
                 },
                 5000),
             "The test couldn't be initialised properly. The places panel should be visible.");
    QTest::qWait(100);
    const int initialPlacesPanelWidth = placesPanel->width();

    m_mainWindow->actionCollection()->action(QStringLiteral("split_view"))->trigger(); // enable split view (starts animation)
    QTest::qWait(300); // wait for animation
    QCOMPARE(placesPanel->width(), initialPlacesPanelWidth);

    m_mainWindow->actionCollection()->action(QStringLiteral("show_filter_bar"))->trigger();
    QCOMPARE(placesPanel->width(), initialPlacesPanelWidth);

    // Make all selection mode bars appear and test for each that this doesn't affect the places panel's width.
    // One of the bottom bars (SelectionMode::BottomBar::GeneralContents) only shows up when at least one item is selected so we do that before we begin iterating.
    m_mainWindow->actionCollection()->action(KStandardAction::name(KStandardAction::SelectAll))->trigger();
    for (int selectionModeStates = SelectionMode::BottomBar::CopyContents; selectionModeStates != SelectionMode::BottomBar::RenameContents;
         selectionModeStates++) {
        const auto contents = static_cast<SelectionMode::BottomBar::Contents>(selectionModeStates);
        m_mainWindow->slotSetSelectionMode(true, contents);
        QTest::qWait(20); // give time for a paint/resize
        QCOMPARE(placesPanel->width(), initialPlacesPanelWidth);
    }

    m_mainWindow->actionCollection()->action(KStandardAction::name(KStandardAction::Find))->trigger();
    QCOMPARE(placesPanel->width(), initialPlacesPanelWidth);

#if HAVE_BALOO
    m_mainWindow->actionCollection()->action(QStringLiteral("show_information_panel"))->setChecked(true); // toggle visible
    QCOMPARE(placesPanel->width(), initialPlacesPanelWidth);
#endif

#if HAVE_TERMINAL
    m_mainWindow->actionCollection()->action(QStringLiteral("show_terminal_panel"))->setChecked(true); // toggle visible
    QCOMPARE(placesPanel->width(), initialPlacesPanelWidth);
#endif

    m_mainWindow->actionCollection()->action(QStringLiteral("split_view"))->trigger(); // disable split view (starts animation)
    QCOMPARE(placesPanel->width(), initialPlacesPanelWidth);

#if HAVE_BALOO
    m_mainWindow->actionCollection()->action(QStringLiteral("show_information_panel"))->trigger(); // toggle invisible
    QCOMPARE(placesPanel->width(), initialPlacesPanelWidth);
#endif

#if HAVE_TERMINAL
    m_mainWindow->actionCollection()->action(QStringLiteral("show_terminal_panel"))->trigger(); // toggle invisible
    QCOMPARE(placesPanel->width(), initialPlacesPanelWidth);
#endif

    m_mainWindow->showMaximized();
    QCOMPARE(placesPanel->width(), initialPlacesPanelWidth);

    QTest::qWait(300); // wait for split view closing animation
    QCOMPARE(placesPanel->width(), initialPlacesPanelWidth);
}

void DolphinMainWindowTest::testGoActions()
{
    QScopedPointer<TestDir> testDir{new TestDir()};
    testDir->createDir("a");
    testDir->createDir("b");
    testDir->createDir("b/b-1");
    testDir->createFile("b/b-2");
    testDir->createDir("c");
    const QUrl childDirUrl(QDir::cleanPath(testDir->url().toString() + "/b"));
    m_mainWindow->openDirectories({childDirUrl}, false); // Open "b" dir
    m_mainWindow->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_mainWindow.data()));
    QVERIFY(m_mainWindow->isVisible());
    QVERIFY(!m_mainWindow->actionCollection()->action(KStandardAction::name(KStandardAction::Forward))->isEnabled());

    m_mainWindow->actionCollection()->action(KStandardAction::name(KStandardAction::Up))->trigger();
    /**
     * Now, after going "up" in the file hierarchy (to "testDir"), the folder one has emerged from ("b") should have keyboard focus.
     * This is especially important when a user wants to peek into multiple folders in quick succession.
     */
    QSignalSpy spyDirectoryLoadingCompleted(m_mainWindow->m_activeViewContainer->view(), &DolphinView::directoryLoadingCompleted);
    QVERIFY(spyDirectoryLoadingCompleted.wait());
    QVERIFY(QTest::qWaitFor([&]() {
        return !m_mainWindow->actionCollection()->action(QStringLiteral("stop"))->isEnabled();
    })); // "Stop" command should be disabled because it finished loading
    QTest::qWait(500); // Somehow the item we emerged from doesn't have keyboard focus yet if we don't wait a split second.
    const QUrl parentDirUrl = m_mainWindow->activeViewContainer()->url();
    QVERIFY(parentDirUrl != childDirUrl);

    auto currentItemUrl = [this]() {
        const int currentIndex = m_mainWindow->m_activeViewContainer->view()->m_container->controller()->selectionManager()->currentItem();
        const KFileItem currentItem = m_mainWindow->m_activeViewContainer->view()->m_model->fileItem(currentIndex);
        return currentItem.url();
    };

    QCOMPARE(currentItemUrl(), childDirUrl); // The item we just emerged from should now have keyboard focus.
    QCOMPARE(m_mainWindow->m_activeViewContainer->view()->selectedItems().count(), 1); // …and it should be selected, too.
    // Pressing arrow keys should not only move the keyboard focus but also select the item.
    // We press "Down" to select "c" below and then "Up" so the folder "b" we just emerged from is selected for the first time.
    m_mainWindow->actionCollection()->action(QStringLiteral("compact"))->trigger();
    QTest::keyClick(m_mainWindow->activeViewContainer()->view()->m_container, Qt::Key::Key_Down, Qt::NoModifier);
    QCOMPARE(m_mainWindow->m_activeViewContainer->view()->selectedItems().count(), 1);
    QVERIFY2(currentItemUrl() != childDirUrl, "The current item didn't change after pressing the 'Down' key.");
    QTest::keyClick(m_mainWindow->activeViewContainer()->view()->m_container, Qt::Key::Key_Up, Qt::NoModifier);
    QCOMPARE(m_mainWindow->m_activeViewContainer->view()->selectedItems().count(), 1);
    QCOMPARE(currentItemUrl(), childDirUrl); // After pressing 'Down' and then 'Up' we should be back where we were.

    // Enter the child folder "b".
    QTest::keyClick(m_mainWindow->activeViewContainer()->view()->m_container, Qt::Key::Key_Enter, Qt::NoModifier);
    QVERIFY(spyDirectoryLoadingCompleted.wait());
    QCOMPARE(m_mainWindow->activeViewContainer()->url(), childDirUrl);
    QVERIFY(m_mainWindow->isUrlOpen(childDirUrl.toString()));

    // Go back to the parent folder.
    m_mainWindow->actionCollection()->action(KStandardAction::name(KStandardAction::Back))->trigger();
    QVERIFY(spyDirectoryLoadingCompleted.wait());
    QTest::qWait(100); // Somehow the item we emerged from doesn't have keyboard focus yet if we don't wait a split second.
    QCOMPARE(m_mainWindow->activeViewContainer()->url(), parentDirUrl);
    QVERIFY(m_mainWindow->isUrlOpen(parentDirUrl.toString()));
    // Going 'Back' means that the view should be in the same state it was in when we left.
    QCOMPARE(currentItemUrl(), childDirUrl); // The item we last interacted with in this location should still have keyboard focus.
    QCOMPARE(m_mainWindow->m_activeViewContainer->view()->selectedItems().count(), 1);
    QCOMPARE(m_mainWindow->m_activeViewContainer->view()->selectedItems().constFirst().url(), childDirUrl); // It should still be selected.

    // Open a new tab for the "b" child dir and verify that this doesn't interfere with anything.
    QTest::keyClick(m_mainWindow->activeViewContainer()->view()->m_container, Qt::Key::Key_Enter, Qt::ControlModifier); // Open new inactive tab
    QVERIFY(m_mainWindow->m_tabWidget->count() == 2);
    QCOMPARE(m_mainWindow->activeViewContainer()->url(), parentDirUrl);
    QVERIFY(m_mainWindow->isUrlOpen(parentDirUrl.toString()));
    QVERIFY(!m_mainWindow->actionCollection()->action(QStringLiteral("undo_close_tab"))->isEnabled());

    // Go forward to the child folder.
    m_mainWindow->actionCollection()->action(KStandardAction::name(KStandardAction::Forward))->trigger();
    QVERIFY(spyDirectoryLoadingCompleted.wait());
    QCOMPARE(m_mainWindow->activeViewContainer()->url(), childDirUrl);
    QCOMPARE(m_mainWindow->m_activeViewContainer->view()->selectedItems().count(), 0); // There was no action in this view yet that would warrant a selection.
    QCOMPARE(currentItemUrl(), QUrl(QDir::cleanPath(testDir->url().toString() + "/b/b-1"))); // The first item in the view should have keyboard focus.

    // Press the 'Down' key in the child folder.
    QTest::keyClick(m_mainWindow->activeViewContainer()->view()->m_container, Qt::Key::Key_Down, Qt::NoModifier);
    // The second item in the view should have keyboard focus and be selected.
    const QUrl secondItemInChildFolderUrl{QDir::cleanPath(testDir->url().toString() + "/b/b-2")};
    QCOMPARE(currentItemUrl(), secondItemInChildFolderUrl);
    QCOMPARE(m_mainWindow->m_activeViewContainer->view()->selectedItems().count(), 1);
    QCOMPARE(m_mainWindow->m_activeViewContainer->view()->selectedItems().constFirst().url(), secondItemInChildFolderUrl);

    // Go back to the parent folder and then re-enter the child folder.
    m_mainWindow->actionCollection()->action(KStandardAction::name(KStandardAction::Back))->trigger();
    QVERIFY(spyDirectoryLoadingCompleted.wait());
    m_mainWindow->actionCollection()->action(KStandardAction::name(KStandardAction::Forward))->trigger();
    QVERIFY(spyDirectoryLoadingCompleted.wait());
    QCOMPARE(m_mainWindow->activeViewContainer()->url(), childDirUrl);
    // The state of the view should be identical to how it was before we triggered "Back" and then "Forward".
    QTRY_COMPARE(currentItemUrl(), secondItemInChildFolderUrl);
    QCOMPARE(m_mainWindow->m_activeViewContainer->view()->selectedItems().count(), 1);
    QCOMPARE(m_mainWindow->m_activeViewContainer->view()->selectedItems().constFirst().url(), secondItemInChildFolderUrl);

    // Go back to the parent folder.
    m_mainWindow->actionCollection()->action(KStandardAction::name(KStandardAction::Back))->trigger();
    QVERIFY(spyDirectoryLoadingCompleted.wait());
    QTest::qWait(100); // Somehow the item we emerged from doesn't have keyboard focus yet if we don't wait a split second.
    QCOMPARE(m_mainWindow->activeViewContainer()->url(), parentDirUrl);
    QVERIFY(m_mainWindow->isUrlOpen(parentDirUrl.toString()));

    // Close current tab and see if the "go" actions are correctly disabled in the remaining tab that was never active until now and shows the "b" dir
    m_mainWindow->actionCollection()->action(KStandardAction::name(KStandardAction::Close))->trigger(); // Close current tab
    QVERIFY(m_mainWindow->m_tabWidget->count() == 1);
    QCOMPARE(m_mainWindow->activeViewContainer()->url(), childDirUrl);
    QCOMPARE(m_mainWindow->m_activeViewContainer->view()->selectedItems().count(), 0); // There was no action in this tab yet that would warrant a selection.
    QVERIFY(!m_mainWindow->actionCollection()->action(KStandardAction::name(KStandardAction::Back))->isEnabled());
    QVERIFY(!m_mainWindow->actionCollection()->action(KStandardAction::name(KStandardAction::Forward))->isEnabled());
    QVERIFY(m_mainWindow->actionCollection()->action(QStringLiteral("undo_close_tab"))->isEnabled());
}

void DolphinMainWindowTest::testOpenFiles()
{
    QScopedPointer<TestDir> testDir{new TestDir()};
    QString testDirUrl(QDir::cleanPath(testDir->url().toString()));
    testDir->createDir("a");
    testDir->createDir("a/b");
    testDir->createDir("a/b/c");
    testDir->createDir("a/b/c/d");
    m_mainWindow->openDirectories({testDirUrl}, false);
    m_mainWindow->show();

    // We only see the unselected "a" folder in the test dir. There are no other tabs.
    QVERIFY(m_mainWindow->isUrlOpen(testDirUrl));
    QVERIFY(m_mainWindow->isItemVisibleInAnyView(testDirUrl + "/a"));
    QVERIFY(!m_mainWindow->isUrlOpen(testDirUrl + "/a"));
    QVERIFY(!m_mainWindow->isItemVisibleInAnyView(testDirUrl + "/a/b"));
    QCOMPARE(m_mainWindow->m_tabWidget->count(), 1);
    QCOMPARE(m_mainWindow->m_tabWidget->currentIndex(), 0);
    QCOMPARE(m_mainWindow->m_activeViewContainer->view()->selectedItems().count(), 0);

    // "a" is already in view, so "opening" "a" should simply select it without opening a new tab.
    m_mainWindow->openFiles({testDirUrl + "/a"}, false);
    QTRY_COMPARE(m_mainWindow->m_activeViewContainer->view()->selectedItems().count(), 1);
    QCOMPARE(m_mainWindow->m_tabWidget->count(), 1);
    QVERIFY(m_mainWindow->isItemVisibleInAnyView(testDirUrl + "/a"));

    // "b" is not in view, so "opening" "b" should open a new active tab of the parent folder "a" and select "b" there.
    m_mainWindow->openFiles({testDirUrl + "/a/b"}, false);
    QTRY_VERIFY(m_mainWindow->isUrlOpen(testDirUrl + "/a"));
    QCOMPARE(m_mainWindow->m_tabWidget->count(), 2);
    QCOMPARE(m_mainWindow->m_tabWidget->currentIndex(), 1);
    QTRY_VERIFY(m_mainWindow->isItemVisibleInAnyView(testDirUrl + "/a/b"));
    QVERIFY2(!m_mainWindow->isUrlOpen(testDirUrl + "/a/b"), "The directory b is supposed to be visible but not open in its own tab.");
    QTRY_COMPARE(m_mainWindow->m_activeViewContainer->view()->selectedItems().count(), 1);

    QVERIFY(m_mainWindow->isUrlOpen(testDirUrl));
    QVERIFY(m_mainWindow->isItemVisibleInAnyView(testDirUrl + "/a"));
    // "a" is still in view in the first tab, so "opening" "a" should switch to the first tab and select "a" there.
    m_mainWindow->openFiles({testDirUrl + "/a"}, false);
    QCOMPARE(m_mainWindow->m_tabWidget->count(), 2);
    QCOMPARE(m_mainWindow->m_tabWidget->currentIndex(), 0);
    QVERIFY(m_mainWindow->isUrlOpen(testDirUrl));
    QVERIFY(m_mainWindow->isUrlOpen(testDirUrl + "/a"));

    // Directory "a" is already open in the second tab in which "b" is selected, so opening the directory "a" should switch to that tab.
    m_mainWindow->openDirectories({testDirUrl + "/a"}, false);
    QCOMPARE(m_mainWindow->m_tabWidget->count(), 2);
    QCOMPARE(m_mainWindow->m_tabWidget->currentIndex(), 1);

    // In the details view mode directories can be expanded, which changes if openFiles() needs to open a new tab or not to open a file.
    m_mainWindow->actionCollection()->action(QStringLiteral("details"))->trigger();
    QTRY_VERIFY(m_mainWindow->activeViewContainer()->view()->itemsExpandable());

    // Expand the already selected "b" with the right arrow key. This should make "c" visible.
    QVERIFY2(!m_mainWindow->isItemVisibleInAnyView(testDirUrl + "/a/b/c"), "The parent folder wasn't expanded yet, so c shouldn't be visible.");
    QTest::keyClick(m_mainWindow->activeViewContainer()->view()->m_container, Qt::Key::Key_Right);
    QTRY_VERIFY(m_mainWindow->isItemVisibleInAnyView(testDirUrl + "/a/b/c"));
    QVERIFY2(!m_mainWindow->isUrlOpen(testDirUrl + "/a/b"), "b is supposed to be expanded, however it shouldn't be open in its own tab.");
    QVERIFY(m_mainWindow->isUrlOpen(testDirUrl + "/a"));

    // Switch to first tab by opening it even though it is already open.
    m_mainWindow->openDirectories({testDirUrl}, false);
    QCOMPARE(m_mainWindow->m_tabWidget->count(), 2);
    QCOMPARE(m_mainWindow->m_tabWidget->currentIndex(), 0);

    // "c" is in view in the second tab because "b" is expanded there, so "opening" "c" should switch to that tab and select "c" there.
    m_mainWindow->openFiles({testDirUrl + "/a/b/c"}, false);
    QCOMPARE(m_mainWindow->m_tabWidget->count(), 2);
    QCOMPARE(m_mainWindow->m_tabWidget->currentIndex(), 1);
    QTRY_COMPARE(m_mainWindow->m_activeViewContainer->view()->selectedItems().count(), 1);
    QVERIFY(m_mainWindow->isUrlOpen(testDirUrl));
    QVERIFY(m_mainWindow->isUrlOpen(testDirUrl + "/a"));

    // Opening the directory "c" on the other hand will open it in a new tab even though it is already visible in the view
    // because openDirecories() and openFiles() serve different purposes. One opens views at urls, the other selects files within views.
    m_mainWindow->openDirectories({testDirUrl + "/a/b/c/d", testDirUrl + "/a/b/c"}, true);
    QCOMPARE(m_mainWindow->m_tabWidget->count(), 3);
    QCOMPARE(m_mainWindow->m_tabWidget->currentIndex(), 2);
    QVERIFY(m_mainWindow->m_tabWidget->currentTabPage()->splitViewEnabled());
    QVERIFY(m_mainWindow->isItemVisibleInAnyView(testDirUrl + "/a/b/c")); // It should still be visible in the second tab.
    QTRY_COMPARE(m_mainWindow->m_activeViewContainer->view()->selectedItems().count(), 0);
    QVERIFY(m_mainWindow->isUrlOpen(testDirUrl + "/a/b/c/d"));
    QVERIFY(m_mainWindow->isUrlOpen(testDirUrl + "/a/b/c"));

    // "c" is in view in the second tab because "b" is expanded there,
    // so "opening" "c" should switch to that tab even though "c" as a directory is open in the current tab.
    m_mainWindow->openFiles({testDirUrl + "/a/b/c"}, false);
    QCOMPARE(m_mainWindow->m_tabWidget->count(), 3);
    QCOMPARE(m_mainWindow->m_tabWidget->currentIndex(), 1);
    QVERIFY2(m_mainWindow->isItemVisibleInAnyView(testDirUrl + "/a/b/c/d"), "It should be visible in the secondary view of the third tab.");

    // Select "b" and un-expand it with the left arrow key. This should make "c" invisible.
    m_mainWindow->openFiles({testDirUrl + "/a/b"}, false);
    QTest::keyClick(m_mainWindow->activeViewContainer()->view()->m_container, Qt::Key::Key_Left);
    QTRY_VERIFY(!m_mainWindow->isItemVisibleInAnyView(testDirUrl + "/a/b/c"));

    // "d" is in view in the third tab in the secondary view, so "opening" "d" should select that view.
    m_mainWindow->openFiles({testDirUrl + "/a/b/c/d"}, false);
    QCOMPARE(m_mainWindow->m_tabWidget->count(), 3);
    QCOMPARE(m_mainWindow->m_tabWidget->currentIndex(), 2);
    QVERIFY(m_mainWindow->m_tabWidget->currentTabPage()->secondaryViewContainer()->isActive());
    QTRY_COMPARE(m_mainWindow->m_activeViewContainer->view()->selectedItems().count(), 1);
}

void DolphinMainWindowTest::testAccessibilityTree()
{
    m_mainWindow->openDirectories({QUrl::fromLocalFile(QDir::homePath())}, false);
    m_mainWindow->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_mainWindow.data()));
    QVERIFY(m_mainWindow->isVisible());
    QTRY_VERIFY_WITH_TIMEOUT(QApplication::activeWindow() != nullptr, 100);

    // Breadcrumb buttons are reused and relabelled asynchronously. Start the
    // static focus-chain audit only once the current destination is reachable,
    // not midway through replacing the preceding window's breadcrumb buttons.
    QTRY_VERIFY([&] {
        const QString leaf = QFileInfo(QDir::homePath()).fileName();
        for (const auto *button : m_mainWindow->activeViewContainer()->urlNavigator()->findChildren<QAbstractButton *>()) {
            QString text = button->text();
            text.remove(QLatin1Char('&'));
            if (text == leaf && button->isVisible() && (button->focusPolicy() & Qt::TabFocus))
                return true;
        }
        return false;
    }());

    QAccessibleInterface *accessibleInterfaceOfMainWindow = QAccessible::queryAccessibleInterface(m_mainWindow.get());
    Q_ASSERT(accessibleInterfaceOfMainWindow);

    /// Test the accessibility of objects while traversing forwards (Tab key) and backwards (Shift+Tab).
    int testedObjectsSizeAfterTraversingForwards = 0;
    for (int i = 0; i < 2; i++) {
        std::tuple<Qt::Key, Qt::KeyboardModifier> focusChainTraversalKeyCombination = {Qt::Key::Key_Tab, Qt::NoModifier};
        if (i) {
            focusChainTraversalKeyCombination = {Qt::Key::Key_Tab, Qt::ShiftModifier};
        }

        /// @see firstNamedAncestor below.
        QAccessibleInterface *firstNamedAncestorOfPreviousIteration = nullptr;

        /// Perform accessibility checks for every object that gets focus. Focus will be changed using the focusChainTraversalKeyCombination.
        std::set<const QObject *> testedObjects; // Makes sure we stop testing when we arrive at an item that was already tested.
        while (qApp->focusObject() && !testedObjects.count(qApp->focusObject())) {
            const auto currentlyFocusedObject = qApp->focusObject();
            QVERIFY2(!currentlyFocusedObject->property("aero7LinuxPrefix").toBool(),
                     "Hidden implementation-path breadcrumbs must not receive keyboard focus.");
            if (qEnvironmentVariableIsSet("AERO7_TRACE_FOCUS")) {
                const auto *widget = qobject_cast<QWidget *>(currentlyFocusedObject);
                qInfo() << "AERO7_FOCUS" << i << currentlyFocusedObject
                        << (widget ? widget->geometry() : QRect())
                        << currentlyFocusedObject->property("text");
            }
            const QAccessibleInterface *accessibleIntefaceOfCurrentlyFocusedObject = QAccessible::queryAccessibleInterface(currentlyFocusedObject);
            QVERIFY(accessibleIntefaceOfCurrentlyFocusedObject);

            /// Test that each object reachable by Tab or Shift+Tab has at least some accessible information. Objects without any accessible information
            /// are even less useful to accessibility software users than unlabeled buttons are e.g. to sighted users, because unlabeled buttons at least
            /// convey some information through their placement and icon.
            if (currentlyFocusedObject != m_mainWindow->m_activeViewContainer->view()->m_container) { // Skip the custom container widget which has no
                                                                                                      // accessible name on purpose.
                /**
                 * The first ancestor with an accessible name is interesting because it is sometimes used to identify an object if the object itself has no
                 * name. We keep it in mind to check if two subsequent objects without a name can at least be told apart by their first named ancestor.
                 */
                QAccessibleInterface *firstNamedAncestor = accessibleIntefaceOfCurrentlyFocusedObject->parent();
                while (firstNamedAncestor) {
                    if (!firstNamedAncestor->text(QAccessible::Name).isEmpty()) {
                        break;
                    }
                    firstNamedAncestor = firstNamedAncestor->parent();
                }
                QTRY_VERIFY2(!accessibleIntefaceOfCurrentlyFocusedObject->text(QAccessible::Name).isEmpty()
                                 || (firstNamedAncestor && firstNamedAncestor != firstNamedAncestorOfPreviousIteration),
                             qPrintable(QStringLiteral("%1's accessibleInterface does not have an accessible name and can not be distinguished from the object"
                                                       " that had focus previously. Please fix this. You can find this %1 within its parent %2.")
                                            .arg(currentlyFocusedObject->metaObject()->className())
                                            .arg(currentlyFocusedObject->parent()->metaObject()->className())));
                firstNamedAncestorOfPreviousIteration = firstNamedAncestor;
            }

            /// Test that each accessible interface has the main window as its parent.
            QAccessibleInterface *accessibleInterface = QAccessible::queryAccessibleInterface(currentlyFocusedObject);
            // The accessibleInterfaces of focused objects might themselves have children.
            // We go down that hierarchy as far as possible and then test the ancestor tree from there.
            while (accessibleInterface->childCount() > 0) {
                accessibleInterface = accessibleInterface->child(0);
            }
            while (accessibleInterface != accessibleInterfaceOfMainWindow) {
                QVERIFY2(accessibleInterface,
                         qPrintable(QStringLiteral("%1's accessibleInterface or one of its accessible children doesn't have the main window as an ancestor.")
                                        .arg(currentlyFocusedObject->metaObject()->className())));
                accessibleInterface = accessibleInterface->parent();
            }

            testedObjects.insert(currentlyFocusedObject); // Add it to testedObjects so we won't test it again later.
            QTest::keyClick(m_mainWindow.get(), std::get<0>(focusChainTraversalKeyCombination), std::get<1>(focusChainTraversalKeyCombination));
            QVERIFY2(currentlyFocusedObject != qApp->focusObject(),
                     "The focus chain is broken. The focused object should have changed after pressing the focusChainTraversalKeyCombination.");
        }

        if (i == 0) {
            testedObjectsSizeAfterTraversingForwards = testedObjects.size();
        } else {
            QCOMPARE(testedObjects.size(), testedObjectsSizeAfterTraversingForwards); // The size after traversing backwards is different than
                                                                                      // after going forwards which is probably not intended.
        }
    }
    QCOMPARE_GE(testedObjectsSizeAfterTraversingForwards, 10); // The test did not reach many objects while using the Tab key to move through Dolphin. Did the
                                                               // test run correctly?
}

void DolphinMainWindowTest::testAutoSaveSession()
{
    m_mainWindow->openDirectories({QUrl::fromLocalFile(QDir::homePath())}, false);
    m_mainWindow->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_mainWindow.data()));
    QVERIFY(m_mainWindow->isVisible());

    // Create config file
    KConfigGui::setSessionConfig(QStringLiteral("dolphin"), QStringLiteral("dolphin"));
    KConfig *config = KConfigGui::sessionConfig();
    m_mainWindow->saveGlobalProperties(config);
    m_mainWindow->savePropertiesInternal(config, 1);
    config->sync();

    // Setup watcher for config file changes
    const QString configFileName = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/" + KConfigGui::sessionConfig()->name();
    QFileSystemWatcher *configWatcher = new QFileSystemWatcher({configFileName}, this);
    QSignalSpy spySessionSaved(configWatcher, &QFileSystemWatcher::fileChanged);

    // Enable session autosave.
    m_mainWindow->setSessionAutoSaveEnabled(true);
    m_mainWindow->m_sessionSaveTimer->setInterval(200); // Lower the interval to speed up the testing

    // Open a new tab
    auto tabWidget = m_mainWindow->findChild<DolphinTabWidget *>("tabWidget");
    QVERIFY(tabWidget);
    tabWidget->openNewActivatedTab(QUrl::fromLocalFile(QDir::tempPath()));
    QCOMPARE(tabWidget->count(), 2);

    // Wait till a session save occurs
    QVERIFY(spySessionSaved.wait(60000));

    // Disable session autosave.
    m_mainWindow->setSessionAutoSaveEnabled(false);
}

void DolphinMainWindowTest::testInlineRename()
{
    QScopedPointer<TestDir> testDir{new TestDir()};
    testDir->createFiles({"aaaa", "bbbb", "cccc", "dddd"});
    m_mainWindow->openDirectories({testDir->url()}, false);
    m_mainWindow->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_mainWindow.data()));
    QVERIFY(m_mainWindow->isVisible());

    DolphinView *view = m_mainWindow->activeViewContainer()->view();
    QSignalSpy viewDirectoryLoadingCompletedSpy(view, &DolphinView::directoryLoadingCompleted);
    QSignalSpy itemsReorderedSpy(view->m_model, &KFileItemModel::itemsMoved);
    QSignalSpy modelDirectoryLoadingCompletedSpy(view->m_model, &KFileItemModel::directoryLoadingCompleted);

    QVERIFY(viewDirectoryLoadingCompletedSpy.wait());
    QTest::qWait(500); // we need to wait for the file widgets to become visible
    view->markUrlsAsSelected({QUrl(testDir->url().toString() + "/aaaa")});
    view->updateViewState();
    view->renameSelectedItems();
    QTest::keyClick(QApplication::focusWidget(), Qt::Key_Left);
    QTest::keyClick(QApplication::focusWidget(), Qt::Key_E);
    QTest::keyClick(QApplication::focusWidget(), Qt::Key_Down);

    QVERIFY(itemsReorderedSpy.wait());
    QVERIFY(view->m_view->m_editingRole);
    KItemListWidget *widget = view->m_view->m_visibleItems.value(view->m_view->firstVisibleIndex());
    QVERIFY(!widget->editedRole().isEmpty());

    QTest::keyClick(QApplication::focusWidget(), Qt::Key_Left);
    QTest::keyClick(QApplication::focusWidget(), Qt::Key_A);
    QTest::keyClick(QApplication::focusWidget(), Qt::Key_Down);
    QTest::keyClick(QApplication::focusWidget(), Qt::Key_Down);
    QTest::keyClick(QApplication::focusWidget(), Qt::Key_Left);
    QTest::keyClick(QApplication::focusWidget(), Qt::Key_A);
    QTest::keyClick(QApplication::focusWidget(), Qt::Key_Down);

    QVERIFY(itemsReorderedSpy.wait());
    QVERIFY(view->m_view->m_editingRole);
    widget = view->m_view->m_visibleItems.value(view->m_view->lastVisibleIndex());
    QVERIFY(!widget->editedRole().isEmpty());

    QTest::keyClick(QApplication::focusWidget(), Qt::Key_Escape);
    QVERIFY(widget->isCurrent());
    view->m_model->refreshDirectory(testDir->url());
    QVERIFY(modelDirectoryLoadingCompletedSpy.wait());

    QCOMPARE(view->m_model->fileItem(0).name(), "abbbb");
    QCOMPARE(view->m_model->fileItem(1).name(), "adddd");
    QCOMPARE(view->m_model->fileItem(2).name(), "cccc");
    QCOMPARE(view->m_model->fileItem(3).name(), "eaaaa");
    QCOMPARE(view->m_model->count(), 4);
}

void DolphinMainWindowTest::testThumbnailAfterRename()
{
    // Create testdir and red square jpg for testing
    QScopedPointer<TestDir> testDir{new TestDir()};
    QImage testImage(256, 256, QImage::Format_Mono);
    testImage.setColorCount(1);
    testImage.setColor(0, qRgba(255, 0, 0, 255)); // Index #0 = Red
    for (short x = 0; x < 256; ++x) {
        for (short y = 0; y < 256; ++y) {
            testImage.setPixel(x, y, 0);
        }
    }
    testImage.save(testDir.data()->path() + "/a.jpg");

    // Open dir and show it
    m_mainWindow->openDirectories({testDir->url()}, false);
    DolphinView *view = m_mainWindow->activeViewContainer()->view();
    // Prepare signal spies
    QSignalSpy viewDirectoryLoadingCompletedSpy(view, &DolphinView::directoryLoadingCompleted);
    QSignalSpy itemsChangedSpy(view->m_model, &KFileItemModel::itemsChanged);
    QSignalSpy modelDirectoryLoadingCompletedSpy(view->m_model, &KFileItemModel::directoryLoadingCompleted);
    QSignalSpy previewUpdatedSpy(view->m_view->m_modelRolesUpdater, &KFileItemModelRolesUpdater::previewJobFinished);
    // Show window and check that our preview has been updated, then wait for it to appear
    m_mainWindow->show();
    QVERIFY(viewDirectoryLoadingCompletedSpy.wait());
    QVERIFY(previewUpdatedSpy.wait());
    QVERIFY(QTest::qWaitForWindowExposed(m_mainWindow.data()));
    QVERIFY(m_mainWindow->isVisible());
    QTest::qWait(500); // we need to wait for the file widgets to become visible

    // Set image selected and rename it to b.jpg, make sure editing role is working
    view->markUrlsAsSelected({QUrl(testDir->url().toString() + "/a.jpg")});
    view->updateViewState();
    view->renameSelectedItems();
    QVERIFY(view->m_view->m_editingRole);
    QTest::keyClick(QApplication::focusWidget(), Qt::Key_B);
    QTest::keyClick(QApplication::focusWidget(), Qt::Key_Enter);
    QVERIFY(itemsChangedSpy.wait()); // Make sure that rename worked

    // Check that preview gets updated and filename is correct
    QVERIFY(previewUpdatedSpy.wait());
    QVERIFY(!view->m_view->m_editingRole);
    QCOMPARE(view->m_model->fileItem(0).name(), "b.jpg");
    QCOMPARE(view->m_model->count(), 1);
}

void DolphinMainWindowTest::testViewModeAfterDynamicView()
{
    GeneralSettings *settings = GeneralSettings::self();
    settings->setGlobalViewProps(true);
    settings->setDynamicView(true);
    settings->save();

    // prepare test data
    QScopedPointer<TestDir> testDir{new TestDir()};
    QString testDirUrl(QDir::cleanPath(testDir->url().toString()));
    testDir->createDir("a");
    QImage testImage(256, 256, QImage::Format_Mono);
    testImage.setColorCount(1);
    testImage.setColor(0, qRgba(255, 0, 0, 255)); // Index #0 = Red
    for (short x = 0; x < 256; ++x) {
        for (short y = 0; y < 256; ++y) {
            testImage.setPixel(x, y, 0);
        }
    }
    testImage.save(testDir->url().path() + "/a/1.jpg");

    // open test dir and set default view mode to "Details"
    m_mainWindow->openDirectories({testDirUrl}, false);
    DolphinView *view = m_mainWindow->activeViewContainer()->view();
    QSignalSpy viewDirectoryLoadingCompletedSpy(view, &DolphinView::directoryLoadingCompleted);
    QSignalSpy modelDirectoryLoadingCompletedSpy(view->m_model, &KFileItemModel::directoryLoadingCompleted);
    m_mainWindow->show();
    QVERIFY(viewDirectoryLoadingCompletedSpy.wait());
    QVERIFY(QTest::qWaitForWindowExposed(m_mainWindow.data()));
    QVERIFY(m_mainWindow->isVisible());
    m_mainWindow->actionCollection()->action(QStringLiteral("details"))->trigger();
    QCOMPARE(view->m_mode, DolphinView::DetailsView);

    // move to child folder and check that dynamic view changed view mode to icons
    m_mainWindow->openFiles({testDirUrl + "/a"}, false);
    view->m_model->loadDirectory(QUrl(testDirUrl + "/a"));
    view->setUrl(QUrl(testDirUrl + "/a"));
    QVERIFY(viewDirectoryLoadingCompletedSpy.wait());
    QTRY_COMPARE_WITH_TIMEOUT(view->m_mode, DolphinView::IconsView, 200);

    // go back to parent folder and check that view mode reverted to details
    m_mainWindow->actionCollection()->action(KStandardAction::name(KStandardAction::Back))->trigger();
    view->m_model->loadDirectory(testDir->url());
    view->setUrl(testDir->url());
    QVERIFY(viewDirectoryLoadingCompletedSpy.wait());
    QTRY_COMPARE_WITH_TIMEOUT(view->m_mode, DolphinView::DetailsView, 200);

    // test for local views
    settings->setGlobalViewProps(false);
    settings->save();

    // go to child folder and check DynamicViewPassed key in view properties as well as view mode
    m_mainWindow->openFiles({testDirUrl + "/a"}, false);
    view->m_model->loadDirectory(QUrl(testDirUrl + "/a"));
    view->setUrl(QUrl(testDirUrl + "/a"));
    QVERIFY(viewDirectoryLoadingCompletedSpy.wait());
    QTRY_COMPARE_WITH_TIMEOUT(view->m_mode, DolphinView::IconsView, 100);
    QTRY_VERIFY_WITH_TIMEOUT(ViewProperties(view->viewPropertiesUrl()).dynamicViewPassed(), 200);

    // change view mode of child folder to "Details"
    m_mainWindow->actionCollection()->action(QStringLiteral("details"))->trigger();
    QCOMPARE(view->m_mode, DolphinView::DetailsView);

    // go back to parent folder
    m_mainWindow->actionCollection()->action(KStandardAction::name(KStandardAction::Back))->trigger();
    view->m_model->loadDirectory(testDir->url());
    view->setUrl(testDir->url());
    QVERIFY(modelDirectoryLoadingCompletedSpy.wait());
    QCOMPARE(view->m_mode, DolphinView::DetailsView);
    QVERIFY(!ViewProperties(view->viewPropertiesUrl()).dynamicViewPassed());

    // store parent current zoom level
    const int parentZoomLevel = view->zoomLevel();

    // go to child folder and make sure view mode change to "Details" is permanent
    m_mainWindow->openFiles({testDirUrl + "/a"}, false);
    view->m_model->loadDirectory(QUrl(testDirUrl + "/a"));
    view->setUrl(QUrl(testDirUrl + "/a"));
    QVERIFY(modelDirectoryLoadingCompletedSpy.wait());
    QCOMPARE(view->m_mode, DolphinView::DetailsView);
    QVERIFY(ViewProperties(view->viewPropertiesUrl()).dynamicViewPassed());

    // still on child, change view zoom level
    const int childZoomLevel = view->zoomLevel() + 2;
    view->setZoomLevel(childZoomLevel);

    // go back to parent folder and check for zoom level
    m_mainWindow->actionCollection()->action(KStandardAction::name(KStandardAction::Back))->trigger();
    view->m_model->loadDirectory(testDir->url());
    view->setUrl(testDir->url());
    QVERIFY(modelDirectoryLoadingCompletedSpy.wait());
    QCOMPARE(view->zoomLevel(), parentZoomLevel);
    QVERIFY(!ViewProperties(view->viewPropertiesUrl()).dynamicViewPassed());

    // go to child and check if zoom level is permanent
    m_mainWindow->openFiles({testDirUrl + "/a"}, false);
    view->m_model->loadDirectory(QUrl(testDirUrl + "/a"));
    view->setUrl(QUrl(testDirUrl + "/a"));
    QVERIFY(modelDirectoryLoadingCompletedSpy.wait());
    QCOMPARE(view->zoomLevel(), childZoomLevel);
    QVERIFY(ViewProperties(view->viewPropertiesUrl()).dynamicViewPassed());

    // test for global views
    settings->setGlobalViewProps(true);
    settings->save();
    QVERIFY(GeneralSettings::globalViewProps());

    // go back to parent folder and set zoom level
    m_mainWindow->actionCollection()->action(KStandardAction::name(KStandardAction::Back))->trigger();
    view->m_model->loadDirectory(testDir->url());
    view->setUrl(testDir->url());
    QVERIFY(modelDirectoryLoadingCompletedSpy.wait());

    // zoom isn't changed
    QCOMPARE(view->zoomLevel(), childZoomLevel);

    // change the zoom
    view->setZoomLevel(parentZoomLevel + 1);
    QCOMPARE(view->zoomLevel(), parentZoomLevel + 1);
    QVERIFY(!ViewProperties(view->viewPropertiesUrl()).dynamicViewPassed());

    // go to child and check if zoom level remains the same
    m_mainWindow->openFiles({testDirUrl + "/a"}, false);
    view->m_model->loadDirectory(QUrl(testDirUrl + "/a"));
    view->setUrl(QUrl(testDirUrl + "/a"));
    QVERIFY(modelDirectoryLoadingCompletedSpy.wait());

    ViewModeSettings modeDefaultSettings{DolphinView::IconsView};
    auto defaultPreviewIconSize = modeDefaultSettings.previewSize();
    auto defaultPreviewZoom = ZoomLevelInfo::zoomLevelForIconSize(QSize(defaultPreviewIconSize, defaultPreviewIconSize));
    // dynamic view works
    QCOMPARE(view->m_mode, DolphinView::IconsView);
    QCOMPARE(view->zoomLevel(), defaultPreviewZoom);
    // that's the global settings, no dynamicViewPassed saved
    QVERIFY(!ViewProperties(view->viewPropertiesUrl()).dynamicViewPassed());
}

void DolphinMainWindowTest::testActivationAndTabTitleAfterRenameOpeningFolder()
{
    QScopedPointer<TestDir> testDir{new TestDir()};
    testDir->createDir("a");
    const QUrl parentDirUrl = QUrl::fromLocalFile(testDir->url().toLocalFile());
    const QUrl childDirUrl = QUrl::fromLocalFile(testDir->url().toLocalFile() + "/a");

    auto tabWidget = m_mainWindow->findChild<DolphinTabWidget *>("tabWidget");
    QVERIFY(tabWidget);

    // Tab 0: Open childDirUrl
    m_mainWindow->openDirectories({childDirUrl}, false);
    m_mainWindow->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_mainWindow.data()));
    QVERIFY(m_mainWindow->isVisible());

    // Tab 0: Enable split view
    m_mainWindow->actionCollection()->action(QStringLiteral("split_view"))->setChecked(true);
    m_mainWindow->actionCollection()->action(QStringLiteral("split_view"))->trigger();
    QVERIFY(tabWidget->currentTabPage()->splitViewEnabled());

    // Tab 1: Open childDirUrl
    tabWidget->openNewActivatedTab(childDirUrl);

    // Tab 1: Open parentDirUrl in right view
    m_mainWindow->actionCollection()->action(QStringLiteral("split_view"))->setChecked(true);
    m_mainWindow->actionCollection()->action(QStringLiteral("split_view"))->trigger();
    QVERIFY(tabWidget->currentTabPage()->splitViewEnabled());

    DolphinView *view = m_mainWindow->activeViewContainer()->view();
    view->m_model->loadDirectory(parentDirUrl);
    view->setUrl(parentDirUrl);

    // Check current view is right view
    QVERIFY(tabWidget->currentTabPage()->secondaryViewContainer()->isActive());

    // Check all tab titles are correct
    // Tab 0: (a) | a
    // Tab 1: (a) | parentDir
    const QString parentDirName = QFileInfo(parentDirUrl.toString()).fileName();
    const QString childDirName = QFileInfo(childDirUrl.toString()).fileName();
    const QString expectedTab0Title = QStringLiteral("(%1) | %2").arg(childDirName, childDirName);
    const QString expectedTab1Title = QStringLiteral("(%1) | %2").arg(childDirName, parentDirName);
    QCOMPARE(tabWidget->tabText(0), expectedTab0Title);
    QCOMPARE(tabWidget->tabText(1), expectedTab1Title);

    // Prepare signal spies
    QSignalSpy viewDirectoryLoadingCompletedSpy(view, &DolphinView::directoryLoadingCompleted);
    QSignalSpy itemsChangedSpy(view->m_model, &KFileItemModel::itemsChanged);

    QVERIFY(viewDirectoryLoadingCompletedSpy.wait());

    // Rename child dir to "b"
    view->markUrlsAsSelected({childDirUrl});
    view->updateViewState();
    view->renameSelectedItems(); // Rename inline

    QTest::keyClick(QApplication::focusWidget(), Qt::Key_B);
    QTest::keyClick(QApplication::focusWidget(), Qt::Key_Enter);
    QVERIFY(itemsChangedSpy.wait()); // Make sure that rename worked
    QVERIFY(viewDirectoryLoadingCompletedSpy.wait()); // and the directory has finished loading

    // Check current view is right view
    QVERIFY(tabWidget->currentTabPage()->secondaryViewContainer()->isActive());

    // Check navigator in left view is inactive
    auto leftViewNavigator = tabWidget->currentTabPage()->primaryViewContainer()->urlNavigator();
    QVERIFY(!leftViewNavigator->isActive());

    // Check all tab titles are correct after rename
    // Tab 0: (b) | b
    // Tab 1: (b) | parentDir
    const QString newChildDirName = QStringLiteral("b");
    const QString expectedNewTab0Title = QStringLiteral("(%1) | %2").arg(newChildDirName, newChildDirName);
    const QString expectedNewTab1Title = QStringLiteral("(%1) | %2").arg(newChildDirName, parentDirName);
    QCOMPARE(tabWidget->tabText(0), expectedNewTab0Title);
    QCOMPARE(tabWidget->tabText(1), expectedNewTab1Title);
}

// Test that switching tabs does not spuriously toggle which split-view pane is active.
// Regression test for the bug where DolphinTabPage::setActive(true) during tab switch
// caused DolphinView::activated() to reach slotViewActivated(), which toggled
// m_primaryViewActive and connected MainWindow signals to the wrong view container.
void DolphinMainWindowTest::testActiveViewAfterTabSwitchWithSplitView()
{
    m_mainWindow->openDirectories({QUrl::fromLocalFile(QDir::homePath())}, false);
    m_mainWindow->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_mainWindow.data()));
    QVERIFY(m_mainWindow->isVisible());

    auto tabWidget = m_mainWindow->findChild<DolphinTabWidget *>("tabWidget");
    QVERIFY(tabWidget);

    // Enable split view on the first tab. After this, the secondary (right) pane
    // becomes active via slotViewActivated(), so primaryViewActive() is false.
    m_mainWindow->actionCollection()->action(QStringLiteral("split_view"))->trigger();
    QVERIFY(tabWidget->currentTabPage()->splitViewEnabled());
    QVERIFY(!tabWidget->currentTabPage()->primaryViewActive());
    auto firstTabPage = tabWidget->currentTabPage();
    auto firstTabSecondary = firstTabPage->secondaryViewContainer();
    QVERIFY(firstTabSecondary->isActive());

    // Open a second tab and switch to it.
    tabWidget->openNewActivatedTab(QUrl::fromLocalFile(QDir::homePath()));
    QCOMPARE(tabWidget->count(), 2);
    QCOMPARE(tabWidget->currentIndex(), 1);

    // Spy on activeViewChanged to count emissions during the tab switch back.
    QSignalSpy activeViewChangedSpy(tabWidget, &DolphinTabWidget::activeViewChanged);

    // Switch back to the first tab.
    tabWidget->setCurrentIndex(0);
    QCOMPARE(tabWidget->currentTabPage(), firstTabPage);

    // activeViewChanged must be emitted exactly once — by currentTabChanged itself.
    // A spurious second emission would indicate slotViewActivated() fired during
    // the programmatic setActive(true) and toggled m_primaryViewActive.
    QCOMPARE(activeViewChangedSpy.count(), 1);

    // The secondary pane must still be the designated active one.
    QVERIFY(!firstTabPage->primaryViewActive());
    QCOMPARE(firstTabPage->activeViewContainer(), firstTabSecondary);
    QVERIFY(firstTabSecondary->isActive());
    QVERIFY(!firstTabPage->primaryViewContainer()->isActive());
}

void DolphinMainWindowTest::cleanupTestCase()
{
    m_mainWindow->showNormal();
    m_mainWindow->actionCollection()->action(QStringLiteral("split_view"))->setChecked(false); // disable split view (starts animation)

#if HAVE_BALOO
    m_mainWindow->actionCollection()->action(QStringLiteral("show_information_panel"))->setChecked(false); // hide panel
#endif

#if HAVE_TERMINAL
    m_mainWindow->actionCollection()->action(QStringLiteral("show_terminal_panel"))->setChecked(false); // hide panel
#endif

    // Quit Dolphin to save the hiding of panels and make sure that normal Quit doesn't crash.
    m_mainWindow->actionCollection()->action(KStandardAction::name(KStandardAction::Quit))->trigger();
}

int main(int argc, char **argv)
{
    QTemporaryDir profile;
    if (!profile.isValid())
        return 2;
    qputenv("XDG_CONFIG_HOME", (profile.path() + "/config").toUtf8());
    qputenv("XDG_DATA_HOME", (profile.path() + "/data").toUtf8());
    qputenv("XDG_CACHE_HOME", (profile.path() + "/cache").toUtf8());
    QApplication app(argc, argv);
    QJsonArray libraries;
    for (const auto &name : {QStringLiteral("Documents"), QStringLiteral("Music"),
                             QStringLiteral("Pictures"), QStringLiteral("Videos")}) {
        const QString folder = profile.path() + "/folders/" + name;
        if (!QDir().mkpath(folder))
            return 2;
        libraries.append(QJsonObject{{"id", name.toLower()}, {"name", name},
                                     {"locations", QJsonArray{folder}}, {"saveLocation", folder}});
    }
    QDir().mkpath(profile.path() + "/config/aero7");
    QFile config(profile.path() + "/config/aero7/libraries.json");
    if (!config.open(QIODevice::WriteOnly)
        || config.write(QJsonDocument(QJsonObject{{"libraries", libraries}}).toJson()) < 0)
        return 2;
    config.close();
    DolphinMainWindowTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "dolphinmainwindowtest.moc"
