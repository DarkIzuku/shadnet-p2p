// SPDX-FileCopyrightText: Copyright 2019-2026 rpcsn Project
// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QLoggingCategory>
#include "bloodborne_website.h"
#include "config.h"
#include "server.h"
#include "server_logger.h"
#include "webapi_server.h"

const QString versionString = QStringLiteral("0.0.13");

int main(int argc, char* argv[]) {
    QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false\n*.info=true\n*.warning=true"));
    qSetMessagePattern(QStringLiteral("%{time yyyy-MM-dd HH:mm:ss.zzz}  "
                                      "%{if-debug}DEBUG%{endif}"
                                      "%{if-info} INFO%{endif}"
                                      "%{if-warning} WARN%{endif}"
                                      "%{if-critical} CRIT%{endif}"
                                      "%{if-fatal}FATAL%{endif}"
                                      "  %{if-category}[%{category}] %{endif}%{message}"));

    QCoreApplication app(argc, argv);
    app.setApplicationName("shadnet");

    // Set working directory to executable location
    QDir::setCurrent(QCoreApplication::applicationDirPath());

    // Install before reading the configuration. This preserves the normal Qt
    // console handler and buffers early startup messages until logging is configured.
    ServerLogger logger;
    logger.InstallEarly();

    qInfo() << "ShadNet Qt server version" << versionString;

    ConfigManager config;
    config.Load();
    config.LoadBannedDomains();

    ServerLogOptions logOptions;
    logOptions.enabled = config.IsServerLogEnabled();
    logOptions.directory = config.GetServerLogDirectory();
    logOptions.keepDays = config.GetServerLogKeepDays();
    logOptions.flushImmediately = config.IsServerLogFlushImmediately();
    logger.Configure(logOptions, versionString);

    ShadNetServer server;
    if (!server.Start(&config)) {
        qCritical() << "Failed to start server";
        return 1;
    }

    // Start the HTTP/JSON WebAPI listener alongside the binary protocol.
    WebApiServer webapi;
    if (!webapi.Start(&config, "db/shadnet.db", &server.Shared())) {
        qWarning() << "WebApiServer failed to start; continuing without WebAPI";
    }

    // The optional community website uses its own listener and only observes
    // authenticated-session events. It does not register game API routes.
    BloodborneWebsiteServer website;
    QObject::connect(&server, &ShadNetServer::ClientAuthenticated, &website,
                     &BloodborneWebsiteServer::OnPlayerAuthenticated);
    QObject::connect(&server, &ShadNetServer::ClientDisconnected, &website,
                     &BloodborneWebsiteServer::OnPlayerDisconnected);
    if (!website.Start(&config, "db/shadnet.db", &server.Shared())) {
        qWarning() << "Bloodborne website failed to start; continuing without website";
    }

    return app.exec();
}
