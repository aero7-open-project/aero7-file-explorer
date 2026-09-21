/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "aero7libraries.h"
#include "aero7storage.h"
#include <QHBoxLayout>
#include <QFileInfo>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QResizeEvent>
#include <QScrollArea>
#include <QShortcut>
#include <QStackedLayout>
#include <QTimer>
#include <QToolButton>
#include <functional>
#include <utility>

// Presentation only: every crumb retains its real filesystem destination.
// Kept private so the exported common-dialog class layout does not change.
namespace Aero7DialogLocation
{
struct Crumb { QString label; QString path; };

inline QList<Crumb> crumbs(const QString &input, const QList<Aero7Storage::Entry> &mounts)
{
    const QString path = QDir::cleanPath(input);
    auto &libraries = Aero7Libraries::instance();
    const QString libraryRoot = QDir::cleanPath(libraries.materializedRoot());
    QList<Crumb> result;
    QString root;
    if (path == libraryRoot || path.startsWith(libraryRoot + '/')) {
        root = libraryRoot;
        result.append({QStringLiteral("Libraries"), root});
        const QString id = libraries.libraryIdForPath(path);
        if (!id.isEmpty()) {
            root = QDir::cleanPath(libraries.materializedPath(id));
            result.append({libraries.library(id).name, root});
        }
    } else {
        for (const auto &mount : mounts) {
            if (mount.root == QLatin1String("/") || mount.root.isEmpty()) continue;
            if ((path == mount.root || path.startsWith(mount.root + '/')) && mount.root.size() > root.size()) {
                root = mount.root;
                result = {{mount.name, root}};
            }
        }
        if (root.isEmpty()) {
            const QString home = QDir::homePath();
            if (path == home || path.startsWith(home + '/')) {
                root = home;
                result.append({QFileInfo(home).fileName(), root});
            } else {
                root = QStringLiteral("/");
                result.append({QStringLiteral("Local Disk (C:)"), root});
            }
        }
    }
    const QString relative = QDir(root).relativeFilePath(path);
    if (relative != QLatin1String(".")) {
        for (const QString &part : relative.split('/', Qt::SkipEmptyParts)) {
            root = QDir(root).filePath(part);
            result.append({part, root});
        }
    }
    return result;
}

class Bar final : public QWidget
{
public:
    Bar(QLineEdit *edit, QWidget *parent, std::function<void(const QString &)> navigate)
        : QWidget(parent), m_edit(edit), m_navigate(std::move(navigate))
    {
        setObjectName(QStringLiteral("locationBreadcrumbs"));
        setMinimumWidth(0);
        m_stack = new QStackedLayout(this);
        m_stack->setContentsMargins(0, 0, 0, 0);
        m_scroll = new QScrollArea;
        m_scroll->setWidgetResizable(true);
        m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_scroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_breadcrumbs = new QWidget;
        auto *strip = new QHBoxLayout(m_breadcrumbs);
        strip->setContentsMargins(0, 0, 0, 0);
        strip->setSpacing(0);
        m_ancestors = new QToolButton(m_breadcrumbs);
        m_ancestors->setObjectName(QStringLiteral("locationAncestors"));
        m_ancestors->setText(QStringLiteral("\u00ab"));
        m_ancestors->setAccessibleName(QStringLiteral("Parent folders"));
        m_ancestors->setToolTip(QStringLiteral("Parent folders"));
        m_ancestors->setPopupMode(QToolButton::InstantPopup);
        auto *ancestors = new QMenu(m_ancestors);
        m_ancestors->setMenu(ancestors);
        QObject::connect(ancestors, &QMenu::aboutToShow, this, [this, ancestors] {
            ancestors->clear();
            // Keep every ancestor reachable even when it has scrolled out of
            // sight. Use the captured real destinations, not elided labels.
            for (qsizetype index = 0; index + 1 < m_buttons.size(); ++index) {
                const auto *button = m_buttons.at(index);
                const QString target = button->property("destination").toString();
                auto *action = ancestors->addAction(escaped(button->accessibleName()));
                action->setData(target);
                action->setToolTip(target);
                QObject::connect(action, &QAction::triggered, this, [this, target] { m_navigate(target); });
            }
        });
        m_ancestors->hide();
        strip->addWidget(m_ancestors);
        strip->addWidget(m_scroll, 1);
        m_stack->addWidget(m_breadcrumbs);
        m_stack->addWidget(edit);
        auto *shortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+L")), parent);
        QObject::connect(shortcut, &QShortcut::activated, this, [this] {
            m_stack->setCurrentWidget(m_edit);
            m_edit->setFocus();
            m_edit->selectAll();
        });
        edit->installEventFilter(this);
    }

    void setLocation(const QString &path)
    {
        m_path = path;
        m_edit->setText(path);
        m_stack->setCurrentWidget(m_breadcrumbs);
        m_buttons.clear();
        if (auto *old = m_scroll->takeWidget()) old->deleteLater();
        auto *row = new QWidget;
        auto *layout = new QHBoxLayout(row);
        layout->setContentsMargins(1, 0, 1, 0);
        layout->setSpacing(0);
        for (const auto &crumb : crumbs(path, Aero7Storage::mounted())) {
            auto *button = new QToolButton(row);
            button->setText(QString(crumb.label).replace('&', QStringLiteral("&&")));
            button->setAccessibleName(crumb.label);
            button->setToolTip(crumb.label);
            button->setProperty("destination", crumb.path);
            button->setAutoRaise(true);
            button->setPopupMode(QToolButton::MenuButtonPopup);
            QObject::connect(button, &QToolButton::clicked, this, [this, target = crumb.path] { m_navigate(target); });
            auto *menu = new QMenu(button);
            button->setMenu(menu);
            QObject::connect(menu, &QMenu::aboutToShow, this, [this, menu, target = crumb.path] {
                menu->clear();
                const auto folders = QDir(target).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase);
                for (const auto &folder : folders) {
                    auto *action = menu->addAction(QString(folder.fileName()).replace('&', QStringLiteral("&&")));
                    QObject::connect(action, &QAction::triggered, this, [this, destination = folder.absoluteFilePath()] { m_navigate(destination); });
                }
                if (folders.isEmpty()) menu->addAction(QStringLiteral("No subfolders"))->setEnabled(false);
            });
            layout->addWidget(button);
            m_buttons.append(button);
        }
        layout->addStretch();
        m_scroll->setWidget(row);
        QTimer::singleShot(0, row, [this] { fitContents(); });
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QWidget::resizeEvent(event);
        QTimer::singleShot(0, this, [this] { fitContents(); });
    }

    void changeEvent(QEvent *event) override
    {
        QWidget::changeEvent(event);
        if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange)
            QTimer::singleShot(0, this, [this] { fitContents(); });
    }

    bool eventFilter(QObject *object, QEvent *event) override
    {
        if (object == m_edit && event->type() == QEvent::KeyPress
            && static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) {
            m_edit->setText(m_path);
            m_stack->setCurrentWidget(m_breadcrumbs);
            return true; // Escape leaves address editing, not the entire dialog.
        }
        return QWidget::eventFilter(object, event);
    }

private:
    static QString escaped(QString label) { return label.replace('&', QStringLiteral("&&")); }

    void fitContents()
    {
        if (m_buttons.isEmpty()) return;
        int naturalWidth = 2;
        int height = 0;
        for (auto *button : std::as_const(m_buttons)) {
            button->setText(escaped(button->accessibleName()));
            button->setMaximumWidth(QWIDGETSIZE_MAX);
            naturalWidth += button->sizeHint().width();
            height = qMax(height, button->sizeHint().height());
        }
        m_scroll->setFixedHeight(qMax(27, height + 2 * m_scroll->frameWidth()));
        m_ancestors->setVisible(m_buttons.size() > 1 && naturalWidth > m_breadcrumbs->width());
        m_breadcrumbs->layout()->activate();
        const int available = qMax(1, m_scroll->viewport()->width() - 2);
        for (auto *button : std::as_const(m_buttons)) {
            const QString label = button->accessibleName();
            // Retain enough space for the style's dropdown and button margins.
            const int chrome = button->sizeHint().width() - button->fontMetrics().horizontalAdvance(label);
            button->setText(escaped(button->fontMetrics().elidedText(label, Qt::ElideMiddle,
                qMax(1, available - chrome))));
            button->setMaximumWidth(available);
        }
        // Show a contiguous suffix of whole buttons. Scrolling to the last
        // component alone can leave an unusable sliver of its predecessor.
        int remaining = available;
        bool fits = true;
        for (qsizetype index = m_buttons.size(); index-- > 0;) {
            auto *button = m_buttons.at(index);
            const int width = qMin(available, button->sizeHint().width());
            fits = fits && width <= remaining;
            button->setVisible(fits);
            if (fits) remaining -= width;
        }
        m_scroll->widget()->layout()->activate();
        m_scroll->ensureWidgetVisible(m_buttons.constLast(), 0, 0);
    }

    QLineEdit *m_edit;
    QScrollArea *m_scroll;
    QWidget *m_breadcrumbs;
    QToolButton *m_ancestors;
    QList<QToolButton *> m_buttons;
    QStackedLayout *m_stack;
    QString m_path;
    std::function<void(const QString &)> m_navigate;
};
}
