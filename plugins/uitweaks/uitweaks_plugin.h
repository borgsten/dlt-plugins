#pragma once

#include <memory>
#include <utility>
#include <vector>

#include <QString>
#include <QWidget>

#include "plugin_utils/control_plugin.h"

#include "tweaks/itweak.h"

/*
 * Applies its tweaks while the plugin is enabled and reverts them when it is
 * disabled.
 */
class UiTweaksPlugin : public plugin_utils::ControlPlugin {
    Q_OBJECT
    Q_INTERFACES(QDLTPluginInterface)
    Q_INTERFACES(QDltPluginControlInterface)
    Q_PLUGIN_METADATA(IID "org.borgsten.DLT.UiTweaksPlugin")

 public:
    UiTweaksPlugin();

 protected:
    void onMainWindowReady(QWidget* mainWindow) override;
    void onEnabledChanged(bool enabled) override;

 private:
    /*
     * Creates a T from args. tweakId names the tweak in logs and should stay
     * stable, so it can serve as its settings key later.
     */
    template <typename T, typename... Args>
    void addTweak(QString tweakId, Args&&... args) {
        m_tweaks.push_back({std::move(tweakId), std::make_unique<T>(std::forward<Args>(args)...)});
    }

    struct Entry {
        QString id;
        std::unique_ptr<ITweak> tweak;
    };

    std::vector<Entry> m_tweaks;
};
