#pragma once

#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include <QString>

#include "plugin_utils/plugin_settings.h"
#include "plugin_utils/settings_form.h"

#include "tweaks/itweak.h"

/*
 * The plugin's tweaks: applies the enabled ones while the plugin is enabled,
 * and shows a toggle per tweak in the dock, as a group box around the tweak's
 * options if it has any. Each tweak's settings live in a group named after
 * its id.
 */
class TweakList {
 public:
    struct Info {
        QString id;
        QString title;
    };

    explicit TweakList(plugin_utils::PluginSettings settings);

    /* Creates a T from args, preceded by its settings if T takes them */
    template <typename T, typename... Args>
    T& add(Info info, Args&&... args) {
        std::unique_ptr<T> tweak;
        if constexpr (std::is_constructible_v<T, plugin_utils::PluginSettings, Args...>) {
            tweak = std::make_unique<T>(settings_.group(info.id), std::forward<Args>(args)...);
        } else {
            tweak = std::make_unique<T>(std::forward<Args>(args)...);
        }
        T& ref = *tweak;
        entries_.push_back({std::move(info), std::move(tweak)});
        return ref;
    }

    void setEnabled(bool enabled);
    void buildSettingsForm(plugin_utils::SettingsForm& form);

 private:
    struct Entry {
        Info info;
        std::unique_ptr<ITweak> tweak;
    };

    [[nodiscard]] bool isTweakEnabled(const Entry& entry) const;
    void onTweakToggled(const Entry& entry, bool enabled) const;
    static void applyTweak(const Entry& entry);
    static void revertTweak(const Entry& entry);

    plugin_utils::PluginSettings settings_;
    std::vector<Entry> entries_;
    bool enabled_ = false;
};
