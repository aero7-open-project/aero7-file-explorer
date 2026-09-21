/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

namespace Aero7Storage
{
// StorageAccess::isIgnored() also rejects an empty mount path. Before setup,
// that is normal, not a volume's explicit hide/ignore policy. StorageVolume
// retains HintIgnore, x-gdu.hide, swap and backing-file exclusions.
inline bool deviceIgnored(bool hasAccess, bool accessible, bool accessIgnored,
                          bool hasVolume, bool volumeIgnored)
{
    return !hasAccess || (hasVolume && volumeIgnored) || (accessible && accessIgnored);
}

// Presentation policy for an existing native KIO device row. Keeping that row
// retains KIO's asynchronous mount/eject and authorization behavior.
inline bool deviceVisible(bool accessible, bool visibleMount, bool ignored,
                          bool filesystem, bool removable)
{
    if (ignored) return false;
    // An accessible device mounted outside the user's allowed roots must not
    // become visible just because it happens to use a removable transport.
    if (accessible) return visibleMount;
    return filesystem && removable;
}
}
