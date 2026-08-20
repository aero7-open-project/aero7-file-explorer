# Aero7 File Explorer fork

Aero7 File Explorer is the complete file-management application for the Aero7
desktop and a maintained fork of KDE Dolphin. It has its own application
identity, executable, desktop entry, metadata, shell model, interface, common
dialogs, file-operation experience, tests, and distribution package.

The installed application identity is:

- binary: `aero7-file-explorer`
- desktop id: `org.aero7.FileExplorer`
- visible name: `File Explorer`

The fork preserves the applicable KDE and Dolphin copyright and free-software
license notices. Historical Dolphin class and library names remain internally
where renaming them would add compatibility risk without improving the Aero7
user experience. The upstream repository remains configured as the source used
to review and adopt future Dolphin changes.

The Aero7 distribution package supplies `dolphin` and `aero7-dolphin` command
aliases solely so existing shortcuts and third-party integrations continue to
work. They are compatibility routes to the Aero7 application, not separate
products.
