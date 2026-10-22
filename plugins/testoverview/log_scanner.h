#pragma once

#include <cstdint>
#include <thread>

#include <QList>
#include <QObject>
#include <qdlt/qdltfile.h>

#include "token_parser.h"

/*
 * Finds the tokens in the first messages of the viewer's file on a thread of
 * its own, reporting back on the thread that started it. Messages are read
 * undecoded.
 */
class LogScanner : public QObject {
    Q_OBJECT

 public:
    LogScanner() = default;
    LogScanner(const LogScanner&) = delete;
    LogScanner& operator=(const LogScanner&) = delete;
    LogScanner(LogScanner&&) = delete;
    LogScanner& operator=(LogScanner&&) = delete;
    ~LogScanner() override;

    /* Scans messages 0 to count - 1, after stopping a running scan */
    void start(QDltFile* file, int count, const TokenPatterns& patterns);
    /* Returns once the scan has stopped; it reports nothing more */
    void stop();

 signals:
    void progress(int percent);
    void finished(const QList<Token>& tokens);

 private:
    // Tells reports of a stopped scan, still queued, from those of the current one.
    std::uint64_t generation_ = 0;
    std::jthread thread_;
};
