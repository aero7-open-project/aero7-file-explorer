/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2020 Felix Ernst <felixernst@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "dolphinurlnavigator.h"
#include "aero7icons.h"

#include "dolphin_generalsettings.h"
#include "dolphinplacesmodelsingleton.h"
#include "dolphinurlnavigatorscontroller.h"
#include "global.h"
#include "aero7/aero7libraries.h"

#include <KLocalizedString>
#include <KUrlComboBox>

#include <QAbstractButton>
#include <QBoxLayout>
#include <QDir>
#include <QFileInfo>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMouseEvent>
#include <QTimer>

DolphinUrlNavigator::DolphinUrlNavigator(QWidget *parent)
    : DolphinUrlNavigator(QUrl(), parent)
{
}

DolphinUrlNavigator::DolphinUrlNavigator(const QUrl &url, QWidget *parent)
    : KUrlNavigator(DolphinPlacesModelSingleton::instance().placesModel(), url, parent)
{
    const GeneralSettings *settings = GeneralSettings::self();
    // Aero7 exposes a fixed Windows 7 breadcrumb. The upstream editable URL
    // mode (including click-to-edit) is intentionally unavailable.
    setUrlEditable(false);
    // Match Windows Explorer: the breadcrumb starts at the user profile and
    // never exposes Linux implementation paths such as /home/aero.
    setShowFullPath(true);
    setHomeUrl(QUrl::fromLocalFile(
        QFileInfo(Dolphin::homeUrl().toLocalFile()).absolutePath()));
    setPlacesSelectorVisible(DolphinUrlNavigatorsController::placesSelectorVisible());
    editor()->setCompletionMode(KCompletion::CompletionMode(settings->urlCompletionMode()));
    setWhatsThis(xi18nc("@info:whatsthis location bar",
                        "<para>This describes the location of the files and folders "
                        "displayed below.</para><para>The name of the currently viewed "
                        "folder can be read at the very right. To the left of it is the "
                        "name of the folder that contains it. The whole line is called "
                        "the <emphasis>path</emphasis> to the current location because "
                        "following these folders from left to right leads here.</para>"
                        "<para>This interactive path "
                        "is more powerful than one would expect. To learn more "
                        "about the basic and advanced features of the location bar "
                        "<link url='help:/dolphin/location-bar.html'>click here</link>. "
                        "This will open the dedicated page in the Handbook.</para>"));

    DolphinUrlNavigatorsController::registerDolphinUrlNavigator(this);

    connect(this, &KUrlNavigator::returnPressed, this, &DolphinUrlNavigator::slotReturnPressed);

    // KUrlNavigator normally collapses everything at the Home place, leaving
    // only "Downloads" visible. Windows 7 keeps the user profile crumb. Build
    // the full path, then reduce the Linux-only prefix to a folder glyph so the
    // visible path is "aero > Downloads".
    connect(this, &KUrlNavigator::urlChanged, this, &DolphinUrlNavigator::updateAero7Breadcrumbs);
    connect(this, &KUrlNavigator::editableStateChanged, this, [this](bool editable) {
        if (editable) {
            QTimer::singleShot(0, this, [this]() {
                setUrlEditable(false);
                updateAero7Breadcrumbs();
            });
        }
    });
    updateAero7Breadcrumbs();

    auto readOnlyBadge = new QLabel();
    readOnlyBadge->setPixmap(Aero7Icons::icon(QStringLiteral("emblem-readonly")).pixmap(12, 12));
    readOnlyBadge->setToolTip(i18nc("@info:tooltip of a 'locked' symbol in url navigator", "This folder is not writable for you."));
    readOnlyBadge->hide();
    setBadgeWidget(readOnlyBadge);
}

void DolphinUrlNavigator::updateAero7Breadcrumbs()
{
    const auto applyBreadcrumbs = [this]() {
        QList<QAbstractButton *> crumbs;
        const auto hideBreadcrumbPart = [this](QAbstractButton *button) {
            button->installEventFilter(this);
            button->setProperty("aero7LinuxPrefix", true);
            button->setMinimumWidth(0);
            button->setMaximumWidth(0);
            button->hide();
        };
        const auto anchorBreadcrumbsAtLeft = [this]() {
            // KF6 uses expanding spacer items to distribute breadcrumb buttons
            // over spare width. Windows Explorer keeps them contiguous at the
            // left edge and puts all unused space after the final crumb.
            if (auto *navigatorLayout = qobject_cast<QBoxLayout *>(layout())) {
                for (int index = 0; index < navigatorLayout->count(); ++index) {
                    if (QSpacerItem *spacer = navigatorLayout->itemAt(index)->spacerItem()) {
                        const bool trailingSpacer = index == navigatorLayout->count() - 1;
                        spacer->changeSize(0,
                                           0,
                                           trailingSpacer ? QSizePolicy::Expanding : QSizePolicy::Fixed,
                                           QSizePolicy::Minimum);
                    }
                }
            }
            layout()->invalidate();
            layout()->activate();
        };
        for (QAbstractButton *button : findChildren<QAbstractButton *>()) {
            const QString className = QString::fromLatin1(button->metaObject()->className());
            if (className.contains(QLatin1String("UrlNavigatorToggleButton"))) {
                hideBreadcrumbPart(button);
                button->setEnabled(false);
                continue;
            }
            if (!className.endsWith(QLatin1String("KUrlNavigatorButton"))) {
                continue;
            }
            crumbs.append(button);
        }

        QString virtualLabel;
        if (locationUrl().scheme() == QLatin1String("aero7computer"))
            virtualLabel = QStringLiteral("Computer");
        else if (locationUrl().scheme() == QLatin1String("trash"))
            virtualLabel = QStringLiteral("Recycle Bin");
        else if (locationUrl().scheme() == QLatin1String("network"))
            virtualLabel = QStringLiteral("Network");
        if (!virtualLabel.isEmpty() && !crumbs.isEmpty()) {
            for (qsizetype index = 0; index + 1 < crumbs.size(); ++index)
                hideBreadcrumbPart(crumbs.at(index));
            QAbstractButton *button = crumbs.constLast();
            button->setProperty("aero7LinuxPrefix", false);
            const int width = button->fontMetrics().horizontalAdvance(virtualLabel) + 24;
            button->setFixedWidth(width);
            button->setText(virtualLabel);
            button->show();
            anchorBreadcrumbsAtLeft();
            return;
        }

        // Libraries are materialized below
        // ~/.local/share/Aero7/Libraries so the normal KUrlNavigator path is
        // an implementation detail. Windows 7 exposes that location as the
        // virtual hierarchy "Libraries > Documents" (and then any child
        // folders), never as a Linux/XDG path.
        if (locationUrl().isLocalFile() && !crumbs.isEmpty()) {
            const Aero7Libraries &libraries = Aero7Libraries::instance();
            const QString localPath = QDir::cleanPath(locationUrl().toLocalFile());
            const QString librariesRoot = QDir::cleanPath(libraries.materializedRoot());
            QStringList visibleLabels;
            if (localPath == librariesRoot) {
                visibleLabels.append(QStringLiteral("Libraries"));
            } else {
                const QString libraryId = libraries.libraryIdForPath(localPath);
                if (!libraryId.isEmpty()) {
                    const Aero7Library library = libraries.library(libraryId);
                    visibleLabels << QStringLiteral("Libraries") << library.name;
                    const QString libraryRoot = QDir::cleanPath(libraries.materializedPath(libraryId));
                    const QString relativePath = QDir(libraryRoot).relativeFilePath(localPath);
                    if (relativePath != QLatin1String(".")) {
                        const QStringList childParts = relativePath.split(QDir::separator(), Qt::SkipEmptyParts);
                        visibleLabels.append(childParts);
                    }
                }
            }

            if (!visibleLabels.isEmpty() && visibleLabels.size() <= crumbs.size()) {
                const qsizetype firstVisible = crumbs.size() - visibleLabels.size();
                for (qsizetype index = 0; index < firstVisible; ++index)
                    hideBreadcrumbPart(crumbs.at(index));
                for (qsizetype index = 0; index < visibleLabels.size(); ++index) {
                    QAbstractButton *button = crumbs.at(firstVisible + index);
                    const QString &label = visibleLabels.at(index);
                    button->setProperty("aero7LinuxPrefix", false);
                    button->setFixedWidth(button->fontMetrics().horizontalAdvance(label) + 24);
                    button->setText(label);
                    button->show();
                }
                anchorBreadcrumbsAtLeft();
                return;
            }
        }

        for (QAbstractButton *button : crumbs) {
            button->installEventFilter(this);
            QString label = button->text();
            label.remove(QLatin1Char('&'));
            const bool linuxPrefix = label == QLatin1String("home")
                || label == QLatin1String("/");
            button->setProperty("aero7LinuxPrefix", linuxPrefix);
            if (linuxPrefix) {
                hideBreadcrumbPart(button);
            } else {
                const int width = button->fontMetrics().horizontalAdvance(label) + 24;
                button->setFixedWidth(width);
                button->show();
            }
        }
        anchorBreadcrumbsAtLeft();
    };
    // KUrlNavigator may finish relabelling its reused buttons after emitting
    // urlChanged. Apply once in the next event turn and once after that
    // internal refresh so navigating from a library child back to the
    // Libraries root cannot erase the virtual label.
    QTimer::singleShot(0, this, applyBreadcrumbs);
    QTimer::singleShot(75, this, applyBreadcrumbs);
}

bool DolphinUrlNavigator::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Show) {
        if (auto *button = qobject_cast<QAbstractButton *>(watched);
            button && button->property("aero7LinuxPrefix").toBool()) {
            QTimer::singleShot(0, button, [button]() {
                button->setMinimumWidth(0);
                button->setMaximumWidth(0);
                button->hide();
            });
        }
    }
    return KUrlNavigator::eventFilter(watched, event);
}

void DolphinUrlNavigator::mousePressEvent(QMouseEvent *event)
{
    // Blank breadcrumb space is inert in Aero7; KUrlNavigator otherwise uses
    // it as a hidden entry point into its editable URL field.
    event->accept();
}

void DolphinUrlNavigator::mouseReleaseEvent(QMouseEvent *event)
{
    event->accept();
}

void DolphinUrlNavigator::mouseDoubleClickEvent(QMouseEvent *event)
{
    event->accept();
}

DolphinUrlNavigator::~DolphinUrlNavigator()
{
    DolphinUrlNavigatorsController::unregisterDolphinUrlNavigator(this);
}

QSize DolphinUrlNavigator::sizeHint() const
{
    if (isUrlEditable()) {
        return editor()->lineEdit()->sizeHint();
    }
    int widthHint = 0;
    for (int i = 0; i < layout()->count(); ++i) {
        QWidget *widget = layout()->itemAt(i)->widget();
        const QAbstractButton *button = qobject_cast<QAbstractButton *>(widget);
        if (button && button->icon().isNull()) {
            widthHint += widget->minimumSizeHint().width();
        }
    }
    if (readOnlyBadgeVisible()) {
        widthHint += badgeWidget()->sizeHint().width();
    }
    return QSize(widthHint, KUrlNavigator::sizeHint().height());
}

std::unique_ptr<DolphinUrlNavigator::VisualState> DolphinUrlNavigator::visualState() const
{
    std::unique_ptr<VisualState> visualState{new VisualState};
    visualState->isUrlEditable = (isUrlEditable());
    const QLineEdit *lineEdit = editor()->lineEdit();
    visualState->hasFocus = lineEdit->hasFocus();
    visualState->text = lineEdit->text();
    visualState->cursorPosition = lineEdit->cursorPosition();
    visualState->selectionStart = lineEdit->selectionStart();
    visualState->selectionLength = lineEdit->selectionLength();
    return visualState;
}

void DolphinUrlNavigator::setVisualState(const VisualState &visualState)
{
    Q_UNUSED(visualState)
    setUrlEditable(false);
}

void DolphinUrlNavigator::clearText() const
{
    editor()->lineEdit()->clear();
}

void DolphinUrlNavigator::setPlaceholderText(const QString &text)
{
    editor()->lineEdit()->setPlaceholderText(text);
}

void DolphinUrlNavigator::setReadOnlyBadgeVisible(bool visible)
{
    QWidget *readOnlyBadge = badgeWidget();
    if (readOnlyBadge) {
        readOnlyBadge->setVisible(visible);
    }
}

bool DolphinUrlNavigator::readOnlyBadgeVisible() const
{
    QWidget *readOnlyBadge = badgeWidget();
    if (readOnlyBadge) {
        return readOnlyBadge->isVisible();
    }
    return false;
}

void DolphinUrlNavigator::slotReturnPressed()
{
    setUrlEditable(false);
}

void DolphinUrlNavigator::keyPressEvent(QKeyEvent *keyEvent)
{
    if (keyEvent->key() == Qt::Key_Escape && !isUrlEditable()) {
        Q_EMIT requestToLoseFocus();
        return;
    }
    KUrlNavigator::keyPressEvent(keyEvent);
}

#include "moc_dolphinurlnavigator.cpp"
