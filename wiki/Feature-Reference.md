# Feature Reference

This page distinguishes implemented behavior from compatibility routes and
known boundaries.

## Explorer chrome and views

- Windows 7-inspired navigation header and command bar.
- Back, Forward, breadcrumb navigation, text-location entry, Refresh, and
  folder- or Library-scoped search.
- Favorites, Libraries, Computer, and Network navigation groups.
- Extra Large, Large, Medium, Small, List, Details, Tiles, and Content view
  commands backed by real view modes and metadata roles.
- Details headers for Name, Date modified, Type, and Size with stable Aero7
  sizing and layout behavior.
- Toggleable preview/information pane with live selection details.
- Aero7 window identity, icon, desktop entry, metadata, D-Bus service, and
  taskbar grouping.

## Libraries

- Documents, Music, Pictures, and Videos defaults.
- Multiple included folders per Library.
- Default save location, optimize-for type, and navigation-pane visibility.
- Offline-location handling and duplicate-name protection.
- Atomic per-user persistence in `~/.config/aero7/libraries.json`.
- Native Library Properties with Add, Remove, Set save location, Restore
  Defaults, Apply, OK, and Cancel semantics.
- One merged Library model shared by Explorer and Aero7 common dialogs.

## Computer and storage

- Integrated Computer view rather than a separate Linux mount browser.
- Fixed and removable user-facing volumes with real capacity and free-space
  data.
- Drive capacity bars and optical-media presentation.
- Filtering of raw Linux implementation mounts from the public Explorer
  navigation surface.

## Recycle Bin and properties

- Real KIO trash, Restore selected, Restore all, and Empty operations.
- Permanent-delete confirmation and configurable delete behavior.
- Aero7 Recycle Bin Properties.
- File and folder Properties with General, Security, and Details pages.
- Recursive folder size/count, allocated size, timestamps, attributes,
  ownership, group, mode, MIME type, image dimensions, and default-program
  integration where supported.

## File operations

- Aero7 copy and move progress with source, destination, current item,
  remaining files/bytes, progress, and speed.
- Small files, large files, and recursive directory trees.
- Collision choices for Replace, Don't Copy, Keep Both, and applying a choice
  to subsequent conflicts.
- Safe `name (2).ext` keep-both naming.
- Retry, Skip, and Cancel paths for recoverable failures.
- Destination free-space preflight, cancellation, rename, Recycle Bin delete,
  and permanent delete.

## Common dialogs

The project installs the Aero7 common file-dialog component used by Aero7-owned
applications. It supports Open File, Open Files, Save File, and Select Folder
workflows while sharing Explorer Libraries and navigation behavior.

## Compatibility and current boundaries

- `dolphin` and `aero7-dolphin` are package aliases to the Aero7 executable.
- Arbitrary third-party toolkit dialogs are not intercepted; applications must
  integrate the Aero7 common-dialog protocol.
- Local operations use Aero7 progress, conflict, and error UI. Remote URL
  operations may still use KIO transport fallback delegates.
- Recoverable permission failures offer Retry, Skip, and Cancel; a dedicated
  administrator-elevation continuation is future work.
- Previous Versions is not shown because Aero7 currently installs no snapshot
  backend. The interface does not display a nonfunctional tab.
