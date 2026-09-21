# Troubleshooting

## File Explorer does not start

Confirm the package and canonical executable:

```bash
pacman -Q aero7-file-explorer
command -v aero7-file-explorer
aero7-file-explorer
```

Start it from a terminal to capture the first error. Missing shared libraries
usually indicate an incomplete system update or a package-repository mismatch.
Update the complete Aero7 system rather than replacing one library manually.

## An old File Explorer window remains after an update

Close all Explorer windows and reopen them. If a background service remains,
sign out and back in so the D-Bus service, global shortcuts, and shell
initialization restart with the same package version.

## Libraries are empty or missing

1. open Library Properties and confirm the included folders;
2. check that removable or network locations are online;
3. use **Restore Defaults** for a standard Library;
4. inspect `~/.config/aero7/libraries.json` only after closing Explorer and
   Aero7 common dialogs;
5. back up a damaged file before allowing `aero7-shell-init` to recreate the
   defaults.

Do not delete an included real folder when you only intend to remove it from a
Library.

## A drive is missing from Computer

Confirm that Linux detects and mounts the volume, then refresh Explorer. System
pseudo-mounts and raw implementation mounts are intentionally filtered. A
volume that is inaccessible to the current user will not become accessible by
changing Explorer presentation.

## A network location is missing

Network entries depend on the installed KIO protocol backend, discovery
services, connectivity, and credentials. Test the network connection and the
specific backend. Remote operations can display KIO transport UI because the
Aero7 operation layer currently owns local operation workflows.

## Copy or move fails

Check destination free space, write permission, filename validity, filesystem
health, and whether a removable destination disconnected. Use Retry only after
correcting the condition. Skip leaves the failed item unresolved; Cancel stops
the remaining operation.

Explorer does not currently provide a general administrator-elevation
continuation for file-operation permission failures. Use an appropriate system
administration workflow rather than weakening filesystem permissions globally.

## Recycle Bin does not accept an item

Some remote or unusual filesystems cannot provide normal trash semantics. Read
the confirmation carefully before using permanent delete. Restore and Empty
operate on the real KIO trash backend, so permissions and filesystem state can
still produce errors.

## Reporting a bug

### A taskbar pin has no icon or opens the wrong application

Confirm `aero7-file-explorer` is installed, then check the pin targets
`org.aero7.FileExplorer.desktop` with that exact capitalization. Close older
Explorer processes after updating and sign out/in if the shell still holds a
stale launcher. Do not install a second Dolphin package or pin all historical
aliases as a workaround. Report the desktop and Explorer package versions
together when a fresh-install pin is broken.

### Release-test limits

The 5 September [1080p Computer capture](Screenshots) shows clipping in the
lower hardware-summary line in historical package 25.12.3-32. Include the
window size and scale when reporting similar behavior; the gallery preserves
that older observation instead of silently rewriting it.

The later follow-up fixes address
search-popup initialization and shutdown, folder-settings fallback persistence,
Places keyboard focus, accessible control names, and the action-layout lookup
under the Aero7 application identity. File Explorer 25.12.3-55 passed the fresh
21 September online/offline test-candidate taskbar/identity and log checks. A
successful screenshot still is not evidence that every file operation or
physical/removable device works.
Physical/removable devices and remote filesystems also require their own
testing. Refer to the main Aero7 Beta 2 release notes for the release gate.

### What to include

Open an issue in the
[Aero7 File Explorer tracker](https://github.com/aero7-open-project/aero7-file-explorer/issues)
with:

- the Aero7 File Explorer package version;
- the Aero7 and KDE Frameworks versions;
- exact reproduction steps;
- whether the location is local, removable, Library, Computer, trash, or
  remote;
- terminal output and relevant logs;
- a screenshot or short recording when the problem is visual.

Report Aero7-specific behavior to the Aero7 fork. Reproduce a suspected
upstream Dolphin defect before filing it with KDE.
