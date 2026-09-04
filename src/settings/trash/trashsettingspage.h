/*
 * SPDX-FileCopyrightText: 2009 Shaun Reich <shaun.reich@kdemail.net>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef TRASHSETTINGSPAGE_H
#define TRASHSETTINGSPAGE_H

#include "settings/settingspagebase.h"

class QCheckBox;
class QLabel;
class QSpinBox;

/**
 * @brief Native Aero7 Recycle Bin settings page.
 */
class TrashSettingsPage : public SettingsPageBase
{
    Q_OBJECT

public:
    explicit TrashSettingsPage(QWidget *parent);
    ~TrashSettingsPage() override;

    /** @see SettingsPageBase::applySettings() */
    void applySettings() override;

    /** @see SettingsPageBase::restoreDefaults() */
    void restoreDefaults() override;

private:
    void loadSettings();

    QCheckBox *m_customSize = nullptr;
    QSpinBox *m_maximumSize = nullptr;
    QCheckBox *m_deleteImmediately = nullptr;
    QCheckBox *m_confirmDelete = nullptr;
    QLabel *m_location = nullptr;
    bool m_loading = false;
};

#endif
