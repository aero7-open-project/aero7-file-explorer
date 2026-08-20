# Architecture and Upstream

## GitHub fork relationship

`aero7-open-project/aero7-file-explorer` is a real GitHub network fork of
[`KDE/dolphin`](https://github.com/KDE/dolphin). GitHub records KDE Dolphin as
both the parent and source repository. The Aero7 default branch is `main`.

## Product boundary

Aero7 File Explorer is the complete file-management product for Aero7. Its
public identity is independent even though its implementation is maintained as
a Dolphin fork:

- executable: `aero7-file-explorer`;
- desktop id and D-Bus identity: `org.aero7.FileExplorer`;
- visible name: File Explorer;
- Aero7 shell, Library, Computer, dialog, properties, and operation layers;
- Aero7-specific tests and distribution package.

Historical Dolphin class, library, and internal file names remain where a mass
rename would create compatibility risk without improving the user experience.
They are implementation details rather than the public product identity.

## Major layers

### Upstream file-manager foundation

The fork retains Dolphin's mature URL navigation, directory model, view model,
KIO integration, metadata, selection, drag-and-drop, network transport, and
general filesystem foundations.

### Aero7 Explorer surface

The main window and view containers add the Windows 7-inspired navigation
header, command bar, navigation groups, Details layout, status surface, search,
preview behavior, and Aero7 application identity.

### Aero7 shell library

The `aero7shellui` implementation owns Libraries, Computer presentation,
properties, local file operations, common dialogs, shell initialization, and
the isolated integration self-test.

### Packaging compatibility

The distribution package supplies `dolphin` and `aero7-dolphin` aliases to the
canonical Aero7 executable. It declares replacement metadata so users migrate
from the retired package rather than running two conflicting file managers.

## Adopting upstream changes

Upstream changes should be reviewed against the current Aero7 shell contract,
then rebased or integrated deliberately. Preserve applicable upstream commits
and license notices. After integration, rerun the focused CTest targets,
`aero7-shell-selftest`, package validation, and VM visual checks.

Avoid replacing Aero7-owned UI with generic KDE surfaces merely to make a
rebase easier. Compatibility with new upstream internals and preservation of
the Aero7 product experience are both required.
