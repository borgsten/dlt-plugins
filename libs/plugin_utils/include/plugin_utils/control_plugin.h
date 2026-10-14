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

namespace plugin_utils {

/*
 * Base for control plugins that act on the main window.
 *
 * Implements the plugin interfaces with defaults and tracks whether the
 * plugin is enabled. A plugin passes its PluginInfo to the constructor and
 * reacts in onMainWindowReady() and onEnabledChanged().
 *
 * The derived class still declares the plugin itself:
 *
 *   class MyPlugin : public plugin_utils::ControlPlugin {
 *       Q_OBJECT
 *       Q_INTERFACES(QDLTPluginInterface)
 *       Q_INTERFACES(QDltPluginControlInterface)
 *       Q_PLUGIN_METADATA(IID "org.example.DLT.MyPlugin")
 *
 *    public:
 *       MyPlugin() : ControlPlugin(kMyPluginInfo) {}
 *       ...
 *   };
 */
class ControlPlugin : public QObject, public QDLTPluginInterface, public QDltPluginControlInterface {
 public:
    explicit ControlPlugin(const PluginInfo& info) : m_info(info) {}

    QString name() final { return m_info.name; }
    QString pluginVersion() final { return m_info.version; }
    QString description() final { return m_info.description; }

    /*
     * Defaults for the interface functions a plugin built on this class does
     * not use.
     */

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
    /* Called once at startup, before the first onEnabledChanged() */
    virtual void onMainWindowReady(QWidget* /*mainWindow*/) {}
    /*
     * Called when the plugin becomes enabled or disabled: at startup if it is
     * enabled, then on Apply Configuration and the global plugin toggle.
     */
    virtual void onEnabledChanged(bool /*enabled*/) {}

    [[nodiscard]] QWidget* mainWindow() const { return m_mainWindow; }
    [[nodiscard]] bool isEnabled() const { return m_enabled; }

    /* Message the viewer shows when one of the interface functions fails */
    void setError(QString error) { m_error = std::move(error); }

 private:
    void syncEnabled();

    PluginInfo m_info;
    QString m_error;
    QPointer<QWidget> m_mainWindow;
    bool m_enabled = false;
};

}  // namespace plugin_utils
