/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aero7commondialog.h"
#include "aero7fileoperations.h"
#include "aero7libraries.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTextStream>
#include <QUrl>

namespace {
bool writeFile(const QString &path, const QByteArray &contents)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && file.write(contents) == contents.size();
}

bool require(bool condition, const QString &message)
{
    QTextStream stream(condition ? stdout : stderr);
    stream << (condition ? "PASS " : "FAIL ") << message << Qt::endl;
    return condition;
}
}

int main(int argc, char **argv)
{
    QTemporaryDir sandbox;
    if (!sandbox.isValid())
        return 2;
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("XDG_CONFIG_HOME", QFile::encodeName(QDir(sandbox.path()).filePath(QStringLiteral("config"))));
    qputenv("XDG_DATA_HOME", QFile::encodeName(QDir(sandbox.path()).filePath(QStringLiteral("data"))));
    qputenv("XDG_CACHE_HOME", QFile::encodeName(QDir(sandbox.path()).filePath(QStringLiteral("cache"))));
    QApplication app(argc, argv);
    bool ok = true;

    const QString one = QDir(sandbox.path()).filePath(QStringLiteral("library-one"));
    const QString two = QDir(sandbox.path()).filePath(QStringLiteral("library-two"));
    const QString offline = QDir(sandbox.path()).filePath(QStringLiteral("offline"));
    QDir().mkpath(one);
    QDir().mkpath(two);
    writeFile(QDir(one).filePath(QStringLiteral("first document.txt")), "one");
    writeFile(QDir(two).filePath(QStringLiteral("second document.txt")), "two");
    Aero7Library documents = Aero7Libraries::instance().library(QStringLiteral("documents"));
    documents.locations = {one, two, offline};
    documents.saveLocation = two;
    QString libraryError;
    ok &= require(Aero7Libraries::instance().saveLibrary(documents, &libraryError),
                  QStringLiteral("Libraries save atomically: %1").arg(libraryError));
    const Aero7Library saved = Aero7Libraries::instance().library(QStringLiteral("documents"));
    ok &= require(saved.locations.size() == 3 && saved.saveLocation == two,
                  QStringLiteral("multiple/offline locations and default save location persist"));
    const QString merged = Aero7Libraries::instance().materializedPath(QStringLiteral("documents"));
    ok &= require(QFileInfo(QDir(merged).filePath(QStringLiteral("first document.txt"))).exists()
                      && QFileInfo(QDir(merged).filePath(QStringLiteral("second document.txt"))).exists(),
                  QStringLiteral("Library merged view contains both folders"));
    const QString search = Aero7Libraries::instance().materializeSearch(
        QStringLiteral("documents"), QStringLiteral("second"), &libraryError);
    ok &= require(!search.isEmpty() && QDir(search).entryList(QDir::AllEntries | QDir::NoDotAndDotDot).size() == 1,
                  QStringLiteral("Library search spans configured folders"));

    for (Aero7CommonDialog::Mode mode : {Aero7CommonDialog::Mode::OpenFile,
                                         Aero7CommonDialog::Mode::OpenFiles,
                                         Aero7CommonDialog::Mode::SaveFile,
                                         Aero7CommonDialog::Mode::ChooseFolder}) {
        Aero7CommonDialog dialog(mode, QStringLiteral("selftest"));
        ok &= require(dialog.windowTitle().size() > 0,
                      QStringLiteral("common dialog mode constructs"));
    }

    const QString source = QDir(sandbox.path()).filePath(QStringLiteral("source"));
    const QString destination = QDir(sandbox.path()).filePath(QStringLiteral("destination"));
    QDir().mkpath(source);
    QDir().mkpath(destination);
    writeFile(QDir(source).filePath(QStringLiteral("small.txt")), "new");
    ok &= require(Aero7FileOperationDialog::run(Aero7FileOperationDialog::Operation::Copy,
                                                {QUrl::fromLocalFile(QDir(source).filePath(QStringLiteral("small.txt")))},
                                                QUrl::fromLocalFile(destination)),
                  QStringLiteral("small file copy completes"));
    writeFile(QDir(destination).filePath(QStringLiteral("small.txt")), "old");
    qputenv("AERO7_TEST_CONFLICT", "keep-both");
    ok &= require(Aero7FileOperationDialog::run(Aero7FileOperationDialog::Operation::Copy,
                                                {QUrl::fromLocalFile(QDir(source).filePath(QStringLiteral("small.txt")))},
                                                QUrl::fromLocalFile(destination))
                      && QFileInfo::exists(QDir(destination).filePath(QStringLiteral("small (2).txt"))),
                  QStringLiteral("same-name keep-both generates a safe name"));
    qputenv("AERO7_TEST_CONFLICT", "replace");
    ok &= require(Aero7FileOperationDialog::run(Aero7FileOperationDialog::Operation::Copy,
                                                {QUrl::fromLocalFile(QDir(source).filePath(QStringLiteral("small.txt")))},
                                                QUrl::fromLocalFile(destination)),
                  QStringLiteral("same-name replace completes"));
    writeFile(QDir(destination).filePath(QStringLiteral("small.txt")), "preserve");
    qputenv("AERO7_TEST_CONFLICT", "skip");
    ok &= require(Aero7FileOperationDialog::run(Aero7FileOperationDialog::Operation::Copy,
                                                {QUrl::fromLocalFile(QDir(source).filePath(QStringLiteral("small.txt")))},
                                                QUrl::fromLocalFile(destination))
                      && QFile(QDir(destination).filePath(QStringLiteral("small.txt"))).size() == 8,
                  QStringLiteral("same-name skip preserves the destination"));
    qputenv("AERO7_TEST_CONFLICT", "cancel");
    ok &= require(!Aero7FileOperationDialog::run(Aero7FileOperationDialog::Operation::Copy,
                                                 {QUrl::fromLocalFile(QDir(source).filePath(QStringLiteral("small.txt")))},
                                                 QUrl::fromLocalFile(destination)),
                  QStringLiteral("conflict cancellation aborts cleanly"));
    qunsetenv("AERO7_TEST_CONFLICT");
    qputenv("AERO7_TEST_AVAILABLE_BYTES", "1");
    ok &= require(!Aero7FileOperationDialog::run(Aero7FileOperationDialog::Operation::Move,
                                                 {QUrl::fromLocalFile(QDir(source).filePath(QStringLiteral("small.txt")))},
                                                 QUrl::fromLocalFile(destination)),
                  QStringLiteral("move preflight rejects insufficient destination space"));
    qunsetenv("AERO7_TEST_AVAILABLE_BYTES");
    writeFile(QDir(source).filePath(QStringLiteral("tree/sub/large.bin")), QByteArray(2 * 1024 * 1024, 'x'));
    ok &= require(Aero7FileOperationDialog::run(Aero7FileOperationDialog::Operation::Move,
                                                {QUrl::fromLocalFile(QDir(source).filePath(QStringLiteral("tree")))},
                                                QUrl::fromLocalFile(destination))
                      && QFileInfo::exists(QDir(destination).filePath(QStringLiteral("tree/sub/large.bin")))
                      && !QFileInfo::exists(QDir(source).filePath(QStringLiteral("tree"))),
                  QStringLiteral("directory-tree move preserves contents"));
    qputenv("AERO7_TEST_CONFIRM_DELETE", "yes");
    ok &= require(Aero7FileOperationDialog::deletePermanently(
                      {QUrl::fromLocalFile(QDir(destination).filePath(QStringLiteral("tree")))}),
                  QStringLiteral("permanent folder deletion is real"));
    qunsetenv("AERO7_TEST_CONFIRM_DELETE");

    return ok ? 0 : 1;
}
