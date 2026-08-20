# Installation and Updates

## Aero7 installations

Current Aero7 installation media includes File Explorer as the desktop's file
manager. A normal fresh installation creates the Aero7 application identity and
the compatibility launch commands automatically.

## Install from the Aero7 package repository

After configuring the signed Aero7 package repository, install or update with:

```bash
sudo pacman -Syu aero7-file-explorer
```

The package declares that it provides `dolphin` and `aero7-dolphin`, conflicts
with the separate packages, and replaces the retired `aero7-dolphin` recipe.
Pacman can therefore perform the supported transition without leaving two
competing default file managers.

The package source is pinned to a reviewed commit in the
[Aero7 package repository](https://github.com/memegeko/aero7-repo). Do not
install an unreviewed binary that merely reuses the Aero7 name.

## After an update

Close existing File Explorer windows and start the application again. If the
desktop shell still holds an older process, sign out and back in. The
`aero7-shell-init` helper checks per-user shell data, including default Library
definitions, at sign-in.

Verify the package with:

```bash
pacman -Q aero7-file-explorer
```

## Compatibility commands

These commands should all resolve to the installed Aero7 application package:

```bash
aero7-file-explorer
dolphin
aero7-dolphin
```

The first command is the canonical executable. The other two preserve existing
shortcuts and integrations.

## Removing the package

Removing File Explorer also removes the compatibility commands owned by its
package. Review reverse dependencies before removal because the Aero7 shell
expects a file-manager provider. Per-user Library definitions remain in the
user profile unless deliberately removed.
