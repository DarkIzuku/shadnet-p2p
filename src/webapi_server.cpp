// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include "webapi_server.h"

#include <utility>

#include <QDebug>
#include <QHostAddress>
#include <QHttpServerRequest>
#include <QHttpServerResponder>
#include <QHttpServerResponse>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <webapi_routes_users.h>
#include "bloodborne_bootstrap.h"
#include "bloodborne_reference_proxy.h"
#include "bloodborne_ssinfo_reference.h"
#include "webapi_auth.h"
#include "webapi_routes_bloodborne.h"
#include "webapi_routes_bloodborne_bootstrap.h"
#include "webapi_routes_presence.h"
#include "webapi_routes_profile.h"
#include "webapi_routes_session.h"

namespace {

QString MethodName(QHttpServerRequest::Method method) {
    switch (method) {
    case QHttpServerRequest::Method::Get:
        return QStringLiteral("GET");
    case QHttpServerRequest::Method::Post:
        return QStringLiteral("POST");
    case QHttpServerRequest::Method::Put:
        return QStringLiteral("PUT");
    case QHttpServerRequest::Method::Delete:
        return QStringLiteral("DELETE");
    default:
        return QString::number(static_cast<int>(method));
    }
}

} // namespace

WebApiServer::WebApiServer(QObject* parent) : QObject(parent) {}
WebApiServer::~WebApiServer() = default;

bool WebApiServer::Start(ConfigManager* config, const QString& dbPath, SharedState* shared) {
    m_config = config;
    m_shared = shared;

    if (m_config->IsBloodborneReferenceProxyEnabled() &&
        !m_config->IsBloodborneBootstrapEnabled()) {
        qCritical() << "BloodborneReferenceProxyEnabled requires BloodborneBootstrapEnabled=true";
        return false;
    }

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
        if (m_config->IsBloodborneReferenceProxyEnabled()) {
            if (!Bloodborne::ValidateReferenceServerStatusInfo(&validationError, &decodedXml)) {
                qCritical().noquote() << "Bloodborne bootstrap reference ss.info validation failed:"
                                      << validationError;
                return false;
            }
            m_bloodborneServerStatusInfo = Bloodborne::ReferenceServerStatusInfo();
            qInfo().nospace().noquote()
                << "Bloodborne bootstrap: serving reference Base64 ss.info bytes="
                << m_bloodborneServerStatusInfo.size()
                << " decoded_xml_bytes=" << decodedXml.size();
        } else {
            m_bloodborneServerStatusInfo = Bloodborne::BuildServerStatusInfo(
                m_config->GetBloodbornePublicBaseUrl(), &decodedXml, &validationError);
            if (m_bloodborneServerStatusInfo.isEmpty()) {
                qCritical().noquote()
                    << "Bloodborne bootstrap local ss.info generation failed:" << validationError;
                return false;
            }
            qInfo().nospace().noquote()
                << "Bloodborne bootstrap: serving local Base64 ss.info base="
                << m_config->GetBloodbornePublicBaseUrl()
                << " api_count=37 decoded_bytes=" << decodedXml.size()
                << " encoded_bytes=" << m_bloodborneServerStatusInfo.size();
        }

        if (m_config->IsBloodborneWelcomeNoticeEnabled()) {
            qInfo().nospace().noquote()
                << "Bloodborne welcome notice: enabled id=" << Bloodborne::WelcomeNoticeId
                << " title_bytes=" << m_config->GetBloodborneWelcomeNoticeTitle().toUtf8().size()
                << " body_bytes=" << m_config->GetBloodborneWelcomeNoticeBody().toUtf8().size();
        } else {
            qInfo().noquote()
                << "Bloodborne welcome notice: disabled; normal NoticeList remains empty";
        }

        if (m_config->IsBloodborneReferenceProxyEnabled()) {
            const QUrl upstreamUrl(m_config->GetBloodborneReferenceProxyUrl());
            if (!upstreamUrl.isValid() || upstreamUrl.host().isEmpty() ||
                (upstreamUrl.scheme() != QStringLiteral("http") &&
                 upstreamUrl.scheme() != QStringLiteral("https")) ||
                (!upstreamUrl.path().isEmpty() && upstreamUrl.path() != QStringLiteral("/")) ||
                !upstreamUrl.query().isEmpty() || !upstreamUrl.fragment().isEmpty()) {
                qCritical().noquote()
                    << "BloodborneReferenceProxyUrl is not a valid http(s) base URL:"
                    << m_config->GetBloodborneReferenceProxyUrl();
                return false;
            }

            Bloodborne::ReferenceProxy::Options options;
            options.upstreamUrl = upstreamUrl;
            m_bloodborneReferenceProxy =
                std::make_unique<Bloodborne::ReferenceProxy>(std::move(options), this);
            QString proxyError;
            if (!m_bloodborneReferenceProxy->Initialize(&proxyError)) {
                qCritical().noquote()
                    << "Bloodborne reference proxy failed to initialize:" << proxyError;
                return false;
            }
            qWarning().noquote()
                << "Bloodborne reference proxy ENABLED - development capture mode; upstream="
                << upstreamUrl.toString(QUrl::RemovePath | QUrl::StripTrailingSlash);
        }
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
        Bloodborne::WelcomeNotice welcomeNotice;
        welcomeNotice.enabled = m_config->IsBloodborneWelcomeNoticeEnabled();
        welcomeNotice.title = m_config->GetBloodborneWelcomeNoticeTitle();
        welcomeNotice.body = m_config->GetBloodborneWelcomeNoticeBody();
        WebApiRoutes::RegisterBloodborneBootstrapRoutes(
            *m_http, *m_db, *m_shared, m_config->GetBloodbornePublicBaseUrl(),
            m_bloodborneServerStatusInfo, m_config->IsBloodborneReferenceProxyEnabled(),
            welcomeNotice);
    }
    WebApiRoutes::RegisterBloodborneRoutes(*m_http, m_config->IsBloodborneSeamlessCoopEnabled());

    m_http->setMissingHandler(
        this, [this](const QHttpServerRequest& req, QHttpServerResponder& responder) {
            if (m_bloodborneReferenceProxy != nullptr &&
                Bloodborne::ReferenceProxy::IsReferenceBackendPath(req.url().path())) {
                m_bloodborneReferenceProxy->Forward(req, std::move(responder));
                return;
            }

            if (m_config->IsBloodborneBootstrapEnabled() &&
                !m_config->IsBloodborneReferenceProxyEnabled() &&
                Bloodborne::ReferenceProxy::IsReferenceBackendPath(req.url().path())) {
                qWarning().noquote()
                    << "[BLOODBORNE LOCAL UNIMPLEMENTED]"
                    << "method=" + MethodName(req.method()) << "path=" + req.url().path()
                    << "query=" + req.url().query(QUrl::FullyEncoded);
            }

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
