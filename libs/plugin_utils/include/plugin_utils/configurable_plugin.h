#pragma once

#include <QWidget>
#include <qdlt/plugininterface.h>
#include <qdlt/qdltfile.h>
#include <qdlt/qdltmsg.h>

#include "plugin_utils/control_plugin.h"
#include "plugin_utils/settings_form.h"

namespace plugin_utils {

/*
 * A ControlPlugin with a settings dock, shown while the plugin's mode is
 * "Show". The derived class must also declare
 * Q_INTERFACES(QDltPluginViewerInterface).
 */
class ConfigurablePlugin : public ControlPlugin, public QDltPluginViewerInterface {
 public:
    using ControlPlugin::ControlPlugin;

    QWidget* initViewer() final;

    void initFileStart(QDltFile* /*file*/) override {}
    void initMsg(int /*index*/, QDltMsg& /*msg*/) override {}
    void initMsgDecoded(int /*index*/, QDltMsg& /*msg*/) override {}
    void initFileFinish() override {}
    void updateFileStart() override {}
    void updateMsg(int /*index*/, QDltMsg& /*msg*/) override {}
    void updateMsgDecoded(int /*index*/, QDltMsg& /*msg*/) override {}
    void updateFileFinish() override {}
    void selectedIdxMsg(int /*index*/, QDltMsg& /*msg*/) override {}
    void selectedIdxMsgDecoded(int /*index*/, QDltMsg& /*msg*/) override {}

 protected:
    /* Called after onMainWindowReady() */
    virtual void buildSettingsForm(SettingsForm& form) = 0;
};

}  // namespace plugin_utils
