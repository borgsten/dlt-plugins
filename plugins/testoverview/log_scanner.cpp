#include "log_scanner.h"

#include <utility>

#include <QMetaObject>
#include <qdlt/qdltmsg.h>

namespace {

constexpr int kPercent = 100;

}  // namespace

LogScanner::~LogScanner() { stop(); }

void LogScanner::start(QDltFile* file, int count, const TokenPatterns& patterns) {
    stop();
    const std::uint64_t generation = ++generation_;
    thread_ = std::jthread([this, file, count, patterns, generation](const std::stop_token& stop_token) {
        // Its regular expressions are used on this thread only.
        const TokenParser parser(patterns);
        QList<Token> tokens;
        QDltMsg msg;
        int reported = -1;
        for (int index = 0; index < count; ++index) {
            if (stop_token.stop_requested()) {
                return;
            }
            // Not getMsg(), whose cache the viewer tunes for its own reads.
            if (file->getMsgNoCache(index, msg)) {
                msg.setIndex(index);
                if (std::optional<Token> token = parser.parse(index, msg)) {
                    tokens.append(std::move(*token));
                }
            }
            if (const int percent = static_cast<int>(static_cast<qint64>(index) * kPercent / count);
                percent != reported) {
                reported = percent;
                QMetaObject::invokeMethod(
                    this,
                    [this, generation, percent] {
                        if (generation == generation_) {
                            emit progress(percent);
                        }
                    },
                    Qt::QueuedConnection);
            }
        }
        QMetaObject::invokeMethod(
            this,
            [this, generation, tokens = std::move(tokens)] {
                if (generation == generation_) {
                    emit finished(tokens);
                }
            },
            Qt::QueuedConnection);
    });
}

void LogScanner::stop() {
    ++generation_;
    if (thread_.joinable()) {
        thread_.request_stop();
        thread_.join();
    }
}
