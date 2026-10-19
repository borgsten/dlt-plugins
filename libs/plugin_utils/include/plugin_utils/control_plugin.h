#pragma once

#include <utility>

#include <QObject>
#include <QPointer>
#include <QWidget>
#include <qdlt/plugininterface.h>
#include <qdlt/qdltconnection.h>
#include <qdlt/qdltcontrol.h>
#include <qdlt/qdltmessagedecoder.h>
#include <qdlt/qdltmsg.h>

#include "plugin_utils/plugin_info.h"
#include "plugin_utils/plugin_settings.h"

namespace plugin_utils {

/*
 * Base for control plugins that act on the main window: defaults for the
 * plugin interfaces and tracking of whether the plugin is enabled.
 */
class ControlPlugin : public QObject, public QDLTPluginInterface, public QDltPluginControlInterface {
 public:
    explicit ControlPlugin(const PluginInfo& info) : info_(info), settings_(info.settings_group) {}

    QString name() final { return info_.name; }
    QString pluginVersion() final { return info_.version; }
    QString description() final { return info_.description; }

    /* QDLTPluginInterface */
    QString pluginInterfaceVersion() override { return {PLUGIN_INTERFACE_VERSION}; }
    QString error() override { return error_; }
    bool loadConfig(QString /*filename*/) override { return true; }
    bool saveConfig(QString /*filename*/) override { return true; }
    QStringList infoConfig() override { return {}; }

    /* QDltPluginControlInterface */
    bool initControl(QDltControl* /*control*/) override { return true; }
    bool initConnections(QStringList /*list*/) override { return true; }
    bool controlMsg(int /*index*/, QDltMsg& /*msg*/) override { return false; }
    bool stateChanged(int /*index*/, QDltConnection::QDltConnectionState /*connection_state*/,
                      QString /*hostname*/) override {
        return true;
    }
    bool autoscrollStateChanged(bool /*enabled*/) override { return true; }
    void initMessageDecoder(QDltMessageDecoder* /*message_decoder*/) override {}
    void initMainTableView(QTableView* p_table_view) override;
    void configurationChanged() override;

 protected:
    /* Called once, before the first onEnabledChanged() */
    virtual void onMainWindowReady(QWidget* /*main_window*/) {}
    /* Mode changes reach the plugin on Apply Configuration */
    virtual void onEnabledChanged(bool /*enabled*/) {}

    [[nodiscard]] QWidget* mainWindow() const { return main_window_; }
    [[nodiscard]] bool isEnabled() const { return enabled_; }
    [[nodiscard]] const PluginSettings& settings() const { return settings_; }

    void setError(QString error) { error_ = std::move(error); }

 private:
    void syncEnabled();

    PluginInfo info_;
    PluginSettings settings_;
    QString error_;
    QPointer<QWidget> main_window_;
    bool enabled_ = false;
};

}  // namespace plugin_utils
