# File Explorer User Guide

## Opening File Explorer

Use any of these normal Aero7 routes:

- choose **Documents**, **Pictures**, **Music**, or **Computer** in the Start
  menu;
- open a folder from the desktop;
- select the File Explorer taskbar button;
- run `aero7-file-explorer` from a terminal or launcher.

The package also accepts the historical `dolphin` and `aero7-dolphin` command
names for compatibility. All three routes open the same Aero7 application.

## Window layout

### Navigation header

The top row contains Back and Forward navigation, the breadcrumb address bar,
Refresh, and search. Breadcrumb segments move directly to a parent location.
Focusing the location field provides a text path for direct entry without
turning the normal Explorer surface into a permanent editable toolbar.

### Command bar

The command bar changes with the current location and provides Windows 7-style
commands such as **Organize**, **Include in library**, **Share with**, **Burn**,
**New folder**, view selection, and preview-pane control. Commands are backed by
real file-manager actions; unavailable operations are disabled instead of
being displayed as fake working buttons.

### Navigation pane

The left pane is organized into:

- **Favorites** — quick access to Desktop, Downloads, Recent Places, and other
  pinned locations;
- **Libraries** — Documents, Music, Pictures, Videos, and user-created Library
  definitions;
- **Computer** — user-facing fixed and removable storage;
- **Network** — discoverable network locations supported by the installed KIO
  backends.

### File area

The file area supports Extra Large, Large, Medium, Small, List, Details, Tiles,
and Content presentations. Details view uses the familiar Name, Date modified,
Type, and Size columns. The selected view is implemented by the underlying
file model and is not a static mockup.

### Status and information

The status area reports item counts and selection information. The optional
preview/information pane shows supported previews and live metadata for the
selected file or folder.

## Searching

Search is scoped to the current folder. When a Library is active, search covers
all included Library locations, including multiple roots. Search results use
the normal file model so opening, properties, and file operations remain
available.

## Selecting and opening items

Normal keyboard modifiers, rubber-band selection, double-click opening,
context menus, drag-and-drop, and inline rename are retained from the mature
Dolphin backend. Aero7 adds its own multi-item rename dialog where inline
editing is not suitable.

## Refreshing and navigation history

Use the Refresh control or the standard refresh action to rescan the current
location. Back and Forward maintain the browsing history for each active view.
Opening a new location after going back creates a new forward-history path, as
expected from a normal Explorer window.

## Related pages

- [Libraries](Libraries)
- [Computer and Navigation](Computer-and-Navigation)
- [File Operations and Dialogs](File-Operations-and-Dialogs)
- [Feature Reference](Feature-Reference)
