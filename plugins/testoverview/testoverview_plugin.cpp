#include "testoverview_plugin.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include <QCoreApplication>
#include <QDebug>
#include <QDockWidget>
#include <QIcon>
#include <QString>
#include <QStringList>
#include <QTimer>

#include "plugin_utils/plugin_info.h"
#include "plugin_utils/qt_compat.h"

using namespace Qt::StringLiterals;

namespace {

constexpr plugin_utils::PluginInfo kInfo{
    .name = "Test Overview"_L1,
    .version = "0.1.0"_L1,
    .description = "Lists the test suites and tests in the log, with their results"_L1,
    .settings_group = "testoverview"_L1,
};

struct PatternField {
    QLatin1StringView key;
    const char* label;
    QLatin1StringView default_value;
};

const auto kToolbarName = u"mainToolBar"_s;
const auto kPluginsButtonName = u"actionToggle_PluginsEnabled"_s;

constexpr auto kAppIdKey = "appId"_L1;
constexpr auto kContextIdKey = "contextId"_L1;
// As Robot Framework's listener logs them, with dotted names and the result
// after "status:".
constexpr PatternField kSuiteStart{
    .key = "suiteStart"_L1,
    .label = "Suite start",
    .default_value = R"(START SUITE: (?<name>.+?)\s*$)"_L1,
};
constexpr PatternField kSuiteEnd{
    .key = "suiteEnd"_L1,
    .label = "Suite end",
    .default_value = R"(END SUITE: (?<name>.+?)(?:\s+status:\s*(?<result>\S+))?\s*$)"_L1,
};
constexpr PatternField kTestStart{
    .key = "testStart"_L1,
    .label = "Test start",
    .default_value = R"(START TESTCASE: (?<name>.+?)\s*$)"_L1,
};
constexpr PatternField kTestEnd{
    .key = "testEnd"_L1,
    .label = "Test end",
    .default_value = R"(END TESTCASE: (?<name>.+?)(?:\s+status:\s*(?<result>\S+))?\s*$)"_L1,
};
constexpr PatternField kPass{
    .key = "passResult"_L1,
    .label = "Passed result",
    .default_value = "(?i)pass(ed)?"_L1,
};

}  // namespace

TestOverviewPlugin::TestOverviewPlugin() : ConfigurablePlugin(kInfo) {
    connect(&scanner_, &LogScanner::progress, this,
            [this](int percent) { showStatus(tr("Reading the log: %1%").arg(percent)); });
    connect(&scanner_, &LogScanner::finished, this, &TestOverviewPlugin::onScanFinished);
    // The scan reads the viewer's file; stop it while that still exists.
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, [this] { scanner_.stop(); });
}

bool TestOverviewPlugin::initControl(QDltControl* control) {
    control_ = control;
    return true;
}

void TestOverviewPlugin::onMainWindowReady(QWidget* main_window) {
    range_filter_.emplace(main_window);
    toolbar_ = main_window->findChild<QToolBar*>(kToolbarName);
    if (!toolbar_) {
        qWarning() << "Test Overview: toolbar not found:" << kToolbarName;
    }
}

void TestOverviewPlugin::onEnabledChanged(bool enabled) {
    if (!dock_ || !toolbar_) {
        return;
    }
    if (enabled) {
        addButton();
    } else {
        toolbar_->removeAction(&button_);
        setActive(false);
    }
}

void TestOverviewPlugin::initFileStart(QDltFile* file) {
    file_ = file;
    loading_ = true;
    if (active_) {
        scanner_.stop();
        scanning_ = false;
        view_->clear();
        showStatus(tr("Waiting for the log to load"));
    }
}

void TestOverviewPlugin::initFileFinish() {
    loading_ = false;
    if (active_) {
        scan();
    }
}

void TestOverviewPlugin::updateMsgDecoded(int index, QDltMsg& msg) {
    // While a file loads, its messages are in the scan that follows.
    if (!active_ || loading_ || !live_parser_) {
        return;
    }
    std::optional<Token> token = live_parser_->parse(index, msg);
    if (!token) {
        return;
    }
    if (!scanning_) {
        view_->addTokens({*token});
    } else if (index >= scan_end_) {
        live_tokens_.append(std::move(*token));
    }
}

void TestOverviewPlugin::buildSettingsForm(plugin_utils::SettingsForm& form) {
    // Read for each scan; a change rescans.
    const auto rescan = [this](const QString& /*value*/) { scan(); };
    form.addLineEdit({.key = kAppIdKey, .label = tr("App ID"), .placeholder = tr("any")}, rescan);
    form.addLineEdit({.key = kContextIdKey, .label = tr("Context ID"), .placeholder = tr("any")}, rescan);
    for (const PatternField& field : {kSuiteStart, kSuiteEnd, kTestStart, kTestEnd, kPass}) {
        form.addLineEdit(
            {
                .key = field.key,
                .label = tr(field.label),
                .default_value = field.default_value,
                .placeholder = field.default_value,
            },
            rescan);
    }
}

std::unique_ptr<QWidget> TestOverviewPlugin::createView() {
    auto view = std::make_unique<TestTreeView>();
    view_ = view.get();
    connect(view_, &TestTreeView::jumpRequested, this, [this](int index) {
        if (control_) {
            control_->jumpToMsg(index);
        }
    });
    connect(view_, &TestTreeView::showOnlyRequested, this, &TestOverviewPlugin::showOnly);
    connect(view_, &TestTreeView::showAllRequested, this, &TestOverviewPlugin::showAll);
    QTimer::singleShot(0, this, &TestOverviewPlugin::attachToDock);
    return view;
}

void TestOverviewPlugin::attachToDock() {
    QWidget* widget = view_;
    while (widget != nullptr && qobject_cast<QDockWidget*>(widget) == nullptr) {
        widget = widget->parentWidget();
    }
    dock_ = qobject_cast<QDockWidget*>(widget);
    if (!dock_) {
        qWarning() << "Test Overview: dock not found";
        return;
    }
    // Closing a plugin's dock disables the plugin (MyPluginDockWidget); it is
    // hidden with the button instead, which does not close it.
    dock_->setFeatures(dock_->features() & ~QDockWidget::DockWidgetClosable);
    button_.setCheckable(true);
    button_.setText(tr("Tests"));
    button_.setIcon(QIcon(u":/testoverview/tests.png"_s));
    button_.setToolTip(tr("Show the test overview, and analyze the log while it is shown"));
    connect(&button_, &QAction::triggered, this, [this](bool checked) {
        dock_->setVisible(checked);
        if (checked) {
            dock_->raise();
        }
    });
    // Also follows the viewer, which shows and hides the dock by the plugin's mode.
    connect(dock_->toggleViewAction(), &QAction::toggled, this, [this](bool visible) {
        button_.setChecked(visible);
        setActive(visible);
    });
    if (isEnabled()) {
        addButton();
    }
    button_.setChecked(!dock_->isHidden());
    setActive(!dock_->isHidden());
}

void TestOverviewPlugin::addButton() {
    if (!toolbar_) {
        return;
    }
    // Next to the viewer's button for all plugins, else at the end.
    const QList<QAction*> actions = toolbar_->actions();
    const auto plugins =
        std::ranges::find_if(actions, [](const QAction* action) { return action->objectName() == kPluginsButtonName; });
    if (plugins == actions.end()) {
        toolbar_->addAction(&button_);
    } else {
        toolbar_->insertAction(std::next(plugins) == actions.end() ? nullptr : *std::next(plugins), &button_);
    }
}

void TestOverviewPlugin::setActive(bool active) {
    if (active == active_) {
        return;
    }
    active_ = active;
    if (active) {
        if (loading_) {
            showStatus(tr("Waiting for the log to load"));
        } else {
            scan();
        }
        return;
    }
    scanner_.stop();
    scanning_ = false;
    live_parser_.reset();
    live_tokens_.clear();
    view_->clear();
    if (limited_) {
        showAll();
    }
}

void TestOverviewPlugin::scan() {
    if (!active_ || loading_) {
        return;
    }
    const TokenPatterns token_patterns = patterns();
    live_parser_.emplace(token_patterns);
    const QStringList errors = live_parser_->errors();
    pattern_errors_ = errors.isEmpty() ? QString() : tr("Invalid patterns:\n%1").arg(errors.join(u'\n'));
    view_->clear();
    live_tokens_.clear();
    if (file_ == nullptr) {
        scanning_ = false;
        // The viewer passes its file only to enabled plugins, when it loads it;
        // one enabled later gets it from a reload.
        if (control_) {
            showStatus(tr("Reloading the log to read it"));
            control_->reopenFile();
        } else {
            showStatus(tr("Open a log to analyze it"));
        }
        return;
    }
    scan_end_ = file_->size();
    scanning_ = true;
    showStatus(tr("Reading the log"));
    scanner_.start(file_, scan_end_, token_patterns);
}

void TestOverviewPlugin::onScanFinished(const QList<Token>& tokens) {
    scanning_ = false;
    view_->addTokens(tokens);
    view_->addTokens(std::exchange(live_tokens_, {}));
    showStatus({});
}

TokenPatterns TestOverviewPlugin::patterns() const {
    const plugin_utils::PluginSettings& values = settings();
    const auto pattern = [&values](const PatternField& field) {
        return values.value(field.key, QString(field.default_value));
    };
    return {
        .app_id = values.value(kAppIdKey, QString()),
        .context_id = values.value(kContextIdKey, QString()),
        .suite_start = pattern(kSuiteStart),
        .suite_end = pattern(kSuiteEnd),
        .test_start = pattern(kTestStart),
        .test_end = pattern(kTestEnd),
        .pass = pattern(kPass),
    };
}

void TestOverviewPlugin::showOnly(int start, int end) {
    if (!range_filter_) {
        return;
    }
    if (const QString error = range_filter_->limit(start, end); !error.isEmpty()) {
        showStatus(error);
        return;
    }
    limited_ = true;
    // UI Tweaks hides the range, so say that it is on.
    QString status = end >= 0
                         ? tr("The log shows only messages %1 to %2; Show all shows everything.").arg(start).arg(end)
                         : tr("The log shows only messages from %1 on; Show all shows everything.").arg(start);
    if (file_ != nullptr && !IndexRangeFilter::appliesUnpatched(*file_)) {
        status += u'\n' + tr("If the log is not limited: dlt-viewer 2.31 applies the range only while a "
                             "positive or negative filter is active.");
    }
    showStatus(status);
}

void TestOverviewPlugin::showAll() {
    if (range_filter_) {
        range_filter_->clear();
    }
    limited_ = false;
    showStatus({});
}

void TestOverviewPlugin::showStatus(const QString& message) const {
    if (view_) {
        QStringList lines{pattern_errors_, message};
        lines.removeAll(QString());
        view_->setStatus(lines.join(u'\n'));
    }
}
