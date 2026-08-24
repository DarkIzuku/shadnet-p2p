// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include "server_logger.h"

#include <algorithm>
#include <cstdio>
#include <utility>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QRegularExpression>

namespace {

QMutex HandlerMutex;
ServerLogger* ActiveLogger = nullptr;

const QRegularExpression SessionDirectoryName(
    QStringLiteral(R"(^\d{4}-\d{2}-\d{2}_\d{2}-\d{2}-\d{2}(?:-\d+)?$)"));

QString Timestamp(const QDateTime& value) {
    return value.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
}

QString Uptime(qint64 milliseconds) {
    const qint64 seconds = milliseconds / 1000;
    return QStringLiteral("%1:%2:%3")
        .arg(seconds / 3600, 2, 10, QLatin1Char('0'))
        .arg((seconds / 60) % 60, 2, 10, QLatin1Char('0'))
        .arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

QString SanitizeForDisk(QString line) {
    // These replacements intentionally affect only the persisted copy. The Qt
    // console handler still receives the original message and retains its format.
    const QRegularExpression jsonSecret(QStringLiteral(
        R"regex((?i)("(?:password|authorizationcode|registrationsecretkey|token|access_token|sessionid|secret|secretkey)"\s*:\s*")[^"]*("))regex"));
    line.replace(jsonSecret, QStringLiteral("\\1<redacted>\\2"));

    const QRegularExpression headerSecret(QStringLiteral(
        R"((?i)\b(?:authorization|proxy-authorization)\s*:\s*[^\s,;]+(?:\s+[^\s,;]+)?)"));
    line.replace(headerSecret, QStringLiteral("Authorization: <redacted>"));

    const QRegularExpression bearerSecret(QStringLiteral(R"((?i)\bbearer\s+[^\s,;]+)"));
    line.replace(bearerSecret, QStringLiteral("Bearer <redacted>"));

    const QRegularExpression keyValueSecret(QStringLiteral(
        R"((?i)\b(password|authorizationcode|registrationsecretkey|token|access_token|sessionid|secret|secretkey)\s*([=:])\s*[^\s,;]+)"));
    line.replace(keyValueSecret, QStringLiteral("\\1\\2<redacted>"));
    return line;
}

bool IsOwnedSessionDirectory(const QFileInfo& directory) {
    if (!directory.isDir() || directory.isSymLink() ||
        !SessionDirectoryName.match(directory.fileName()).hasMatch()) {
        return false;
    }

    QFile logFile(QDir(directory.absoluteFilePath()).filePath(QStringLiteral("shadnet.log")));
    if (!logFile.open(QIODevice::ReadOnly))
        return false;
    return logFile.read(64).startsWith("=== SHADNET SESSION START ===");
}

} // namespace

ServerLogger::~ServerLogger() {
    Close();
    Uninstall();
}

void ServerLogger::InstallEarly() {
    QMutexLocker handlerLock(&HandlerMutex);
    if (m_installed)
        return;

    m_previousHandler = qInstallMessageHandler(&ServerLogger::MessageHandler);
    ActiveLogger = this;
    m_installed = true;
}

void ServerLogger::Configure(const ServerLogOptions& options, const QString& version) {
    QMutexLocker lock(&m_mutex);
    if (m_configured)
        return;

    m_configured = true;
    m_enabled = options.enabled;
    m_flushImmediately = options.flushImmediately;
    m_keepDays = std::max(0, options.keepDays);
    m_version = version;
    if (!m_enabled) {
        m_startupMessages.clear();
        return;
    }

    QString directory = options.directory.trimmed();
    if (directory.isEmpty())
        directory = QStringLiteral("logs");
    const QString rootDirectory =
        QDir::isAbsolutePath(directory)
            ? QDir::cleanPath(directory)
            : QDir(QCoreApplication::applicationDirPath()).filePath(directory);
    QDir root(rootDirectory);
    if (!root.mkpath(QStringLiteral("."))) {
        m_enabled = false;
        m_startupMessages.clear();
        ReportInternalError(QStringLiteral("could not create log directory %1").arg(rootDirectory));
        return;
    }

    m_rootDirectory = root.absolutePath();
    const QDateTime now = QDateTime::currentDateTime();
    RemoveExpiredSessionsLocked(now);

    const QString baseName = now.toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss"));
    QString sessionName = baseName;
    for (int suffix = 1; root.exists(sessionName); ++suffix)
        sessionName = baseName + QStringLiteral("-%1").arg(suffix, 2, 10, QLatin1Char('0'));

    if (!root.mkpath(sessionName)) {
        m_enabled = false;
        m_startupMessages.clear();
        ReportInternalError(
            QStringLiteral("could not create session directory under %1").arg(root.absolutePath()));
        return;
    }

    m_sessionDirectory = root.absoluteFilePath(sessionName);
    m_logFilePath = QDir(m_sessionDirectory).filePath(QStringLiteral("shadnet.log"));
    m_file.setFileName(m_logFilePath);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_enabled = false;
        m_startupMessages.clear();
        ReportInternalError(
            QStringLiteral("could not open %1: %2").arg(m_logFilePath, m_file.errorString()));
        return;
    }

    m_startedAt = now;
    m_uptime.start();
    WriteLineLocked(QStringLiteral("=== SHADNET SESSION START ==="), true);
    WriteLineLocked(QStringLiteral("Started: %1").arg(Timestamp(m_startedAt)), true);
    WriteLineLocked(QStringLiteral("Version: %1").arg(m_version), true);
    WriteLineLocked(QStringLiteral("PID: %1").arg(QCoreApplication::applicationPid()), true);
    WriteLineLocked(QStringLiteral("Log directory: %1").arg(m_sessionDirectory), true);
    WriteLineLocked(QStringLiteral("============================"), true);
    for (const QString& message : std::as_const(m_startupMessages))
        WriteLineLocked(message, m_flushImmediately);
    m_startupMessages.clear();
}

void ServerLogger::Close() {
    QMutexLocker lock(&m_mutex);
    if (!m_file.isOpen())
        return;

    const QDateTime endedAt = QDateTime::currentDateTime();
    WriteLineLocked(QStringLiteral("=== SHADNET SESSION END ==="), true);
    WriteLineLocked(QStringLiteral("Ended: %1").arg(Timestamp(endedAt)), true);
    WriteLineLocked(QStringLiteral("Uptime: %1").arg(Uptime(m_uptime.elapsed())), true);
    WriteLineLocked(QStringLiteral("=========================="), true);
    m_file.flush();
    m_file.close();
}

bool ServerLogger::IsActive() const {
    QMutexLocker lock(&m_mutex);
    return m_file.isOpen();
}

QString ServerLogger::SessionDirectory() const {
    QMutexLocker lock(&m_mutex);
    return m_sessionDirectory;
}

QString ServerLogger::LogFilePath() const {
    QMutexLocker lock(&m_mutex);
    return m_logFilePath;
}

void ServerLogger::MessageHandler(QtMsgType type, const QMessageLogContext& context,
                                  const QString& message) {
    // Keep the global lock until the instance has consumed the message. This
    // prevents a concurrent destructor from uninstalling and freeing it first.
    QMutexLocker handlerLock(&HandlerMutex);
    ServerLogger* logger = nullptr;
    QtMessageHandler previousHandler = nullptr;
    logger = ActiveLogger;
    if (logger != nullptr)
        previousHandler = logger->m_previousHandler;

    if (logger != nullptr)
        logger->AppendMessage(type, context, message);

    handlerLock.unlock();

    if (previousHandler != nullptr) {
        previousHandler(type, context, message);
        return;
    }

    const QByteArray formatted = qFormatLogMessage(type, context, message).toUtf8();
    std::fwrite(formatted.constData(), 1, static_cast<size_t>(formatted.size()), stderr);
    std::fputc('\n', stderr);
    std::fflush(stderr);
    if (type == QtFatalMsg)
        std::abort();
}

void ServerLogger::ReportInternalError(const QString& message) {
    const QByteArray utf8 =
        (QStringLiteral("Server logger: ") + message + QLatin1Char('\n')).toUtf8();
    std::fwrite(utf8.constData(), 1, static_cast<size_t>(utf8.size()), stderr);
    std::fflush(stderr);
}

void ServerLogger::AppendMessage(QtMsgType type, const QMessageLogContext& context,
                                 const QString& message) {
    const QString formatted = SanitizeForDisk(qFormatLogMessage(type, context, message));
    QMutexLocker lock(&m_mutex);
    if (!m_configured) {
        m_startupMessages.append(formatted);
        return;
    }
    if (m_file.isOpen())
        WriteLineLocked(formatted, m_flushImmediately);
}

void ServerLogger::WriteLineLocked(const QString& line, bool flush) {
    if (!m_file.isOpen())
        return;
    m_file.write(line.toUtf8());
    m_file.write("\n");
    if (flush)
        m_file.flush();
}

void ServerLogger::RemoveExpiredSessionsLocked(const QDateTime& now) {
    if (m_keepDays == 0)
        return;

    const QDateTime cutoff = now.addDays(-m_keepDays);
    QDir root(m_rootDirectory);
    if (!root.exists())
        return;
    const QFileInfoList candidates =
        root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
    for (const QFileInfo& candidate : candidates) {
        if (!IsOwnedSessionDirectory(candidate))
            continue;
        const QDateTime started = QDateTime::fromString(candidate.fileName().left(19),
                                                        QStringLiteral("yyyy-MM-dd_HH-mm-ss"));
        if (!started.isValid() || started >= cutoff)
            continue;
        QDir(candidate.absoluteFilePath()).removeRecursively();
    }
}

void ServerLogger::Uninstall() {
    QMutexLocker handlerLock(&HandlerMutex);
    if (!m_installed || ActiveLogger != this)
        return;

    const QtMessageHandler replaced = qInstallMessageHandler(m_previousHandler);
    if (replaced != &ServerLogger::MessageHandler)
        qInstallMessageHandler(replaced);
    ActiveLogger = nullptr;
    m_installed = false;
}
