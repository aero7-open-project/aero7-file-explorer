/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aero7commondialog.h"

#include <Aero7Qt/stylesheet.h>
#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QTextStream>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("aero7-file-dialog"));
    Aero7::applyApplicationStyle(&app);

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({QStringLiteral("mode"), QStringLiteral("open, open-multiple, save, or folder"),
                      QStringLiteral("mode"), QStringLiteral("open")});
    parser.addOption({QStringLiteral("app-id"), QStringLiteral("Application state identifier"),
                      QStringLiteral("id"), QStringLiteral("aero7")});
    parser.addOption({QStringLiteral("name"), QStringLiteral("Suggested file name"), QStringLiteral("name")});
    parser.addOption({QStringLiteral("suffix"), QStringLiteral("Default extension"), QStringLiteral("extension")});
    parser.addOption({QStringLiteral("filter"), QStringLiteral("File type filter; may be repeated"),
                      QStringLiteral("filter")});
    parser.process(app);

    const QString modeName = parser.value(QStringLiteral("mode"));
    const Aero7CommonDialog::Mode mode = modeName == QLatin1String("save")
        ? Aero7CommonDialog::Mode::SaveFile
        : modeName == QLatin1String("folder")
            ? Aero7CommonDialog::Mode::ChooseFolder
            : modeName == QLatin1String("open-multiple")
                ? Aero7CommonDialog::Mode::OpenFiles
                : Aero7CommonDialog::Mode::OpenFile;
    Aero7CommonDialog dialog(mode, parser.value(QStringLiteral("app-id")));
    if (parser.isSet(QStringLiteral("filter")))
        dialog.setNameFilters(parser.values(QStringLiteral("filter")));
    dialog.setSuggestedFileName(parser.value(QStringLiteral("name")));
    dialog.setDefaultSuffix(parser.value(QStringLiteral("suffix")));
    if (dialog.exec() != QDialog::Accepted)
        return 1;
    QTextStream output(stdout);
    for (const QString &path : dialog.selectedFiles())
        output << path << Qt::endl;
    return 0;
}
