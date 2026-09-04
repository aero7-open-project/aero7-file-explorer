/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QDialog>
#include <QStringList>

class QComboBox;
class QFileSystemModel;
class QLineEdit;
class QListView;
class QPushButton;
class QStandardItemModel;
class QTreeWidget;
class QTreeWidgetItem;

class Aero7CommonDialog final : public QDialog
{
    Q_OBJECT

public:
    enum class Mode { OpenFile, OpenFiles, SaveFile, ChooseFolder };

    explicit Aero7CommonDialog(Mode mode, const QString &applicationId,
                               QWidget *parent = nullptr);
    void setNameFilters(const QStringList &filters);
    void setSuggestedFileName(const QString &name);
    void setDefaultSuffix(const QString &suffix);
    void setInitialDirectory(const QString &path);
    QStringList selectedFiles() const;

private Q_SLOTS:
    void accept() override;
    void setDirectory(const QString &path, bool addHistory = true);
    void navigateBack();
    void navigateForward();
    void navigateUp();
    void activateIndex(const QModelIndex &index);
    void selectionChanged();
    void runSearch(const QString &text);

private:
    void buildNavigation();
    void updateButtons();
    QString pathForIndex(const QModelIndex &index) const;
    QString saveStateGroup() const;
    void restoreState();
    void persistState() const;

    Mode m_mode;
    QString m_applicationId;
    QString m_currentDirectory;
    QString m_defaultSuffix;
    QStringList m_result;
    QStringList m_history;
    int m_historyIndex = -1;

    QTreeWidget *m_navigation = nullptr;
    QListView *m_view = nullptr;
    QFileSystemModel *m_fileModel = nullptr;
    QStandardItemModel *m_searchModel = nullptr;
    QLineEdit *m_pathEdit = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QLineEdit *m_fileName = nullptr;
    QComboBox *m_fileType = nullptr;
    QPushButton *m_back = nullptr;
    QPushButton *m_forward = nullptr;
    QPushButton *m_up = nullptr;
    QPushButton *m_acceptButton = nullptr;
};
