# File Operations and Dialogs

## Copy and move

File Explorer provides Aero7 progress windows for local copy and move jobs.
The progress surface reports the source, destination, current item, completed
portion, remaining files and bytes, and transfer speed. Recursive directory
trees and large files use the same operation model.

Before a local transfer starts, Explorer checks destination free space where a
reliable capacity value is available. A rejected move does not delete the
source. Cancel stops the remaining work and preserves already completed items
according to the normal operation semantics.

## Name conflicts

When the destination already contains an item with the same name, choose:

- **Copy and Replace** — replace the destination item;
- **Don't Copy** — leave the destination item unchanged;
- **Copy but keep both** — create a safe name such as `report (2).txt`;
- **Apply to subsequent conflicts** — reuse the selected decision for the rest
  of the current operation.

Cancel leaves unresolved items untouched. Retry and Skip are available for
recoverable read, write, directory creation, replacement, and deletion errors.

## Rename

Single items normally support inline rename. Multi-item or non-inline cases use
an Aero7 rename dialog. File extensions and duplicate names are handled by the
same filesystem checks used by other Explorer operations.

## Delete and Recycle Bin

Normal delete routes items through the real KIO trash backend. Recycle Bin
supports Restore selected, Restore all, Empty, and Properties. Permanent delete
requires explicit confirmation when that safety setting is enabled.

Recycle Bin settings affect real behavior; they are not presentation-only
switches. Filesystems or remote locations that cannot use trash may require a
permanent-delete path.

## File and folder Properties

Properties provides Aero7 General, Security, and Details pages where the
backend can supply the data. Available information includes size, allocated
size, recursive folder counts, timestamps, MIME type, attributes, ownership,
group, permission mode, image dimensions, and the configured default program.

**Previous Versions** is intentionally absent while no snapshot backend is
installed. Aero7 does not show a fake tab.

## Common file dialogs

The `aero7-file-dialog` component supplies Open File, Open Files, Save File,
and Select Folder workflows to Aero7-owned applications. It shares Libraries,
Favorites, Computer, navigation, and file metadata with Explorer.

Third-party applications continue to use their toolkit's dialog unless they
explicitly integrate the Aero7 protocol. System-wide interception is not
claimed.
