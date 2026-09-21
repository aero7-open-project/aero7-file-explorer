# Everyday File Explorer Tasks

## Open a location

Use the File Explorer taskbar pin, a Start-menu folder link, or Computer.
Click a breadcrumb to move to a parent. Enter a real path when you need a
location outside the friendly navigation groups. **Computer** is integrated
into the same Explorer window; it is not a separate disk-list application.

The name and icon belong to the maintained Aero7 Dolphin fork. The canonical
package is `aero7-file-explorer` and desktop entry is
`org.aero7.FileExplorer.desktop`. Historical `dolphin` and `aero7-dolphin`
commands remain aliases, not separate applications you must pin as well.

## Choose a useful file view

Use the view selector to switch among Extra Large, Large, Medium, Small,
List, Details, Tiles, and Content. Use larger previews for pictures and
Details when names, dates, types, and sizes matter. Sort through the available
column or sorting controls. The preview pane depends on installed preview
support; an unavailable preview does not mean the file is empty.

The command bar changes with the location and selection. A disabled Burn or
sharing action is not a completed operation. Check the available device,
backend, and selected item before treating it as a defect.

## Create and use a Library

Open Libraries, create a Library, and give it a name. In Library Properties,
include the real folders you want to see together. Choose the default save
location, optimization type, and navigation-pane visibility where available,
then **Apply** or **OK**. **Cancel** discards unapplied property changes.

A Library is a collection of locations, not another copy of their contents.
Removing a folder from a Library does not delete that folder. Before deleting
anything, distinguish a Library definition from a real file/folder selected
inside it. An offline disk or network location can make some Library contents
unavailable. See [Libraries](Libraries) for storage and repair details.

## Copy, move, rename, and delete

Select the intended items and use copy/cut/paste, drag-and-drop, rename, or the
context menu. For a destination conflict, read the names and paths before
choosing **Replace**, **Don't copy**, or **Keep both**. Progress and remaining
errors are real operation state; waiting for the window to close is not a
substitute for checking the destination.

**Retry** retries a failed step after you fix its cause. **Skip** leaves that
item unresolved. **Cancel** stops remaining work but cannot necessarily undo
items already transferred. Keep backups when replacing important files.
Batch rename is available separately from single-item inline rename.

Use Recycle Bin for recoverable deletion where the filesystem supports it.
Restore returns a trashed item through the real trash backend. Emptying the
bin and permanent deletion are destructive; read confirmations carefully.
Remote filesystems do not always provide desktop trash semantics.

## Understand Computer and Network

Computer displays user-facing storage with real capacity/free-space data and
removable/optical groups where devices exist. Linux pseudo-filesystems and
implementation mounts are intentionally filtered. They are not erased or
unmounted by hiding them from this view.

Network depends on connectivity, installed KIO protocols, discovery, access
rights, and credentials. A Windows-style Network heading does not implement
the retired Windows HomeGroup protocol. See
[Computer and Navigation](Computer-and-Navigation).

## Open and Save dialogs

Applications using Aero7's common-dialog integration can show the familiar
navigation, Libraries, filename, and filter controls. This is not a global
replacement of every toolkit's chooser: sandboxed, browser-owned, GTK, or
other third-party dialogs can differ. A local file operation may use Aero7 UI
while a remote KIO transport still presents its own authentication/error UI.

## Limits worth knowing before reporting a bug

There is no general administrator-elevation continuation for arbitrary file
permission errors, and Previous Versions is not claimed without a real
snapshot backend. Do not change system-wide file permissions to make a file
operation succeed. The current Beta 2 release gate also includes non-green
upstream-derived search/view/accessibility tests; screenshots do not close
those failures. See [Troubleshooting](Troubleshooting).
