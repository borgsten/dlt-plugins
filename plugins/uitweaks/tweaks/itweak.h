#pragma once

#include "plugin_utils/settings_form.h"

/*
 * A single change to the viewer UI that can be undone. A tweak with settings
 * takes its PluginSettings as first constructor argument.
 */
class ITweak {
 public:
    ITweak() = default;
    ITweak(const ITweak&) = delete;
    ITweak& operator=(const ITweak&) = delete;
    ITweak(ITweak&&) = delete;
    ITweak& operator=(ITweak&&) = delete;
    virtual ~ITweak() = default;

    virtual void apply() = 0;
    virtual void revert() = 0;
    /* Options shown below the tweak's toggle, bound to its settings */
    virtual void buildSettings(plugin_utils::SettingsForm& form) = 0;
};
