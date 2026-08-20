# Aero7 File Explorer architecture

Aero7 File Explorer is the complete file-management application for the Aero7
desktop. It has its own application identity, executable, desktop entry,
metadata, shell model, interface, common dialogs, file-operation experience,
tests, and distribution package.

The installed application identity is:

- binary: `aero7-file-explorer`
- desktop id: `org.aero7.FileExplorer`
- visible name: `File Explorer`

The codebase includes implementation heritage from KDE Dolphin and preserves
the applicable upstream copyright and free-software license notices. Historical
Dolphin class and library names remain internally where renaming them would add
compatibility risk without improving the Aero7 user experience.

The Aero7 distribution package supplies `dolphin` and `aero7-dolphin` command
aliases solely so existing shortcuts and third-party integrations continue to
work. They are compatibility routes to the Aero7 application, not separate
products.
