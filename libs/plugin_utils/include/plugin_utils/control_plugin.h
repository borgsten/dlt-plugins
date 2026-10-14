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
    explicit ControlPlugin(const PluginInfo& info) : m_info(info), m_settings(info.settingsGroup) {}

    QString name() final { return m_info.name; }
    QString pluginVersion() final { return m_info.version; }
    QString description() final { return m_info.description; }

    /* QDLTPluginInterface */
    QString pluginInterfaceVersion() override { return {PLUGIN_INTERFACE_VERSION}; }
    QString error() override { return m_error; }
    bool loadConfig(QString /*filename*/) override { return true; }
    bool saveConfig(QString /*filename*/) override { return true; }
    QStringList infoConfig() override { return {}; }

    /* QDltPluginControlInterface */
    bool initControl(QDltControl* /*control*/) override { return true; }
    bool initConnections(QStringList /*list*/) override { return true; }
    bool controlMsg(int /*index*/, QDltMsg& /*msg*/) override { return false; }
    bool stateChanged(int /*index*/, QDltConnection::QDltConnectionState /*connectionState*/,
                      QString /*hostname*/) override {
        return true;
    }
    bool autoscrollStateChanged(bool /*enabled*/) override { return true; }
    void initMessageDecoder(QDltMessageDecoder* /*messageDecoder*/) override {}
    void initMainTableView(QTableView* pTableView) override;
    void configurationChanged() override;

 protected:
    /* Called once, before the first onEnabledChanged() */
    virtual void onMainWindowReady(QWidget* /*mainWindow*/) {}
    /* Mode changes reach the plugin on Apply Configuration */
    virtual void onEnabledChanged(bool /*enabled*/) {}

    [[nodiscard]] QWidget* mainWindow() const { return m_mainWindow; }
    [[nodiscard]] bool isEnabled() const { return m_enabled; }
    [[nodiscard]] const PluginSettings& settings() const { return m_settings; }

    void setError(QString error) { m_error = std::move(error); }

 private:
    void syncEnabled();

    PluginInfo m_info;
    PluginSettings m_settings;
    QString m_error;
    QPointer<QWidget> m_mainWindow;
    bool m_enabled = false;
};

}  // namespace plugin_utils
