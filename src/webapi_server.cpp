// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include "webapi_server.h"

#include <QDebug>
#include <QHostAddress>
#include <QHttpServerRequest>
#include <QHttpServerResponder>
#include <QHttpServerResponse>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <webapi_routes_users.h>
#include "bloodborne_ssinfo_reference.h"
#include "webapi_auth.h"
#include "webapi_routes_bloodborne.h"
#include "webapi_routes_bloodborne_bootstrap.h"
#include "webapi_routes_presence.h"
#include "webapi_routes_profile.h"
#include "webapi_routes_session.h"

WebApiServer::WebApiServer(QObject* parent) : QObject(parent) {}
WebApiServer::~WebApiServer() = default;

bool WebApiServer::Start(ConfigManager* config, const QString& dbPath, SharedState* shared) {
    m_config = config;
    m_shared = shared;

    if (m_config->IsBloodborneBootstrapEnabled()) {
        const QUrl publicUrl(m_config->GetBloodbornePublicBaseUrl());
        if (!publicUrl.isValid() || publicUrl.host().isEmpty() ||
            (publicUrl.scheme() != QStringLiteral("http") &&
             publicUrl.scheme() != QStringLiteral("https")) ||
            (!publicUrl.path().isEmpty() && publicUrl.path() != QStringLiteral("/")) ||
            !publicUrl.query().isEmpty() || !publicUrl.fragment().isEmpty()) {
            qCritical().noquote()
                << "Bloodborne bootstrap is enabled, but BloodbornePublicBaseUrl is not a valid "
                   "http(s) base URL:"
                << m_config->GetBloodbornePublicBaseUrl();
            return false;
        }

        QString validationError;
        QByteArray decodedXml;
        if (!Bloodborne::ValidateReferenceServerStatusInfo(&validationError, &decodedXml)) {
            qCritical().noquote() << "Bloodborne bootstrap reference ss.info validation failed:"
                                  << validationError;
            return false;
        }
        qInfo().nospace().noquote()
            << "Bloodborne bootstrap: serving reference Base64 ss.info bytes="
            << Bloodborne::ReferenceServerStatusInfo().size()
            << " decoded_xml_bytes=" << decodedXml.size();
    }

    m_db = std::make_unique<Database>(QStringLiteral("webapi_main"));
    if (!m_db->Open(dbPath)) {
        qCritical() << "WebApiServer: failed to open database at" << dbPath;
        return false;
    }

    m_http = std::make_unique<QHttpServer>(this);
    RegisterRoutes();

    m_tcp = std::make_unique<QTcpServer>(this);

    const QString host = m_config->GetHost();
    const quint16 port = m_config->GetWebApiPort().toUShort();
    if (!m_tcp->listen(QHostAddress(host), port)) {
        qCritical() << "WebApiServer: failed to bind" << host << ":" << port << "—"
                    << m_tcp->errorString();
        return false;
    }

    if (!m_http->bind(m_tcp.get())) {
        qCritical() << "WebApiServer: QHttpServer failed to attach to listener";
        return false;
    }

    qInfo().nospace().noquote() << "WebApiServer listening on: " << host << ":" << port;
    return true;
}

void WebApiServer::RegisterRoutes() {
    m_http->route("/status", [](const QHttpServerRequest&) {
        QJsonObject body;
        body.insert("ok", true);
        body.insert("service", "shadnet-webapi");
        return QHttpServerResponse{"application/json",
                                   QJsonDocument(body).toJson(QJsonDocument::Compact),
                                   QHttpServerResponse::StatusCode::Ok};
    });

    // user routes
    WebApiRoutes::RegisterUserRoutes(*m_http, *m_db, *m_shared);
    WebApiRoutes::RegisterProfileRoutes(*m_http, *m_db, *m_shared);
    WebApiRoutes::RegisterPresenceRoutes(*m_http, *m_db, *m_shared);
    WebApiRoutes::RegisterSessionRoutes(*m_http, *m_db, *m_shared);
    if (m_config->IsBloodborneBootstrapEnabled()) {
        WebApiRoutes::RegisterBloodborneBootstrapRoutes(*m_http, *m_db, *m_shared,
                                                        m_config->GetBloodbornePublicBaseUrl());
    }
    WebApiRoutes::RegisterBloodborneRoutes(*m_http, m_config->IsBloodborneSeamlessCoopEnabled());

    m_http->setMissingHandler(
        this, [](const QHttpServerRequest& req, QHttpServerResponder& responder) {
            qWarning() << "WebAPI: unhandled" << req.method() << req.url().path()
                       << "(query:" << req.url().query() << ")";

            QJsonObject errorObj;
            errorObj.insert("code", static_cast<qint64>(0x80920005));
            errorObj.insert("message", QStringLiteral("Endpoint not implemented"));
            QJsonObject body;
            body.insert("error", errorObj);
            responder.sendResponse(QHttpServerResponse{
                "application/json",
                QJsonDocument(body).toJson(QJsonDocument::Compact),
                QHttpServerResponder::StatusCode::NotFound,
            });
        });
}
