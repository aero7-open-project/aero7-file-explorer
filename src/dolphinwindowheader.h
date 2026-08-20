#ifndef DOLPHINWINDOWHEADER_H
#define DOLPHINWINDOWHEADER_H

#include <QWidget>
#include <QList>
#include <QString>
#include "ui_dolphinwindowheader.h"

class DolphinMainWindow;
class QToolButton;

class DolphinWindowHeader : public QWidget
{
    Q_OBJECT
    friend class DolphinMainWindow;

public:
    explicit DolphinWindowHeader(QWidget *parent = nullptr);
    ~DolphinWindowHeader();
    void setComputerMode(bool enabled);
    void setRecycleBinMode(bool enabled);
    void setLocationName(const QString &name);

private:
    void updateShellMode();

    Ui::DolphinWindowHeader *ui;
    QToolButton *m_organize = nullptr;
    QToolButton *m_include = nullptr;
    QToolButton *m_share = nullptr;
    QToolButton *m_newFolder = nullptr;
    QToolButton *m_preview = nullptr;
    QToolButton *m_views = nullptr;
    QToolButton *m_restoreAll = nullptr;
    QToolButton *m_emptyRecycleBin = nullptr;
    QToolButton *m_recycleBinProperties = nullptr;
    QList<QToolButton *> m_computerCommands;
    QList<QToolButton *> m_recycleBinCommands;
    QString m_normalSearchPlaceholder;
    bool m_computerMode = false;
    bool m_recycleBinMode = false;
};

#endif // DOLPHINWINDOWHEADER_H
