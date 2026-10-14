#pragma once

#include <QWidget>

#include "plugin_utils/configurable_plugin.h"
#include "plugin_utils/settings_form.h"

#include "tweak_list.h"

class UiTweaksPlugin : public plugin_utils::ConfigurablePlugin {
    Q_OBJECT
    Q_INTERFACES(QDLTPluginInterface)
    Q_INTERFACES(QDltPluginControlInterface)
    Q_INTERFACES(QDltPluginViewerInterface)
    Q_PLUGIN_METADATA(IID "org.borgsten.DLT.UiTweaksPlugin")

 public:
    UiTweaksPlugin();

 protected:
    void onMainWindowReady(QWidget* mainWindow) override;
    void onEnabledChanged(bool enabled) override;
    void buildSettingsForm(plugin_utils::SettingsForm& form) override;

 private:
    TweakList m_tweaks;
};
