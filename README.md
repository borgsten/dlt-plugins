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
`DLT_PLUGIN_INSTALL_DIR`).

## Layout

```
libs/plugin_utils/   shared helpers, linked into every plugin
plugins/<name>/      one directory per plugin
cmake/               qdlt lookup and dlt_add_plugin()
testapp/             DLT log source for manual testing (not built by default)
```

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
};

MyPlugin::MyPlugin() : ControlPlugin(kInfo) {}
```

Use snake_case file names. Avoid names starting with `ui_`: AUTOUIC treats
`ui_*.h` includes as headers generated from `.ui` files.

## plugin_utils

Shared helpers for plugins, linked by `dlt_add_plugin()`:

- `plugin_utils/plugin_info.h`: `PluginInfo`, the name, version and
  description the viewer shows.
- `plugin_utils/control_plugin.h`: `ControlPlugin`, a base for control
  plugins that act on the main window. Provides defaults for the plugin
  interfaces and calls `onMainWindowReady()` once at startup and
  `onEnabledChanged()` whenever the plugin becomes enabled or disabled.
- `plugin_utils/plugin_state.h`: `isPluginEnabled()`, the plugin's mode
  combined with the global plugin switch.

## Plugins

- `plugins/uitweaks`: hides main window UI elements that are not needed and
  extends the Recent files/projects/filters menus to 10 entries each. Active
  only while the plugin is enabled and plugins are globally enabled; mode
  changes apply on Apply Configuration.
