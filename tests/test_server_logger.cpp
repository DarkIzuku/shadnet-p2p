// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include <iostream>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QRegularExpression>
#include <QTemporaryDir>

#include "server_logger.h"

namespace {

QMutex ConsoleMutex;
QStringList ConsoleMessages;

bool Check(bool condition, const char *expression, int line) {
  if (!condition)
    std::cerr << "check failed at line " << line << ": " << expression << '\n';
  return condition;
}

#define CHECK(expression)                                                      \
  do {                                                                         \
    if (!Check((expression), #expression, __LINE__))                           \
      return 1;                                                                \
  } while (false)

void CaptureConsole(QtMsgType type, const QMessageLogContext &context,
                    const QString &message) {
  QMutexLocker lock(&ConsoleMutex);
  ConsoleMessages.append(qFormatLogMessage(type, context, message));
}

QString ReadFile(const QString &path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    return {};
  return QString::fromUtf8(file.readAll());
}

bool WriteFile(const QString &path, const QByteArray &data) {
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    return false;
  return file.write(data) == data.size();
}

QStringList SessionDirectories(const QString &root) {
  QDir directory(root);
  return directory.entryList(
      QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDir::Name);
}

ServerLogOptions Options(const QString &directory) {
  ServerLogOptions options;
  options.enabled = true;
  options.directory = directory;
  options.keepDays = 0;
  options.flushImmediately = true;
  return options;
}

} // namespace

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);
  qSetMessagePattern(QStringLiteral("%{type} %{message}"));
  const QtMessageHandler priorHandler = qInstallMessageHandler(CaptureConsole);

  QTemporaryDir temporary;
  CHECK(temporary.isValid());

  const QString disabledRoot =
      temporary.filePath(QStringLiteral("disabled-logs"));
  {
    ServerLogger logger;
    logger.InstallEarly();
    ServerLogOptions options = Options(disabledRoot);
    options.enabled = false;
    logger.Configure(options, QStringLiteral("test-version"));
    qInfo().noquote() << "disabled logger remains console-only";
    logger.Close();
  }
  CHECK(!QFileInfo::exists(disabledRoot));

  const QString root = temporary.filePath(QStringLiteral("logs"));
  QString firstSession;
  {
    ServerLogger logger;
    logger.InstallEarly();
    logger.Configure(Options(root), QStringLiteral("test-version"));
    CHECK(logger.IsActive());
    firstSession = logger.SessionDirectory();
    CHECK(QFileInfo(firstSession).isDir());
    CHECK(QFileInfo(logger.LogFilePath()).isFile());

    qDebug().noquote() << "debug logger line";
    qInfo().noquote() << "info logger line";
    qWarning().noquote() << "warning logger line";
    qCritical().noquote() << "critical logger line";
    qInfo().noquote() << "UTF-8: cazadores ¡buena cacería!";
    qInfo().noquote() << "AuthorizationCode=must-not-reach-disk";
    qInfo().noquote()
        << R"({"password":"hidden-password","token":"hidden-token","RegistrationSecretKey":"hidden-key"})";
    qInfo().noquote() << "Authorization: Bearer hidden-bearer";

    // Immediate flushing makes the entry visible before the process exits.
    const QString activeLog = ReadFile(logger.LogFilePath());
    CHECK(activeLog.contains(QStringLiteral("info logger line")));
    CHECK(
        activeLog.contains(QStringLiteral("UTF-8: cazadores ¡buena cacería!")));
    logger.Close();
  }

  const QString firstLog =
      ReadFile(QDir(firstSession).filePath(QStringLiteral("shadnet.log")));
  CHECK(firstLog.contains(QStringLiteral("=== SHADNET SESSION START ===")));
  CHECK(firstLog.contains(QStringLiteral("Version: test-version")));
  CHECK(firstLog.contains(QStringLiteral("debug logger line")));
  CHECK(firstLog.contains(QStringLiteral("info logger line")));
  CHECK(firstLog.contains(QStringLiteral("warning logger line")));
  CHECK(firstLog.contains(QStringLiteral("critical logger line")));
  CHECK(firstLog.contains(QStringLiteral("UTF-8: cazadores ¡buena cacería!")));
  CHECK(firstLog.contains(QStringLiteral("AuthorizationCode=<redacted>")));
  CHECK(!firstLog.contains(QStringLiteral("must-not-reach-disk")));
  CHECK(!firstLog.contains(QStringLiteral("hidden-password")));
  CHECK(!firstLog.contains(QStringLiteral("hidden-token")));
  CHECK(!firstLog.contains(QStringLiteral("hidden-key")));
  CHECK(!firstLog.contains(QStringLiteral("hidden-bearer")));
  CHECK(firstLog.contains(QStringLiteral("=== SHADNET SESSION END ===")));
  CHECK(firstLog.contains(QStringLiteral("Uptime: 00:00:")));

  QString secondSession;
  {
    ServerLogger logger;
    logger.InstallEarly();
    logger.Configure(Options(root), QStringLiteral("test-version"));
    secondSession = logger.SessionDirectory();
    qInfo().noquote() << "second logger session";
    logger.Close();
  }
  CHECK(firstSession != secondSession);
  CHECK(SessionDirectories(root).size() == 2);
  const QRegularExpression sessionName(
      QStringLiteral(R"(^\d{4}-\d{2}-\d{2}_\d{2}-\d{2}-\d{2}(?:-\d+)?$)"));
  CHECK(sessionName.match(QFileInfo(firstSession).fileName()).hasMatch());
  CHECK(sessionName.match(QFileInfo(secondSession).fileName()).hasMatch());

  const QString oldOwned =
      QDir(root).filePath(QStringLiteral("2000-01-01_00-00-00"));
  const QString oldExternalNamed =
      QDir(root).filePath(QStringLiteral("2000-01-02_00-00-00"));
  const QString external = QDir(root).filePath(QStringLiteral("do-not-delete"));
  CHECK(QDir().mkpath(oldOwned));
  CHECK(QDir().mkpath(oldExternalNamed));
  CHECK(QDir().mkpath(external));
  CHECK(WriteFile(QDir(oldOwned).filePath(QStringLiteral("shadnet.log")),
                  "=== SHADNET SESSION START ===\nold owned logger\n"));
  CHECK(
      WriteFile(QDir(oldExternalNamed).filePath(QStringLiteral("shadnet.log")),
                "external file without logger marker\n"));
  CHECK(WriteFile(QDir(external).filePath(QStringLiteral("notes.txt")),
                  "keep this\n"));

  {
    ServerLogger logger;
    logger.InstallEarly();
    ServerLogOptions options = Options(root);
    options.keepDays = 1;
    logger.Configure(options, QStringLiteral("test-version"));
    logger.Close();
  }
  CHECK(!QFileInfo::exists(oldOwned));
  CHECK(QFileInfo::exists(oldExternalNamed));
  CHECK(QFileInfo::exists(external));

  QString console;
  {
    QMutexLocker lock(&ConsoleMutex);
    console = ConsoleMessages.join(QLatin1Char('\n'));
  }
  CHECK(console.contains(QStringLiteral("info logger line")));
  CHECK(console.contains(QStringLiteral("warning logger line")));
  CHECK(console.contains(QStringLiteral("critical logger line")));

  qInstallMessageHandler(priorHandler);
  return 0;
}
