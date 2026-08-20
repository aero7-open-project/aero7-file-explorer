# Building and Testing

## Source repositories

- Aero7 fork: <https://github.com/aero7-open-project/aero7-file-explorer>
- KDE Dolphin parent: <https://github.com/KDE/dolphin>

Clone the Aero7 fork when building the Aero7 application:

```bash
git clone https://github.com/aero7-open-project/aero7-file-explorer.git
cd aero7-file-explorer
```

## Dependencies

The build uses CMake and Ninja with Qt 6, KDE Frameworks 6, Extra CMake
Modules, Baloo, Solid, and the Aero7 Qt library. The authoritative dependency
set is maintained by the top-level and `src` CMake files and by the reviewed
Arch package recipe.

On Aero7, use the package repository's clean build environment rather than
inventing unreviewed dependency substitutions.

## Configure and build

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DCMAKE_INSTALL_LIBEXECDIR=lib \
  -DBUILD_TESTING=ON
cmake --build build
```

Use a separate build directory. Generated build products do not belong in the
source tree or the GitHub Wiki.

## Focused tests

Run registered tests with:

```bash
ctest --test-dir build --output-on-failure
```

The File Explorer test suite includes contracts for Aero7 window chrome,
location entry, available-screen fitting, Library activation, navigation-pane
width, and stable Details layout.

The installed integration helper runs isolated shell-model, Library,
common-dialog, and local file-operation checks:

```bash
/usr/bin/aero7-shell-selftest
```

Its current contract covers atomic Library persistence, offline and multiple
roots, Library search, all four common-dialog modes, copy, keep-both, replace,
skip, cancellation, insufficient-space rejection, directory-tree move, and
permanent deletion.

## Before publishing a package

1. build from the exact reviewed Git commit;
2. run the focused CTest targets;
3. run `aero7-shell-selftest` in an isolated installed environment;
4. validate package metadata and source locks in `memegeko/aero7-repo`;
5. perform a fresh-install VM pass for release images;
6. record the exact source and package revisions.
