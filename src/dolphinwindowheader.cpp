#include "dolphinwindowheader.h"

#include <QHBoxLayout>
#include <QIcon>
#include <QLayoutItem>
#include <QLabel>
#include <QProcess>
#include <QToolButton>
#include <QVBoxLayout>

#include <utility>

namespace
{
class Aero7CommandButton final : public QToolButton
{
public:
    using QToolButton::QToolButton;

    QSize minimumSizeHint() const override
    {
        QSize hint = QToolButton::minimumSizeHint();
        hint.setWidth(0);
        return hint;
    }
};
}

DolphinWindowHeader::DolphinWindowHeader(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::DolphinWindowHeader)
{
    ui->setupUi(this);
    setObjectName(QStringLiteral("aero7ExplorerHeader"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(65);

    QLayout *oldLayout = layout();
    QList<QLayoutItem *> navigationItems;
    while (QLayoutItem *item = oldLayout->takeAt(0))
        navigationItems.append(item);
    delete oldLayout;

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 2, 3, 0);
    outer->setSpacing(0);

    auto *navigationBar = new QWidget(this);
    navigationBar->setObjectName(QStringLiteral("aero7NavigationBar"));
    navigationBar->setFixedHeight(28);
    auto *navigation = new QHBoxLayout(navigationBar);
    navigation->setContentsMargins(4, 0, 3, 0);
    navigation->setSpacing(4);
    for (QLayoutItem *item : std::as_const(navigationItems)) {
        if (QWidget *widget = item->widget())
            widget->setParent(navigationBar);
        navigation->addItem(item);
    }
    // Windows Explorer leaves a slightly wider glass gutter before Search.
    // Keeping it as an explicit spacer lets the expanding breadcrumb absorb
    // the difference while Search stays anchored to the right edge.
    navigation->insertSpacing(navigation->indexOf(ui->searchBar), 4);
    outer->addWidget(navigationBar);

    auto *navigationGap = new QWidget(this);
    navigationGap->setObjectName(QStringLiteral("aero7NavigationGap"));
    navigationGap->setFixedHeight(4);
    outer->addWidget(navigationGap);

    auto *commandBar = new QWidget(this);
    commandBar->setObjectName(QStringLiteral("aero7CommandBar"));
    commandBar->setFixedHeight(31);
    auto *commands = new QHBoxLayout(commandBar);
    commands->setContentsMargins(2, 0, 2, 0);
    commands->setSpacing(2);
    auto makeButton = [this, commands](const QString &text) {
        auto *button = new Aero7CommandButton(this);
        button->setProperty("aero7Command", true);
        button->setText(text);
        button->setAutoRaise(true);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setFixedHeight(26);
        // Explorer command bars compress their contents when the display is
        // narrowed; long labels must never become a top-level window minimum.
        // Preferred preserves the natural label width; Aero7CommandButton's
        // zero-width minimum lets QHBoxLayout compress it only when necessary.
        button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        commands->addWidget(button);
        return button;
    };
    m_organize = makeButton(QStringLiteral("Organize"));
    m_include = makeButton(QStringLiteral("Include in library"));
    m_share = makeButton(QStringLiteral("Share with"));
    m_newFolder = makeButton(QStringLiteral("New folder"));
    const auto makeComputerButton = [this, commands](const QString &text,
                                                      const QStringList &command) {
        auto *button = new Aero7CommandButton(this);
        button->setProperty("aero7Command", true);
        button->setText(text);
        button->setAutoRaise(true);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setFixedHeight(26);
        button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        button->hide();
        commands->addWidget(button);
        connect(button, &QToolButton::clicked, this, [command]() {
            if (!command.isEmpty())
                QProcess::startDetached(command.first(), command.mid(1));
        });
        m_computerCommands.append(button);
    };
    makeComputerButton(QStringLiteral("System properties"),
                       {QStringLiteral("control"), QStringLiteral("--page"), QStringLiteral("system")});
    makeComputerButton(QStringLiteral("Uninstall or change a program"),
                       {QStringLiteral("control"), QStringLiteral("--page"), QStringLiteral("programs-features")});
    makeComputerButton(QStringLiteral("Map network drive"),
                       {QStringLiteral("aero7-file-explorer"), QStringLiteral("network:/")});
    makeComputerButton(QStringLiteral("Open Control Panel"),
                       {QStringLiteral("control")});
    const auto makeRecycleBinButton = [this, commands](const QString &text) {
        auto *button = new Aero7CommandButton(this);
        button->setProperty("aero7Command", true);
        button->setText(text);
        button->setAutoRaise(true);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setFixedHeight(26);
        button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        button->hide();
        commands->addWidget(button);
        m_recycleBinCommands.append(button);
        return button;
    };
    m_restoreAll = makeRecycleBinButton(QStringLiteral("Restore all items"));
    m_emptyRecycleBin = makeRecycleBinButton(QStringLiteral("Empty the Recycle Bin"));
    m_recycleBinProperties = makeRecycleBinButton(QStringLiteral("Recycle Bin properties"));
    commands->addStretch(1);
    m_views = makeButton(QString());
    m_views->setProperty("aero7IconCommand", true);
    m_views->setIcon(QIcon::fromTheme(QStringLiteral("view-list-details")));
    m_views->setIconSize(QSize(16, 16));
    m_views->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_views->setToolTip(QStringLiteral("Change your view"));
    m_views->setFixedWidth(50);
    m_preview = makeButton(QString());
    m_preview->setProperty("aero7IconCommand", true);
    m_preview->setCheckable(true);
    m_preview->setIcon(QIcon::fromTheme(QStringLiteral("view-right-new")));
    m_preview->setIconSize(QSize(16, 16));
    m_preview->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_preview->setToolTip(QStringLiteral("Show the preview pane"));
    m_preview->setFixedWidth(38);
    auto *help = makeButton(QString());
    help->setProperty("aero7IconCommand", true);
    help->setIcon(QIcon::fromTheme(QStringLiteral("dialog-question")));
    help->setIconSize(QSize(16, 16));
    help->setToolButtonStyle(Qt::ToolButtonIconOnly);
    help->setToolTip(QStringLiteral("Get help"));
    help->setFixedWidth(30);
    outer->addWidget(commandBar);

    auto *searchIcon = new QLabel(ui->searchBar);
    searchIcon->setObjectName(QStringLiteral("aero7SearchIcon"));
    searchIcon->setFixedSize(22, 22);
    searchIcon->setAlignment(Qt::AlignCenter);
    searchIcon->setAttribute(Qt::WA_TransparentForMouseEvents);
    searchIcon->setPixmap(QIcon::fromTheme(QStringLiteral("edit-find")).pixmap(22, 22));
    auto *searchChrome = new QHBoxLayout(ui->searchBar);
    searchChrome->setContentsMargins(0, 0, 2, 0);
    searchChrome->setSpacing(0);
    searchChrome->addStretch(1);
    searchChrome->addWidget(searchIcon);
    m_normalSearchPlaceholder = QStringLiteral("Search");

    const QFont normalSearchFont = ui->searchBar->font();
    QFont placeholderFont = normalSearchFont;
    placeholderFont.setItalic(true);
    ui->searchBar->setFont(placeholderFont);
    connect(ui->searchBar, &QLineEdit::textChanged, this,
            [searchBar = ui->searchBar, normalSearchFont, placeholderFont](const QString &text) {
                searchBar->setFont(text.isEmpty() ? placeholderFont : normalSearchFont);
            });

    /* Force all three textboxes to be the same height */
    ui->primaryNavHole->setContentsMargins(QMargins(0, 0, 0, 0));
    ui->secondaryNavHole->setContentsMargins(QMargins(0, 0, 0, 0));
    ui->primaryNavHole->setFixedHeight(23);
    ui->secondaryNavHole->setFixedHeight(23);
    ui->navs->setFixedHeight(28);
    ui->navs->back()->setFixedSize(27, 25);
    ui->navs->forward()->setFixedSize(24, 25);
    ui->navs->menuButton()->setFixedSize(13, 25);
    ui->searchBar->setFixedSize(240, 23);

    setStyleSheet(QStringLiteral(R"(
        #aero7ExplorerHeader { background: #eef5fc; }
        #aero7NavigationBar { background: #b9d1ea; }
        #aero7NavigationGap {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                        stop:0 #b9d1ea,
                                        stop:0.74 #b9d1ea,
                                        stop:0.75 #a9bfd6,
                                        stop:1 #a9bfd6);
        }
        QFrame#primaryNavHole,
        QFrame#secondaryNavHole {
            border-top: 1px solid #53595e;
            border-right: 1px solid #7f9db9;
            border-bottom: 1px solid #a9b4bf;
            border-left: 1px solid #7f9db9;
            border-radius: 0;
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                        stop:0 #fafcfe,
                                        stop:0.08 #eaf1f9,
                                        stop:0.16 #ebf2f9,
                                        stop:0.22 #ecf2f9,
                                        stop:0.78 #ecf2f9,
                                        stop:0.84 #ebf2f9,
                                        stop:0.92 #eaf1f9,
                                        stop:1 #fafcfe);
        }
        QFrame#primaryNavHole QLabel,
        QFrame#secondaryNavHole QLabel {
            border: 0;
            background: transparent;
        }
        QToolButton#aero7AddressHistoryButton,
        QToolButton#aero7RefreshButton {
            border: 0;
            border-radius: 0;
            background: transparent;
            margin: 0;
            padding: 0;
        }
        QToolButton#aero7RefreshButton {
            border-left: 1px solid #a9b4bf;
        }
        QToolButton#aero7AddressHistoryButton::menu-indicator {
            image: none;
            width: 0;
        }
        QToolButton#aero7AddressHistoryButton:hover,
        QToolButton#aero7RefreshButton:hover {
            background: rgba(185, 218, 245, 145);
        }
        #aero7CommandBar {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                        stop:0 #f5f9fd, stop:1 #e5eef8);
            border-top: 1px solid #d7e2ed;
            border-bottom: 1px solid #a0afc3;
        }
        QToolButton[aero7Command="true"] {
            color: #17375e;
            background: transparent;
            border: 1px solid transparent;
            border-radius: 2px;
            padding: 0 13px;
        }
        QToolButton[aero7Command="true"]:hover {
            color: #0b2f57;
            border-color: #a8bfd8;
            background: rgba(255, 255, 255, 150);
        }
        QToolButton[aero7Command="true"]:pressed,
        QToolButton[aero7Command="true"]:checked {
            border-color: #7da2c9;
            background: #d8e8f7;
        }
        QToolButton[aero7IconCommand="true"] {
            padding: 0;
        }
        #aero7NavigationBar QLineEdit {
            border-top: 1px solid #53595e;
            border-right: 1px solid #7f9db9;
            border-bottom: 1px solid #a9b4bf;
            border-left: 1px solid #7f9db9;
            border-radius: 0;
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                        stop:0 #fafcfe,
                                        stop:0.08 #eaf1f9,
                                        stop:0.16 #ebf2f9,
                                        stop:0.22 #ecf2f9,
                                        stop:0.78 #ecf2f9,
                                        stop:0.84 #ebf2f9,
                                        stop:0.92 #eaf1f9,
                                        stop:1 #fafcfe);
            padding: 1px 27px 1px 6px;
            color: #222;
        }
        QLabel#aero7SearchIcon {
            border: 0;
            background: transparent;
        }
    )"));
}

void DolphinWindowHeader::setComputerMode(bool enabled)
{
    m_computerMode = enabled;
    if (enabled)
        m_recycleBinMode = false;
    updateShellMode();
}

void DolphinWindowHeader::setRecycleBinMode(bool enabled)
{
    m_recycleBinMode = enabled;
    if (enabled)
        m_computerMode = false;
    updateShellMode();
}

void DolphinWindowHeader::updateShellMode()
{
    m_organize->setVisible(true);
    const bool normalFolderMode = !m_computerMode && !m_recycleBinMode;
    m_include->setVisible(normalFolderMode);
    m_share->setVisible(normalFolderMode);
    m_newFolder->setVisible(normalFolderMode);
    for (QToolButton *button : std::as_const(m_computerCommands))
        button->setVisible(m_computerMode);
    for (QToolButton *button : std::as_const(m_recycleBinCommands))
        button->setVisible(m_recycleBinMode);

    if (m_computerMode)
        ui->searchBar->setPlaceholderText(QStringLiteral("Search Computer"));
    else if (m_recycleBinMode)
        ui->searchBar->setPlaceholderText(QStringLiteral("Search Recycle Bin"));
    else
        ui->searchBar->setPlaceholderText(m_normalSearchPlaceholder);

    if (QLabel *icon = findChild<QLabel *>(QStringLiteral("aero7LocationIcon"))) {
        QString iconName = QStringLiteral("folder-download");
        if (m_computerMode)
            iconName = QStringLiteral("computer");
        else if (m_recycleBinMode)
            iconName = QStringLiteral("user-trash");
        icon->setPixmap(QIcon::fromTheme(iconName).pixmap(16, 16));
    }
}

void DolphinWindowHeader::setLocationName(const QString &name)
{
    const QString scope = name.trimmed().isEmpty() ? QStringLiteral("this folder") : name.trimmed();
    m_normalSearchPlaceholder = QStringLiteral("Search %1").arg(scope);
    if (!m_computerMode && !m_recycleBinMode)
        ui->searchBar->setPlaceholderText(m_normalSearchPlaceholder);
}

DolphinWindowHeader::~DolphinWindowHeader()
{
    delete ui;
}
