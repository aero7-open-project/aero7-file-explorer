/*
 * SPDX-FileCopyrightText: 2009 Shaun Reich <shaun.reich@kdemail.net>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "trashsettingspage.h"

#include <QCheckBox>
#include <QDir>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSettings>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QVBoxLayout>

namespace {
QString trashConfigPath()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation))
        .filePath(QStringLiteral("trashrc"));
}

QString humanSize(quint64 bytes)
{
    static constexpr quint64 kib = 1024;
    static constexpr quint64 mib = kib * 1024;
    static constexpr quint64 gib = mib * 1024;
    if (bytes >= gib)
        return QStringLiteral("%1 GB").arg(QString::number(double(bytes) / double(gib), 'f', 1));
    if (bytes >= mib)
        return QStringLiteral("%1 MB").arg(QString::number(double(bytes) / double(mib), 'f', 1));
    return QStringLiteral("%1 KB").arg(bytes / kib);
}
}

TrashSettingsPage::TrashSettingsPage(QWidget *parent)
    : SettingsPageBase(parent)
{
    auto *outer = new QVBoxLayout(this);

    const QString trashPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + QStringLiteral("/Trash");
    const QStorageInfo storage(trashPath);
    auto *locationBox = new QGroupBox(QStringLiteral("Recycle Bin Location"), this);
    auto *locationForm = new QFormLayout(locationBox);
    m_location = new QLabel(QDir::toNativeSeparators(trashPath), locationBox);
    m_location->setTextInteractionFlags(Qt::TextSelectableByMouse);
    locationForm->addRow(QStringLiteral("Location:"), m_location);
    locationForm->addRow(QStringLiteral("Space available:"),
                         new QLabel(humanSize(storage.bytesAvailable()), locationBox));
    outer->addWidget(locationBox);

    auto *settingsBox = new QGroupBox(QStringLiteral("Settings for selected location"), this);
    auto *settingsLayout = new QVBoxLayout(settingsBox);
    m_customSize = new QCheckBox(QStringLiteral("Custom size"), settingsBox);
    m_maximumSize = new QSpinBox(settingsBox);
    m_maximumSize->setRange(1, 1024 * 1024);
    m_maximumSize->setSuffix(QStringLiteral(" MB"));
    m_deleteImmediately = new QCheckBox(
        QStringLiteral("Don't move files to the Recycle Bin. Remove files immediately when deleted."),
        settingsBox);
    settingsLayout->addWidget(m_customSize);
    settingsLayout->addWidget(m_maximumSize);
    settingsLayout->addWidget(m_deleteImmediately);
    outer->addWidget(settingsBox);

    m_confirmDelete = new QCheckBox(QStringLiteral("Display delete confirmation dialog"), this);
    outer->addWidget(m_confirmDelete);
    outer->addStretch(1);

    connect(m_customSize, &QCheckBox::toggled, m_maximumSize, &QWidget::setEnabled);
    const auto edited = [this]() {
        if (!m_loading)
            Q_EMIT changed();
    };
    connect(m_customSize, &QCheckBox::toggled, this, edited);
    connect(m_maximumSize, &QSpinBox::valueChanged, this, edited);
    connect(m_deleteImmediately, &QCheckBox::toggled, this, edited);
    connect(m_confirmDelete, &QCheckBox::toggled, this, edited);

    loadSettings();
}

TrashSettingsPage::~TrashSettingsPage() = default;

void TrashSettingsPage::applySettings()
{
    QSettings output(trashConfigPath(), QSettings::IniFormat);
    output.beginGroup(QStringLiteral("Aero7"));
    output.setValue(QStringLiteral("MaximumSizeMiB"),
                    m_customSize->isChecked() ? m_maximumSize->value() : 0);
    output.setValue(QStringLiteral("DeleteImmediately"), m_deleteImmediately->isChecked());
    output.setValue(QStringLiteral("ConfirmDelete"), m_confirmDelete->isChecked());
    output.endGroup();
    output.sync();
}

void TrashSettingsPage::restoreDefaults()
{
    m_loading = true;
    m_customSize->setChecked(false);
    m_maximumSize->setValue(1024);
    m_maximumSize->setEnabled(false);
    m_deleteImmediately->setChecked(false);
    m_confirmDelete->setChecked(true);
    m_loading = false;
    Q_EMIT changed();
}

void TrashSettingsPage::loadSettings()
{
    QSettings input(trashConfigPath(), QSettings::IniFormat);
    input.beginGroup(QStringLiteral("Aero7"));
    const int maximum = input.value(QStringLiteral("MaximumSizeMiB"), 0).toInt();
    const bool immediate = input.value(QStringLiteral("DeleteImmediately"), false).toBool();
    const bool confirm = input.value(QStringLiteral("ConfirmDelete"), true).toBool();
    input.endGroup();

    m_loading = true;
    m_customSize->setChecked(maximum > 0);
    m_maximumSize->setValue(maximum > 0 ? maximum : 1024);
    m_maximumSize->setEnabled(maximum > 0);
    m_deleteImmediately->setChecked(immediate);
    m_confirmDelete->setChecked(confirm);
    m_loading = false;
}

#include "moc_trashsettingspage.cpp"
