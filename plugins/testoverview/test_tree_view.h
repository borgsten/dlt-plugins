#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#include <QAction>
#include <QLabel>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

#include "token_parser.h"

/*
 * The test suites and tests found in the log, with their results, and actions
 * to go to them or show only their messages. Suites nest, and contain tests;
 * an end closes the suites and tests still open within. Dotted names are
 * paths: a name starting with an open suite's name and a dot belongs in it.
 * Tests outside a suite go into an unnamed one, which takes the name of a
 * suite end token, as when the log starts within a suite.
 */
class TestTreeView : public QWidget {
    Q_OBJECT

 public:
    TestTreeView();

    void clear();
    void addTokens(const QList<Token>& tokens);
    /* Errors and hints, shown above the tree; empty hides it */
    void setStatus(const QString& status);

 signals:
    void jumpRequested(int index);
    /* end is -1 for a suite or test that is still running */
    void showOnlyRequested(int start, int end);
    void showAllRequested();

 private:
    enum class State : std::uint8_t { kRunning, kPassed, kFailed, kEnded, kUnfinished };
    enum class Match : std::uint8_t { kSame, kParent };

    struct Counts {
        int suites = 0;
        int tests = 0;
        int passed = 0;
        int failed = 0;
        int unfinished = 0;
    };

    void addToken(const Token& token);
    void startSuite(const Token& token);
    void endSuite(const Token& token);
    void startTest(const Token& token);
    void endTest(const Token& token);
    // Below the innermost open suite.
    QTreeWidgetItem* addItem(const Token& token);
    /* The depth of the innermost open suite named name, or its parent */
    [[nodiscard]] std::optional<std::size_t> openSuite(const QString& name, Match match) const;
    static bool isUnnamed(const QTreeWidgetItem* suite);
    // For the unnamed suite, when its end token shows which suite it was.
    static void nameSuite(QTreeWidgetItem* suite, const QString& name);
    // Open ones that will not see their end tokens, before next; suites
    // from depth on.
    void abandonTest(int next_index);
    void abandonSuites(std::size_t depth, const Token& next);
    static void setState(QTreeWidgetItem* item, State state, const QString& result = {});
    // For the current test, in the totals and every suite it is in; running
    // counts it.
    void countTest(State state);
    static void countIn(Counts& counts, State state);
    static void updateSuiteCounts(QTreeWidgetItem* suite, const Counts& counts);
    void updateSummary();
    void updateActions();
    [[nodiscard]] QTreeWidgetItem* currentItem() const;

    // Destroyed before the QWidget base, which then no longer owns them.
    QVBoxLayout layout_{this};
    QLabel status_;
    QTreeWidget tree_;
    QLabel summary_;
    QAction go_to_start_;
    QAction go_to_end_;
    QAction show_only_;
    QAction show_all_;

    // Suites of the batch being added, built before they are in the tree,
    // which makes adding them far cheaper.
    QList<QTreeWidgetItem*> new_suites_;
    // Per suite; suites, tests and the shown counts change in a batch, the
    // shown counts at its end.
    std::unordered_map<QTreeWidgetItem*, Counts> suite_counts_;
    QSet<QTreeWidgetItem*> stale_counts_;
    // Innermost last.
    std::vector<QTreeWidgetItem*> suites_;
    QTreeWidgetItem* test_ = nullptr;
    Counts counts_;
};
