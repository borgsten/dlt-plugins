#include "test_tree_view.h"

#include <memory>
#include <utility>

#include <QBrush>
#include <QColor>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QToolButton>
#include <QVariant>

#include "plugin_utils/qt_compat.h"

using namespace Qt::StringLiterals;

namespace {

constexpr int kNameColumn = 0;
constexpr int kResultColumn = 1;
constexpr int kTestsColumn = 2;
constexpr int kColumnCount = 3;

constexpr int kStartRole = Qt::UserRole;
constexpr int kEndRole = Qt::UserRole + 1;
// As in the tokens; the shown name drops the suite's.
constexpr int kFullNameRole = Qt::UserRole + 2;

constexpr int kNoEnd = -1;

const QColor kPassedColor(0x2e, 0x7d, 0x32);
const QColor kFailedColor(0xc6, 0x28, 0x28);
const QColor kUnfinishedColor(0xef, 0x6c, 0x00);
const QColor kRunningColor(0x15, 0x65, 0xc0);

bool namesMatch(const QTreeWidgetItem* item, const QString& name) {
    const QString open = item->data(kNameColumn, kFullNameRole).toString();
    return name.isEmpty() || open.isEmpty() || open == name;
}

struct ItemFields {
    QString name;
    QString shown_name;
    int start;
};

// Names may be paths, as in "suite.subsuite.test"; shown below their parent
// without its part.
QString relativeName(const QString& parent, const QString& name) {
    const QString prefix = parent + u'.';
    return !parent.isEmpty() && name.startsWith(prefix) ? name.mid(prefix.size()) : name;
}

std::unique_ptr<QTreeWidgetItem> createItem(const ItemFields& fields) {
    auto item = std::make_unique<QTreeWidgetItem>();
    item->setText(kNameColumn, fields.shown_name);
    item->setData(kNameColumn, kFullNameRole, fields.name);
    item->setData(kNameColumn, kStartRole, fields.start);
    item->setData(kNameColumn, kEndRole, kNoEnd);
    return item;
}

}  // namespace

TestTreeView::TestTreeView() {
    setWindowTitle(tr("Tests"));
    layout_.setContentsMargins(0, 0, 0, 0);

    status_.setWordWrap(true);
    status_.hide();
    layout_.addWidget(&status_);

    tree_.setColumnCount(kColumnCount);
    tree_.setHeaderLabels({tr("Name"), tr("Result"), tr("Tests")});
    tree_.setUniformRowHeights(true);
    tree_.header()->setStretchLastSection(false);
    tree_.header()->setSectionResizeMode(kNameColumn, QHeaderView::Stretch);
    tree_.header()->setSectionResizeMode(kResultColumn, QHeaderView::ResizeToContents);
    tree_.header()->setSectionResizeMode(kTestsColumn, QHeaderView::ResizeToContents);
    tree_.setContextMenuPolicy(Qt::ActionsContextMenu);
    layout_.addWidget(&tree_);

    go_to_start_.setText(tr("Go to start"));
    go_to_end_.setText(tr("Go to end"));
    show_only_.setText(tr("Show only this"));
    show_only_.setToolTip(tr("Limit the log to the messages of this suite or test (index range filter)"));
    show_all_.setText(tr("Show all"));
    show_all_.setToolTip(tr("Remove the index range filter"));
    tree_.addActions({&go_to_start_, &go_to_end_, &show_only_, &show_all_});

    auto buttons = std::make_unique<QHBoxLayout>();
    for (QAction* action : {&go_to_start_, &go_to_end_, &show_only_, &show_all_}) {
        auto button = std::make_unique<QToolButton>();
        button->setDefaultAction(action);
        buttons->addWidget(button.release());
    }
    buttons->addStretch();
    layout_.addLayout(buttons.release());
    layout_.addWidget(&summary_);

    connect(&go_to_start_, &QAction::triggered, this, [this] {
        if (const QTreeWidgetItem* item = currentItem()) {
            emit jumpRequested(item->data(kNameColumn, kStartRole).toInt());
        }
    });
    connect(&go_to_end_, &QAction::triggered, this, [this] {
        if (const QTreeWidgetItem* item = currentItem()) {
            emit jumpRequested(item->data(kNameColumn, kEndRole).toInt());
        }
    });
    connect(&show_only_, &QAction::triggered, this, [this] {
        if (const QTreeWidgetItem* item = currentItem()) {
            emit showOnlyRequested(item->data(kNameColumn, kStartRole).toInt(),
                                   item->data(kNameColumn, kEndRole).toInt());
        }
    });
    connect(&show_all_, &QAction::triggered, this, &TestTreeView::showAllRequested);
    connect(&tree_, &QTreeWidget::itemActivated, &go_to_start_, &QAction::trigger);
    connect(&tree_, &QTreeWidget::currentItemChanged, this, &TestTreeView::updateActions);

    updateActions();
    updateSummary();
}

void TestTreeView::clear() {
    tree_.clear();
    qDeleteAll(std::exchange(new_suites_, {}));
    suite_counts_.clear();
    stale_counts_.clear();
    suites_.clear();
    test_ = nullptr;
    counts_ = {};
    updateActions();
    updateSummary();
}

void TestTreeView::addTokens(const QList<Token>& tokens) {
    if (tokens.isEmpty()) {
        return;
    }
    for (const Token& token : tokens) {
        addToken(token);
    }
    for (QTreeWidgetItem* suite : std::as_const(stale_counts_)) {
        const auto counts = suite_counts_.find(suite);
        updateSuiteCounts(suite, counts != suite_counts_.end() ? counts->second : Counts{});
    }
    stale_counts_.clear();
    tree_.addTopLevelItems(std::exchange(new_suites_, {}));
    updateActions();
    updateSummary();
}

void TestTreeView::setStatus(const QString& status) {
    status_.setText(status);
    status_.setVisible(!status.isEmpty());
}

void TestTreeView::addToken(const Token& token) {
    switch (token.kind) {
        case Token::Kind::kSuiteStart:
            startSuite(token);
            break;
        case Token::Kind::kSuiteEnd:
            endSuite(token);
            break;
        case Token::Kind::kTestStart:
            startTest(token);
            break;
        case Token::Kind::kTestEnd:
            endTest(token);
            break;
    }
}

void TestTreeView::startSuite(const Token& token) {
    abandonTest(token.index);
    // The unnamed suite holds only tests.
    if (!suites_.empty() && isUnnamed(suites_.back())) {
        abandonSuites(suites_.size() - 1, token);
    }
    if (const auto open = openSuite(token.name, Match::kSame)) {
        // Started again without having ended.
        abandonSuites(*open, token);
    } else if (const auto parent = openSuite(token.name, Match::kParent)) {
        abandonSuites(*parent + 1, token);
    }
    QTreeWidgetItem* suite = addItem(token);
    setState(suite, State::kRunning);
    suites_.push_back(suite);
    ++counts_.suites;
}

void TestTreeView::endSuite(const Token& token) {
    std::optional<std::size_t> open = openSuite(token.name, Match::kSame);
    if (!open && !suites_.empty() && isUnnamed(suites_.back()) && !token.name.isEmpty()) {
        // The log started within this suite.
        nameSuite(suites_.back(), token.name);
        ++counts_.suites;
        open = suites_.size() - 1;
    }
    if (!open) {
        return;
    }
    abandonTest(token.index);
    abandonSuites(*open + 1, token);
    QTreeWidgetItem* suite = suites_.back();
    suite->setData(kNameColumn, kEndRole, token.index);
    if (!token.passed) {
        setState(suite, State::kEnded, token.result);
    } else {
        setState(suite, *token.passed ? State::kPassed : State::kFailed, token.result);
    }
    suites_.pop_back();
}

void TestTreeView::startTest(const Token& token) {
    abandonTest(token.index);
    if (const auto parent = openSuite(token.name, Match::kParent)) {
        abandonSuites(*parent + 1, token);
    }
    if (suites_.empty()) {
        auto item = createItem({.name = {}, .shown_name = tr("(no suite)"), .start = token.index});
        suites_.push_back(item.get());
        new_suites_.append(item.release());
    }
    test_ = addItem(token);
    setState(test_, State::kRunning);
    countTest(State::kRunning);
}

void TestTreeView::endTest(const Token& token) {
    if (test_ == nullptr || !namesMatch(test_, token.name)) {
        return;
    }
    test_->setData(kNameColumn, kEndRole, token.index);
    State state = State::kEnded;
    if (token.passed) {
        state = *token.passed ? State::kPassed : State::kFailed;
    }
    setState(test_, state, token.result);
    countTest(state);
    test_ = nullptr;
}

QTreeWidgetItem* TestTreeView::addItem(const Token& token) {
    QTreeWidgetItem* parent = suites_.empty() ? nullptr : suites_.back();
    const QString parent_name = parent != nullptr ? parent->data(kNameColumn, kFullNameRole).toString() : QString();
    auto item = createItem({
        .name = token.name,
        .shown_name = relativeName(parent_name, token.name),
        .start = token.index,
    });
    QTreeWidgetItem* added = item.get();
    if (parent != nullptr) {
        parent->addChild(item.release());
    } else {
        new_suites_.append(item.release());
    }
    return added;
}

std::optional<std::size_t> TestTreeView::openSuite(const QString& name, Match match) const {
    for (std::size_t depth = suites_.size(); depth-- > 0;) {
        const QString open = suites_.at(depth)->data(kNameColumn, kFullNameRole).toString();
        if (match == Match::kSame ? namesMatch(suites_.at(depth), name) && !isUnnamed(suites_.at(depth))
                                  : !open.isEmpty() && name.startsWith(open + u'.')) {
            return depth;
        }
    }
    return std::nullopt;
}

bool TestTreeView::isUnnamed(const QTreeWidgetItem* suite) {
    return suite->data(kNameColumn, kFullNameRole).toString().isEmpty();
}

void TestTreeView::nameSuite(QTreeWidgetItem* suite, const QString& name) {
    suite->setData(kNameColumn, kFullNameRole, name);
    suite->setText(kNameColumn, name);
    for (int i = 0; i < suite->childCount(); ++i) {
        QTreeWidgetItem* test = suite->child(i);
        test->setText(kNameColumn, relativeName(name, test->data(kNameColumn, kFullNameRole).toString()));
    }
}

void TestTreeView::abandonTest(int next_index) {
    if (test_ == nullptr) {
        return;
    }
    // Its messages end where the next suite or test begins.
    test_->setData(kNameColumn, kEndRole, next_index - 1);
    setState(test_, State::kUnfinished);
    countTest(State::kUnfinished);
    test_ = nullptr;
}

void TestTreeView::abandonSuites(std::size_t depth, const Token& next) {
    while (suites_.size() > depth) {
        QTreeWidgetItem* suite = suites_.back();
        suite->setData(kNameColumn, kEndRole, next.index - 1);
        if (!isUnnamed(suite)) {
            setState(suite, State::kUnfinished);
        }
        suites_.pop_back();
    }
}

void TestTreeView::setState(QTreeWidgetItem* item, State state, const QString& result) {
    QString text = result;
    QColor color;
    switch (state) {
        case State::kRunning:
            text = tr("running");
            color = kRunningColor;
            break;
        case State::kPassed:
            color = kPassedColor;
            break;
        case State::kFailed:
            color = kFailedColor;
            break;
        case State::kEnded:
            break;
        case State::kUnfinished:
            text = tr("unfinished");
            color = kUnfinishedColor;
            break;
    }
    item->setText(kResultColumn, text);
    if (color.isValid()) {
        item->setForeground(kResultColumn, color);
    } else {
        item->setData(kResultColumn, Qt::ForegroundRole, QVariant());
    }
    // Items only change from running to their result, so never back from bold.
    if (state == State::kFailed) {
        QFont font = item->font(kResultColumn);
        font.setBold(true);
        item->setFont(kResultColumn, font);
    }
}

void TestTreeView::countTest(State state) {
    countIn(counts_, state);
    for (QTreeWidgetItem* suite = test_->parent(); suite != nullptr; suite = suite->parent()) {
        countIn(suite_counts_.try_emplace(suite).first->second, state);
        stale_counts_.insert(suite);
    }
}

void TestTreeView::countIn(Counts& counts, State state) {
    switch (state) {
        case State::kRunning:
            ++counts.tests;
            break;
        case State::kPassed:
            ++counts.passed;
            break;
        case State::kFailed:
            ++counts.failed;
            break;
        case State::kUnfinished:
            ++counts.unfinished;
            break;
        case State::kEnded:
            break;
    }
}

void TestTreeView::updateSuiteCounts(QTreeWidgetItem* suite, const Counts& counts) {
    QString text = tr("%1/%2 passed").arg(counts.passed).arg(counts.tests);
    if (counts.failed > 0) {
        text += tr(", %1 failed").arg(counts.failed);
    }
    if (counts.unfinished > 0) {
        text += tr(", %1 unfinished").arg(counts.unfinished);
    }
    suite->setText(kTestsColumn, text);
    suite->setForeground(kTestsColumn, counts.failed > 0 ? QBrush(kFailedColor) : QBrush());
}

void TestTreeView::updateSummary() {
    if (counts_.tests == 0 && counts_.suites == 0) {
        summary_.setText(tr("No tests found"));
        return;
    }
    QString text = tr("%1 suites, %2 tests: %3 passed, %4 failed")
                       .arg(counts_.suites)
                       .arg(counts_.tests)
                       .arg(counts_.passed)
                       .arg(counts_.failed);
    if (counts_.unfinished > 0) {
        text += tr(", %1 unfinished").arg(counts_.unfinished);
    }
    summary_.setText(text);
}

void TestTreeView::updateActions() {
    const QTreeWidgetItem* item = currentItem();
    go_to_start_.setEnabled(item != nullptr);
    go_to_end_.setEnabled(item != nullptr && item->data(kNameColumn, kEndRole).toInt() != kNoEnd);
    show_only_.setEnabled(item != nullptr);
}

QTreeWidgetItem* TestTreeView::currentItem() const { return tree_.currentItem(); }
