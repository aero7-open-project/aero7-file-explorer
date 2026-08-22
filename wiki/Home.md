# Aero7 File Explorer Wiki

Welcome to the documentation for **Aero7 File Explorer**, the complete
Windows 7-style file-management application for the Aero7 desktop and a
maintained fork of [KDE Dolphin](https://github.com/KDE/dolphin).

This Wiki belongs only to the File Explorer project. Documentation for the
Aero7 operating system, installer, Control Panel, and package repository
remains in each project's own repository.

[![Aero7 File Explorer Documents view](https://raw.githubusercontent.com/aero7-open-project/aero7-file-explorer/main/docs/screenshots/file-explorer-documents.png)](Screenshots)

## Start here

| I want to… | Read… |
| --- | --- |
| Learn the Explorer window and navigation model | [User Guide](User-Guide) |
| Review every Aero7-owned feature | [Feature Reference](Feature-Reference) |
| Configure Documents, Music, Pictures, or Videos | [Libraries](Libraries) |
| Understand Computer and the navigation pane | [Computer and Navigation](Computer-and-Navigation) |
| Learn copy, move, delete, properties, and dialogs | [File Operations and Dialogs](File-Operations-and-Dialogs) |
| Install or update the package | [Installation and Updates](Installation-and-Updates) |
| Build and test the source | [Building and Testing](Building-and-Testing) |
| Understand the Dolphin fork relationship | [Architecture and Upstream](Architecture-and-Upstream) |
| Fix a startup or file-operation problem | [Troubleshooting](Troubleshooting) |
| Browse current VM captures | [Screenshots](Screenshots) |

## Application identity

| | |
| --- | --- |
| Visible name | File Explorer |
| Executable | `aero7-file-explorer` |
| Desktop id | `org.aero7.FileExplorer` |
| Package | `aero7-file-explorer` |
| Compatibility commands | `dolphin`, `aero7-dolphin` |
| Upstream parent | `KDE/dolphin` |

The compatibility commands are supplied by the Aero7 package so existing
shortcuts and integrations continue to open File Explorer. They do not install
a second file manager or change the public Aero7 application identity.

## What makes it Aero7 File Explorer

- Windows 7-inspired navigation header, command bar, details view, status area,
  and navigation-pane organization.
- Favorites, Libraries, Computer, and Network as first-class navigation groups.
- A real multi-location Library model shared with Aero7 common file dialogs.
- An integrated Computer surface showing user-facing storage with capacity and
  free-space information while suppressing raw Linux implementation mounts.
- Aero7-owned properties, Recycle Bin, copy, move, rename, delete, conflict,
  progress, and recoverable-error workflows.
- A separate executable, desktop entry, D-Bus identity, metadata, tests, and
  distribution package.

The project retains the mature KDE/Dolphin file-management foundation and its
applicable license notices while maintaining the Aero7 interface and shell
contract as the product experience.

## Current status

File Explorer is included in Aero7 and distributed as the
`aero7-file-explorer` package through the signed Aero7 package repository.
Current documentation and screenshots cover the installed application,
Libraries, Computer, common dialogs, file operations, and the supported
package transition from the retired `aero7-dolphin` name.

The project is actively maintained. Features without a safe and complete
backend are disabled or omitted instead of being presented as working. Review
[Feature Reference](Feature-Reference) for the implemented boundary and
[Troubleshooting](Troubleshooting) before reporting a problem.

## Project links

- [Source repository](https://github.com/aero7-open-project/aero7-file-explorer)
- [Issue tracker](https://github.com/aero7-open-project/aero7-file-explorer/issues)
- [KDE Dolphin upstream](https://github.com/KDE/dolphin)
- [Aero7 package repository](https://github.com/memegeko/aero7-repo)
- [Aero7 operating system](https://github.com/aero7-open-project/aero7)
- [Aero7 website](https://aero7.miku-dayo.com/)

## Independent-project notice

Aero7 File Explorer and Aero7 are independent open-source projects. They are
not affiliated with, authorized, sponsored, endorsed, or approved by Microsoft
Corporation. Microsoft and Windows are trademarks of the Microsoft group of
companies. KDE, Dolphin, and other trademarks belong to their respective
owners.
