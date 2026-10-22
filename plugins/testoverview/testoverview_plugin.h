#pragma once

#include <memory>
#include <optional>

#include <QAction>
#include <QDockWidget>
#include <QList>
#include <QPointer>
#include <QString>
#include <QToolBar>
#include <QWidget>
#include <qdlt/qdltcontrol.h>
#include <qdlt/qdltfile.h>
#include <qdlt/qdltmsg.h>

#include "plugin_utils/configurable_plugin.h"
#include "plugin_utils/settings_form.h"

#include "index_range_filter.h"
#include "log_scanner.h"
#include "test_tree_view.h"
#include "token_parser.h"

/*
 * Active only while its dock is shown, from a toolbar button: it then reads
 * the log on a thread of its own, and follows live messages. Hidden, it does
 * nothing, so the viewer runs as without it.
 */
class TestOverviewPlugin : public plugin_utils::ConfigurablePlugin {
    Q_OBJECT
    Q_INTERFACES(QDLTPluginInterface)
    Q_INTERFACES(QDltPluginControlInterface)
    Q_INTERFACES(QDltPluginViewerInterface)
    Q_PLUGIN_METADATA(IID "org.borgsten.DLT.TestOverviewPlugin")

 public:
    TestOverviewPlugin();

    bool initControl(QDltControl* control) override;
    void initFileStart(QDltFile* file) override;
    void initFileFinish() override;
    void updateMsgDecoded(int index, QDltMsg& msg) override;

 protected:
    void onMainWindowReady(QWidget* main_window) override;
    void onEnabledChanged(bool enabled) override;
    void buildSettingsForm(plugin_utils::SettingsForm& form) override;
    std::unique_ptr<QWidget> createView() override;

 private:
    // The viewer puts the view in a dock after creating it.
    void attachToDock();
    void addButton();
    void setActive(bool active);
    void scan();
    void onScanFinished(const QList<Token>& tokens);
    [[nodiscard]] TokenPatterns patterns() const;
    void showOnly(int start, int end);
    void showAll();
    /* Shown below any pattern errors */
    void showStatus(const QString& message) const;

    QPointer<QDltControl> control_;
    // The viewer's, which outlives the plugin's use of it.
    QDltFile* file_ = nullptr;
    QPointer<TestTreeView> view_;
    QPointer<QDockWidget> dock_;
    QAction button_;
    QPointer<QToolBar> toolbar_;
    std::optional<IndexRangeFilter> range_filter_;
    bool limited_ = false;
    QString pattern_errors_;

    bool active_ = false;
    // Between initFileStart() and initFileFinish(); the scan waits for it.
    bool loading_ = false;
    LogScanner scanner_;
    bool scanning_ = false;
    // Live messages from here on are not in the scan; their tokens follow it.
    int scan_end_ = 0;
    QList<Token> live_tokens_;
    std::optional<TokenParser> live_parser_;
};
