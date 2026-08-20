# Libraries

Libraries combine one or more real folders into a single Documents, Music,
Pictures, Videos, or custom logical view. Files remain in their original
locations; a Library is an index and navigation definition, not another copy.

## Default Libraries

Aero7 initializes these definitions for each user:

- Documents
- Music
- Pictures
- Videos

Library data is stored at:

```text
~/.config/aero7/libraries.json
```

Explorer and Aero7 common file dialogs read the same versioned database.
Updates use atomic persistence so an interrupted write does not leave a
partially written Library file.

## Opening Library Properties

Open a Library's context menu and choose **Properties**. The Library page shows
its included locations and supports:

- **Include a folder** — add another root;
- **Remove** — stop including the selected root without deleting its files;
- **Set save location** — choose the default destination for new content;
- **Optimize this library for** — Documents, Music, Pictures, Videos, or
  general content;
- **Shown in navigation pane** — control whether the Library appears at left;
- **Restore Defaults** — restore the standard definition;
- **Apply**, **OK**, and **Cancel** — commit or discard changes normally.

## Multiple and offline locations

A Library may contain multiple folders on different local volumes. Offline
roots remain in the definition and become available again when their storage
returns. Search and Library browsing merge all currently visible roots without
silently deleting unavailable entries.

## Save location

The save location is the preferred folder used when an Aero7 application saves
new content to the Library. Changing it does not move existing files. Removing
the current save location requires selecting another valid included folder.

## Repairing a Library

First use **Restore Defaults** in Library Properties. If the per-user database
is damaged beyond normal recovery, close Explorer, back up
`~/.config/aero7/libraries.json`, and allow `aero7-shell-init` to initialize the
default definitions at the next sign-in. Do not edit the JSON while Explorer or
an Aero7 common dialog is writing it.
