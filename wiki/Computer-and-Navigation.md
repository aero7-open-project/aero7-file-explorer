# Computer and Navigation

## Integrated Computer view

The **Computer** node is part of File Explorer. It presents storage as a normal
Explorer location rather than launching a generic Linux mount utility or an
unrelated settings program.

Computer displays user-facing fixed, removable, and optical volumes with:

- the Aero7 drive name and icon;
- drive letter presentation where assigned by the Aero7 shell model;
- total capacity and available space;
- a capacity bar for mounted storage;
- direct navigation into accessible volumes.

## Linux mount filtering

Linux exposes many implementation mounts that are useful to system software
but confusing and unsafe as ordinary Explorer destinations. The Aero7 places
model filters raw runtime, pseudo-filesystem, container, and duplicate mount
entries from the public Favorites, Libraries, and Computer groups.

Filtering changes presentation only. It does not unmount devices, hide files
from administrators, or bypass Linux permissions. Advanced system maintenance
continues to use the appropriate Linux tools.

## Favorites

Favorites provide direct routes to common user locations such as Desktop,
Downloads, and Recent Places. Activating a favorite changes the current view;
it does not create another copy of the folder.

## Network

The Network group exposes locations supported by the installed KIO network
backends. Availability depends on the current network, discovery services,
credentials, and protocol packages. Remote file operations may use KIO's
transport UI where the Aero7 local-operation engine does not apply.

## Missing storage

If a device is absent from Computer:

1. confirm the guest or physical system detects it;
2. wait for the removable device to finish mounting;
3. use Refresh in File Explorer;
4. check whether the volume is intentionally system-only or inaccessible to
   the current user;
5. review system logs or Disk Management for hardware and filesystem errors.

See [Troubleshooting](Troubleshooting) for startup, permissions, and mount
diagnostics.
