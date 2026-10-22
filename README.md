# dlt-plugins

Out-of-tree plugins for [dlt-viewer](https://github.com/COVESA/dlt-viewer).

## Building

```sh
just build     # debug build in build/debug
just release   # release build in build/release
               # both copy the plugins to build/plugins
just install   # install the release plugins
just format    # clang-format all sources
just lint      # clang-tidy all sources
just clean     # remove build/
```

or with plain CMake:

```sh
cmake -S . -B build
cmake --build build
```

The plugins link against dlt-viewer's `libqdlt` and use the same Qt major
version (detected from `libqdlt`). qdlt is located in this order:

1. `-DDLT_VIEWER_SOURCE_DIR=/path/to/dlt-viewer`: a built checkout; the
   library is taken from `DLT_VIEWER_BUILD_DIR` (default `<source>/build`).
2. pkg-config module `qdlt`.
3. An installed dlt-viewer (`include/qdlt` and `libqdlt`) in the default
   search paths or `CMAKE_PREFIX_PATH`.

Built plugins end up in `<build dir>/out/plugins` (with just, also copied to
`build/plugins`). Either point the viewer at that directory (Settings →
Plugins path) or install them with `cmake --install build` (target directory:
`DLT_PLUGIN_INSTALL_DIR`, by default `<prefix>/lib/dlt-viewer/plugins`) and
point the viewer there; system plugin directories belong to the package.

## CI

`.github/workflows/build.yml` builds libqdlt from dlt-viewer's master (or the
ref given when run by hand) and the plugins against it, with Qt 5 and Qt 6 on
Ubuntu 24.04 and 26.04, warnings as errors. It also runs weekly, to catch
dlt-viewer changes that break the plugins.

## Debug output

Qt sends `qDebug()` output to the systemd journal when stderr is not a
terminal. Run the viewer with `QT_FORCE_STDERR_LOGGING=1` (or check
`journalctl --user`) to see messages such as `Load plugin "UI Tweaks"`.

## Adding a plugin

Create `plugins/<name>/` with a `CMakeLists.txt` that calls

```cmake
dlt_add_plugin(myplugin myplugin.h myplugin.cpp)
```

and add it with `add_subdirectory()` in the root `CMakeLists.txt`.
`dlt_add_plugin()` links `plugin_utils` and qdlt, and adds the plugin's
directory to the include path, so includes are written as `"tweaks/foo.h"`.

The plugin class derives from `plugin_utils::ControlPlugin` and passes its
`PluginInfo`:

```cpp
constexpr plugin_utils::PluginInfo kInfo{
    .name = "My Plugin"_L1,
    .version = "0.1.0"_L1,
    .description = "What it does"_L1,
    .settings_group = "myplugin"_L1,
};

MyPlugin::MyPlugin() : ControlPlugin(kInfo) {}
```

For a settings dock, derive from `plugin_utils::ConfigurablePlugin` instead,
fill the form in `buildSettingsForm()`, and also declare
`Q_INTERFACES(QDltPluginViewerInterface)`; without it the viewer never asks for
the dock.

Use snake_case file names. Avoid names starting with `ui_`: AUTOUIC treats
`ui_*.h` includes as headers generated from `.ui` files.

## plugin_utils

Shared helpers linked into every plugin: plugin metadata and settings, a base
class for plugins that act on the main window, and one that adds a settings
dock built from a form of checkboxes, spin boxes, text fields and combo boxes
bound to the plugin's settings.

## Plugins

- `plugins/uitweaks`: small UI fixes, each can be turned off in its dock
  - hide the filter index range and filter append widgets
  - keep the app and context ID columns wide enough
  - fit the payload column to the table width
  - more entries in the Recent menus
  - dark text on filter colors
  - soft marker color palette with shades in the filter dialog
  - start on the Filter tab
  - unclip the filter dialog's type buttons (temporary, until fixed upstream)
- `plugins/dltproxy`: forwards selected app IDs from a DLT source to a local
  port, for an ECU in the project to connect to

## Tools

- `tools/dlt-filter-proxy`: the proxy as a standalone program; also builds on
  its own with `cmake -S tools/dlt-filter-proxy -B build/proxy`
