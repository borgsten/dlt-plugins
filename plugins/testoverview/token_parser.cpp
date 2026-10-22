#include "token_parser.h"

#include "plugin_utils/qt_compat.h"

using namespace Qt::StringLiterals;

namespace {

const auto kNameGroup = u"name"_s;
const auto kResultGroup = u"result"_s;

}  // namespace

TokenParser::TokenParser(const TokenPatterns& patterns)
    : app_id_(patterns.app_id),
      context_id_(patterns.context_id),
      patterns_{
          {.kind = Token::Kind::kSuiteStart, .regex = QRegularExpression(patterns.suite_start)},
          {.kind = Token::Kind::kSuiteEnd, .regex = QRegularExpression(patterns.suite_end)},
          {.kind = Token::Kind::kTestStart, .regex = QRegularExpression(patterns.test_start)},
          {.kind = Token::Kind::kTestEnd, .regex = QRegularExpression(patterns.test_end)},
      },
      pass_(QRegularExpression::anchoredPattern(patterns.pass)) {}

std::optional<Token> TokenParser::parse(int index, const QDltMsg& msg) const {
    if ((!app_id_.isEmpty() && msg.getApid() != app_id_) || (!context_id_.isEmpty() && msg.getCtid() != context_id_)) {
        return std::nullopt;
    }
    // String arguments keep their terminating NUL; the viewer drops it too.
    const QString payload = msg.toStringPayload().remove(QChar::Null);
    for (const Pattern& pattern : patterns_) {
        if (pattern.regex.pattern().isEmpty() || !pattern.regex.isValid()) {
            continue;
        }
        const QRegularExpressionMatch match = pattern.regex.match(payload);
        if (match.hasMatch()) {
            const QString result = match.captured(kResultGroup).trimmed();
            return Token{
                .kind = pattern.kind,
                .name = match.captured(kNameGroup).trimmed(),
                .result = result,
                .passed =
                    result.isEmpty() ? std::nullopt : std::optional(pass_.isValid() && pass_.match(result).hasMatch()),
                .index = index,
            };
        }
    }
    return std::nullopt;
}

QStringList TokenParser::errors() const {
    QStringList errors;
    for (const Pattern& pattern : patterns_) {
        if (!pattern.regex.isValid()) {
            errors.append(u"%1: %2"_s.arg(pattern.regex.pattern(), pattern.regex.errorString()));
        }
    }
    if (!pass_.isValid()) {
        errors.append(u"%1: %2"_s.arg(pass_.pattern(), pass_.errorString()));
    }
    return errors;
}
