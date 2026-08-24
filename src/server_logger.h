// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QDateTime>
#include <QElapsedTimer>
#include <QFile>
#include <QMutex>
#include <QString>
#include <QStringList>

struct ServerLogOptions {
    bool enabled = true;
    QString directory = QStringLiteral("logs");
    int keepDays = 30;
    bool flushImmediately = true;
};

// Duplicates Qt log messages to one UTF-8 file per server process. The existing
// Qt handler remains responsible for console output; this class only observes it.
class ServerLogger {
public:
    ServerLogger() = default;
    ~ServerLogger();

    ServerLogger(const ServerLogger&) = delete;
    ServerLogger& operator=(const ServerLogger&) = delete;

    // Install before configuration is read. Startup messages are buffered until
    // Configure() decides whether this process should have a log file.
    void InstallEarly();
    void Configure(const ServerLogOptions& options, const QString& version);
    void Close();

    bool IsActive() const;
    QString SessionDirectory() const;
    QString LogFilePath() const;

private:
    static void MessageHandler(QtMsgType type, const QMessageLogContext& context,
                               const QString& message);
    static void ReportInternalError(const QString& message);

    void AppendMessage(QtMsgType type, const QMessageLogContext& context, const QString& message);
    void WriteLineLocked(const QString& line, bool flush);
    void RemoveExpiredSessionsLocked(const QDateTime& now);
    void Uninstall();

    mutable QMutex m_mutex;
    QFile m_file;
    QStringList m_startupMessages;
    QString m_rootDirectory;
    QString m_sessionDirectory;
    QString m_logFilePath;
    QString m_version;
    QDateTime m_startedAt;
    QElapsedTimer m_uptime;
    QtMessageHandler m_previousHandler = nullptr;
    bool m_installed = false;
    bool m_configured = false;
    bool m_enabled = false;
    bool m_flushImmediately = true;
    int m_keepDays = 0;
};
