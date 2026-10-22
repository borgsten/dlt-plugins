#pragma once

#include <cstdint>
#include <optional>

#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <qdlt/qdltmsg.h>

struct TokenPatterns {
    // Empty matches any.
    QString app_id;
    QString context_id;
    // Matched in the payload; group "name" names the suite or test, group
    // "result" of the end patterns holds its result.
    QString suite_start;
    QString suite_end;
    QString test_start;
    QString test_end;
    // A whole result that counts as passed.
    QString pass;
};

struct Token {
    enum class Kind : std::uint8_t { kSuiteStart, kSuiteEnd, kTestStart, kTestEnd };

    Kind kind;
    QString name;
    // Empty when the end pattern has no result.
    QString result;
    // Whether result is a pass, if there is a result.
    std::optional<bool> passed;
    int index;
};

/*
 * Finds the start and end tokens of test suites and tests in messages. Not
 * for use from more than one thread at a time.
 */
class TokenParser {
 public:
    explicit TokenParser(const TokenPatterns& patterns);

    [[nodiscard]] std::optional<Token> parse(int index, const QDltMsg& msg) const;
    /* The patterns that are not valid regular expressions */
    [[nodiscard]] QStringList errors() const;

 private:
    struct Pattern {
        Token::Kind kind;
        QRegularExpression regex;
    };

    QString app_id_;
    QString context_id_;
    QList<Pattern> patterns_;
    QRegularExpression pass_;
};
