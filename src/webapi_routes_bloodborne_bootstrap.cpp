// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include "webapi_routes_bloodborne_bootstrap.h"

#include <cstdlib>
#include <memory>
#include <optional>

#include <QDateTime>
#include <QDebug>
#include <QHttpServer>
#include <QHttpServerRequest>
#include <QHttpServerResponse>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMutex>
#include <QMutexLocker>
#include <QString>
#include <QUrlQuery>
#include <QUuid>

#include "bloodborne_bootstrap.h"
#include "client_session.h"
#include "database.h"

namespace WebApiRoutes {
namespace {

struct Identity {
    qint64 userId = 0;
    QString npid;
};

struct BootstrapSession {
    QString sessionId;
    QString npid;
    qint64 charaId = 0;
};

class BootstrapRuntime {
public:
    BootstrapRuntime(Database& db, SharedState& shared) : m_db(db), m_shared(shared) {}

    std::optional<Identity> ResolveIdentity(const QHttpServerRequest& request,
                                            const QString& platformAccountId) const {
        const QHostAddress remote = request.remoteAddress();
        std::optional<Identity> result;
        QReadLocker lock(&m_shared.clientsLock);

        // Prefer the identity carried by Bloodborne when shadPS4 supplies one, but only
        // accept it when the same NPID is already authenticated on shadNet's TCP service.
        if (!platformAccountId.isEmpty()) {
            const auto userIt = m_shared.npidToUserId.constFind(platformAccountId);
            if (userIt != m_shared.npidToUserId.constEnd()) {
                const auto clientIt = m_shared.clients.constFind(*userIt);
                if (clientIt != m_shared.clients.constEnd())
                    return Identity{clientIt.key(), clientIt->npid};
            }
        }

        // Some NP stubs leave PlatformAccountId empty. In that case a unique live TCP
        // client at the same peer address is the only safe correlation available.
        for (auto it = m_shared.clients.constBegin(); it != m_shared.clients.constEnd(); ++it) {
            bool storedV4 = false;
            bool remoteV4 = false;
            const quint32 storedAddress = it->peerAddress.toIPv4Address(&storedV4);
            const quint32 remoteAddress = remote.toIPv4Address(&remoteV4);
            const bool samePeer =
                (storedV4 && remoteV4) ? storedAddress == remoteAddress : it->peerAddress == remote;
            if (!samePeer)
                continue;
            if (result.has_value()) {
                qWarning().noquote()
                    << "Bloodborne bootstrap: more than one authenticated shadNet account uses"
                    << remote.toString() << "so Login cannot be correlated safely";
                return std::nullopt;
            }
            result = Identity{it.key(), it->npid};
        }
        return result;
    }

    std::optional<BootstrapSession> StartSession(const Identity& identity) {
        const QList<qint64> charaIds = m_db.GetOrCreateBloodborneCharaIds(identity.userId, 1);
        if (charaIds.size() != 1)
            return std::nullopt;

        BootstrapSession session;
        session.sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        session.npid = identity.npid;
        session.charaId = charaIds.front();
        QMutexLocker lock(&m_mutex);
        m_sessions.insert(identity.userId, session);
        return session;
    }

    std::optional<BootstrapSession> ValidateSession(const QJsonObject& body,
                                                    const QHttpServerRequest& request) const {
        const QJsonValue userValue = body.value(QStringLiteral("UserId"));
        const QString sessionId = body.value(QStringLiteral("SessionId")).toString();
        if (!userValue.isDouble() || sessionId.isEmpty())
            return std::nullopt;

        const qint64 userId = userValue.toVariant().toLongLong();
        const QString queryUserId =
            QUrlQuery(request.url()).queryItemValue(QStringLiteral("user_id"));
        if (!queryUserId.isEmpty() && queryUserId.toLongLong() != userId)
            return std::nullopt;

        QMutexLocker lock(&m_mutex);
        const auto it = m_sessions.constFind(userId);
        if (it == m_sessions.constEnd() || it->sessionId != sessionId)
            return std::nullopt;
        return *it;
    }

private:
    Database& m_db;
    SharedState& m_shared;
    mutable QMutex m_mutex;
    QHash<qint64, BootstrapSession> m_sessions;
};

bool EnvEnabled(const char* name) {
    const char* value = std::getenv(name);
    return value != nullptr && value[0] != '\0' && QString::fromUtf8(value).trimmed() != "0";
}

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

QJsonObject Redact(const QJsonObject& input) {
    QJsonObject result = input;
    for (auto it = result.begin(); it != result.end(); ++it) {
        const QString key = it.key().toLower();
        if (key.contains(QStringLiteral("password")) || key.contains(QStringLiteral("token")) ||
            key.contains(QStringLiteral("secret")) ||
            key.contains(QStringLiteral("authorizationcode"))) {
            it.value() = QStringLiteral("<redacted>");
        }
    }
    return result;
}

void TraceRequest(const QString& api, const QHttpServerRequest& request,
                  const std::optional<QJsonObject>& body = std::nullopt) {
    if (!EnvEnabled("SHADNET_BLOODBORNE_BOOTSTRAP_TRACE"))
        return;

    QString renderedBody = QStringLiteral("<none>");
    if (body.has_value()) {
        renderedBody =
            QString::fromUtf8(QJsonDocument(Redact(*body)).toJson(QJsonDocument::Compact));
    } else if (!request.body().isEmpty()) {
        renderedBody = QStringLiteral("<invalid JSON; omitted>");
    }
    qInfo().noquote() << "[BLOODBORNE BOOTSTRAP REQUEST]"
                      << "api=" + api << "method=" + MethodName(request.method())
                      << "path=" + request.url().path()
                      << "remote=" + request.remoteAddress().toString() << "body=" + renderedBody;
}

void TraceResponse(const QString& api, QHttpServerResponse::StatusCode status,
                   const QByteArray& body) {
    if (!EnvEnabled("SHADNET_BLOODBORNE_BOOTSTRAP_TRACE"))
        return;
    qInfo().noquote() << "[BLOODBORNE BOOTSTRAP RESPONSE]"
                      << "api=" + api << "status=" + QString::number(static_cast<int>(status))
                      << "body=" + QString::fromUtf8(body);
}

QHttpServerResponse JsonResponse(
    const QString& api, const QJsonObject& object,
    QHttpServerResponse::StatusCode status = QHttpServerResponse::StatusCode::Ok) {
    const QByteArray body = QJsonDocument(object).toJson(QJsonDocument::Compact);
    TraceResponse(api, status, body);
    return QHttpServerResponse{"application/json", body, status};
}

QHttpServerResponse ErrorResponse(const QString& api, QHttpServerResponse::StatusCode status,
                                  const QString& message) {
    QJsonObject body;
    body.insert(QStringLiteral("Error"), message);
    return JsonResponse(api, body, status);
}

std::optional<QJsonObject> ParseRequest(const QHttpServerRequest& request) {
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(request.body(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return std::nullopt;
    return document.object();
}

bool HasNumber(const QJsonObject& body, const char* name) {
    return body.value(QLatin1String(name)).isDouble();
}

bool HasString(const QJsonObject& body, const char* name) {
    return body.value(QLatin1String(name)).isString();
}

bool HasMessageId(const QJsonObject& body, const char* expected) {
    return body.value(QStringLiteral("MessageId")).toString() == QLatin1String(expected);
}

} // namespace

void RegisterBloodborneBootstrapRoutes(QHttpServer& http, Database& db, SharedState& shared,
                                       const QString& publicBaseUrl) {
    const auto runtime = std::make_shared<BootstrapRuntime>(db, shared);
    const QByteArray serverStatusInfo = Bloodborne::BuildServerStatusInfo(publicBaseUrl);

    http.route("/bb-eu/ss.info", QHttpServerRequest::Method::Get,
               [serverStatusInfo](const QHttpServerRequest& request) {
                   TraceRequest(QStringLiteral("ss.info"), request);
                   TraceResponse(QStringLiteral("ss.info"), QHttpServerResponse::StatusCode::Ok,
                                 serverStatusInfo);
                   return QHttpServerResponse{Bloodborne::ServerStatusInfoContentType,
                                              serverStatusInfo,
                                              QHttpServerResponse::StatusCode::Ok};
               });

    http.route(
        "/basic_utils/login", QHttpServerRequest::Method::Post,
        [runtime](const QHttpServerRequest& request) {
            const QString api = QStringLiteral("Login");
            const auto body = ParseRequest(request);
            TraceRequest(api, request, body);
            if (!body || !HasMessageId(*body, "LoginRequest") ||
                !HasString(*body, "PlatformAccountId") || !HasString(*body, "AuthorizationCode") ||
                !HasNumber(*body, "NatType") || !HasNumber(*body, "RegionId") ||
                !HasNumber(*body, "LanguageId") || !HasNumber(*body, "IssuerId") ||
                !HasNumber(*body, "ApplicationVersion")) {
                return ErrorResponse(api, QHttpServerResponse::StatusCode::BadRequest,
                                     QStringLiteral("Invalid LoginRequest"));
            }

            const auto identity = runtime->ResolveIdentity(
                request, body->value(QStringLiteral("PlatformAccountId")).toString());
            if (!identity) {
                return ErrorResponse(
                    api, QHttpServerResponse::StatusCode::Unauthorized,
                    QStringLiteral("No unique authenticated shadNet session for remote"));
            }
            const auto session = runtime->StartSession(*identity);
            if (!session) {
                return ErrorResponse(api, QHttpServerResponse::StatusCode::InternalServerError,
                                     QStringLiteral("Could not allocate character ID"));
            }

            const int languageId = body->value(QStringLiteral("LanguageId")).toInt();
            const QJsonObject response =
                Bloodborne::BuildLoginResponse(identity->userId, languageId, session->sessionId);
            qInfo().noquote()
                << QStringLiteral(
                       "Bloodborne bootstrap: Login successful user=%1 user_id=%2 chara_id=%3")
                       .arg(identity->npid)
                       .arg(identity->userId)
                       .arg(session->charaId);
            return JsonResponse(api, response);
        });

    http.route("/basic_utils/get_datetime", QHttpServerRequest::Method::Post,
               [runtime](const QHttpServerRequest& request) {
                   const QString api = QStringLiteral("ServerTimeGet");
                   const auto body = ParseRequest(request);
                   TraceRequest(api, request, body);
                   if (!body || !HasMessageId(*body, "ServerTimeGetRequest") ||
                       !runtime->ValidateSession(*body, request)) {
                       return ErrorResponse(api, QHttpServerResponse::StatusCode::BadRequest,
                                            QStringLiteral("Invalid ServerTimeGetRequest"));
                   }
                   return JsonResponse(api, Bloodborne::BuildServerTimeResponse());
               });

    http.route(
        "/basic_utils/sync_chara_id", QHttpServerRequest::Method::Post,
        [runtime, &db](const QHttpServerRequest& request) {
            const QString api = QStringLiteral("SyncCharaId");
            const auto body = ParseRequest(request);
            TraceRequest(api, request, body);
            const auto session = body ? runtime->ValidateSession(*body, request) : std::nullopt;
            if (!body || !session || !HasMessageId(*body, "SyncCharaIdRequest") ||
                !HasNumber(*body, "CharaIdNum")) {
                return ErrorResponse(api, QHttpServerResponse::StatusCode::BadRequest,
                                     QStringLiteral("Invalid SyncCharaIdRequest"));
            }

            const int count = body->value(QStringLiteral("CharaIdNum")).toInt();
            if (count <= 0 || count > 16) {
                return ErrorResponse(api, QHttpServerResponse::StatusCode::BadRequest,
                                     QStringLiteral("CharaIdNum is out of range"));
            }
            const qint64 userId = body->value(QStringLiteral("UserId")).toVariant().toLongLong();
            const QList<qint64> charaIds = db.GetOrCreateBloodborneCharaIds(userId, count);
            if (charaIds.size() != count) {
                return ErrorResponse(api, QHttpServerResponse::StatusCode::InternalServerError,
                                     QStringLiteral("Could not allocate character IDs"));
            }
            return JsonResponse(api, Bloodborne::BuildSyncCharaIdResponse(charaIds));
        });

    http.route("/basic_utils/get_normal_notice", QHttpServerRequest::Method::Post,
               [runtime](const QHttpServerRequest& request) {
                   const QString api = QStringLiteral("NoticeNormalGet");
                   const auto body = ParseRequest(request);
                   TraceRequest(api, request, body);
                   if (!body || !runtime->ValidateSession(*body, request) ||
                       !HasMessageId(*body, "NoticeNormalGetRequest") ||
                       !HasNumber(*body, "Language") || !HasNumber(*body, "Region")) {
                       return ErrorResponse(api, QHttpServerResponse::StatusCode::BadRequest,
                                            QStringLiteral("Invalid NoticeNormalGetRequest"));
                   }
                   return JsonResponse(api, Bloodborne::BuildNoticeNormalResponse());
               });

    http.route("/basic_utils/get_emergency_notice", QHttpServerRequest::Method::Post,
               [runtime](const QHttpServerRequest& request) {
                   const QString api = QStringLiteral("NoticeEmergencyGet");
                   const auto body = ParseRequest(request);
                   TraceRequest(api, request, body);
                   if (!body || !runtime->ValidateSession(*body, request) ||
                       !HasMessageId(*body, "NoticeEmergencyGetRequest") ||
                       !HasNumber(*body, "Language") || !HasNumber(*body, "Region") ||
                       !HasString(*body, "CheckTime")) {
                       return ErrorResponse(api, QHttpServerResponse::StatusCode::BadRequest,
                                            QStringLiteral("Invalid NoticeEmergencyGetRequest"));
                   }
                   const QString checkTime = QDateTime::currentDateTimeUtc().toString(
                       QStringLiteral("yyyy-MM-dd'T'HH:mm:ss"));
                   return JsonResponse(api, Bloodborne::BuildNoticeEmergencyResponse(checkTime));
               });

    qInfo().noquote() << "Bloodborne bootstrap routes registered; public base URL" << publicBaseUrl;
}

} // namespace WebApiRoutes
