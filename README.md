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
libs/dlt_proxy/      the DLT filter proxy, used by the tool and the dltproxy plugin
plugins/<name>/      one directory per plugin
cmake/               qdlt lookup and dlt_add_plugin()
test/                test programs, e.g. a DLT log source for proxy testing
tools/               standalone programs
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
    .settingsGroup = "myplugin"_L1,
};

MyPlugin::MyPlugin() : ControlPlugin(kInfo) {}
```

Use snake_case file names. Avoid names starting with `ui_`: AUTOUIC treats
`ui_*.h` includes as headers generated from `.ui` files.

## plugin_utils

Shared helpers for plugins, linked by `dlt_add_plugin()`:

- `plugin_utils/plugin_info.h`: `PluginInfo`, the name, version and
  description the viewer shows, and the plugin's settings group.
- `plugin_utils/plugin_settings.h`: `PluginSettings`, the plugin's part of the
  viewer's settings; every key goes under the plugin's settings group.
  `ControlPlugin::settings()` returns it. Viewer settings are only reachable
  explicitly through `QDltSettingsManager`.
- `plugin_utils/settings_form.h`: `SettingsForm`, a panel of checkboxes and
  spin boxes bound to `PluginSettings` keys, in titled or checkable group
  boxes whose keys can go in a settings subgroup.
- `plugin_utils/configurable_plugin.h`: `ConfigurablePlugin`, a
  `ControlPlugin` with a settings dock: it implements the viewer plugin
  interface, and the plugin fills a `SettingsForm` in `buildSettingsForm()`.
  The plugin also declares `Q_INTERFACES(QDltPluginViewerInterface)`.
- `plugin_utils/control_plugin.h`: `ControlPlugin`, a base for control
  plugins that act on the main window. Provides defaults for the plugin
  interfaces and calls `onMainWindowReady()` once at startup and
  `onEnabledChanged()` whenever the plugin becomes enabled or disabled.

## Plugins

- `plugins/uitweaks`: hides main window UI elements that are not needed,
  keeps the app and context ID columns wide enough for their IDs, and extends
  the Recent files/projects/filters menus (10 entries by default). Each tweak
  can be turned off, and the number of Recent entries set, in the plugin's
  dock (plugin mode Show). Active only while the plugin is enabled and plugins
  are globally enabled; mode changes apply on Apply Configuration.
- `plugins/dltproxy`: runs the filter proxy inside the viewer, on its own
  thread, while the plugin is enabled. It connects to the source host and
  port, and once connected listens on the listen port and forwards the
  messages with the configured app IDs (all if empty). Until the first
  client connects, it keeps what it forwards (up to 16 MB). Configure an ECU
  in the project with `localhost` and the listen port: the viewer retries the
  connection every second, so it connects once the proxy listens. Settings are in the plugin's dock (plugin
  mode Show); changes restart the proxy.

## Tools

- `tools/dlt-filter-proxy`: the same proxy as a program, e.g.
  `dlt-filter-proxy --source localhost:3490 --listen 9999 --appid NAV
  --appid MED`. Builds on its own too:
  `cmake -S tools/dlt-filter-proxy -B build/proxy`.
