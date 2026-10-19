#pragma once

#include <optional>

#include "dlt_proxy/proxy.h"
#include "plugin_utils/configurable_plugin.h"
#include "plugin_utils/settings_form.h"

class DltProxyPlugin : public plugin_utils::ConfigurablePlugin {
    Q_OBJECT
    Q_INTERFACES(QDLTPluginInterface)
    Q_INTERFACES(QDltPluginControlInterface)
    Q_INTERFACES(QDltPluginViewerInterface)
    Q_PLUGIN_METADATA(IID "org.borgsten.DLT.DltProxyPlugin")

 public:
    DltProxyPlugin();

 protected:
    void onEnabledChanged(bool enabled) override;
    void buildSettingsForm(plugin_utils::SettingsForm& form) override;

 private:
    void restart();
    [[nodiscard]] std::optional<dlt_proxy::Options> readOptions() const;

    std::optional<dlt_proxy::Proxy> m_proxy;
};
