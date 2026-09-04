#!/usr/bin/env python3
"""Allow theme lookup only for user content and third-party applications."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ALLOWED_FILES = {
    "settings/servicemodel.cpp",                 # third-party service menu metadata
    "dolphinnavigatorswidgetaction.cpp",         # KDE Connect application identity
    "dolphintabwidget.cpp",                      # URL and user-folder content
    "kitemviews/kfileitemlistview.cpp",          # file model content
    "dolphinrecenttabsmenu.cpp",                 # recently visited URL content
    "itemactions/setfoldericonitemaction.cpp",   # user-selected folder icon
    "aero7/aero7properties.cpp",                 # user-folder content
    "search/selectors/filetypeselector.cpp",     # MIME type content
    "aero7/aero7commondialog.cpp",               # user locations and folders
    "aero7/aero7computerdialog.cpp",             # mounted-device content
    "dolphinplacesmodelsingleton.cpp",           # mounted devices and user places
    "search/popup.cpp",                          # external KFind identity
    "panels/places/placespanel.cpp",             # user-place content
    "statusbar/diskspaceusagemenu.cpp",           # external Filelight/KDiskFree identities
    "statusbar/dolphinstatusbar.cpp",             # current URL/file content
    "views/dolphinview.cpp",                     # file and MIME content
    "dolphinmainwindow.cpp",                     # URLs and external tools
    "kitemviews/kstandarditemlistwidget.cpp",    # file icons and metadata overlays
    "panels/information/informationpanelcontent.cpp", # selected file content
}


def main() -> int:
    errors: list[str] = []
    for path in ROOT.rglob("*"):
        if path.suffix not in {".cpp", ".h", ".qml", ".ui"}:
            continue
        relative = path.relative_to(ROOT).as_posix()
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            if "QIcon::fromTheme" in line and relative not in ALLOWED_FILES:
                errors.append(f"{relative}:{number}: unapproved theme icon: {line.strip()}")

    for desktop in (ROOT).glob("org.aero7*.desktop"):
        for number, line in enumerate(desktop.read_text(encoding="utf-8").splitlines(), 1):
            if line.startswith("Icon=") and not line.startswith("Icon=aero7-"):
                errors.append(f"{desktop.relative_to(ROOT)}:{number}: Aero7 launcher icon is not namespaced")
    shell_init = ROOT / "aero7/org.aero7.ShellInit.desktop"
    if "Icon=aero7-file-explorer" not in shell_init.read_text(encoding="utf-8"):
        errors.append("aero7/org.aero7.ShellInit.desktop: launcher icon is not namespaced")

    file_explorer_desktop = ROOT / "org.aero7.FileExplorer.desktop"
    file_explorer_identity = file_explorer_desktop.read_text(encoding="utf-8")
    for required_line in (
        "Name=File Explorer",
        "Icon=aero7-file-explorer",
        "StartupWMClass=org.aero7.FileExplorer",
    ):
        if required_line not in file_explorer_identity.splitlines():
            errors.append(
                f"org.aero7.FileExplorer.desktop: missing stable launcher identity: {required_line}"
            )

    if errors:
        raise SystemExit("\n".join(errors))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
