// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include "webapi_routes_bloodborne_bootstrap.h"

#include <array>
#include <cstdlib>
#include <functional>
#include <memory>
#include <optional>

#include <QCryptographicHash>
#include <QDebug>
#include <QHttpServer>
#include <QHttpServerRequest>
#include <QHttpServerResponse>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMutex>
#include <QMutexLocker>
#include <QString>
#include <QUrlQuery>
#include <QUuid>

#include "bloodborne_bootstrap.h"
#include "bloodborne_ssinfo_reference.h"
#include "client_session.h"
#include "database.h"

namespace WebApiRoutes {
namespace {

struct Identity {
    qint64 userId = 0;
    QString npid;
};

struct BootstrapSession {
    qint64 userId = 0;
    QString sessionId;
    QString npid;
    QString platformAccountId;
    int languageId = 0;
    int regionId = 0;
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

    std::optional<BootstrapSession> StartSession(const Identity& identity,
                                                 const QString& platformAccountId, int languageId,
                                                 int regionId) {
        const QList<qint64> charaIds = m_db.GetOrCreateBloodborneCharaIds(identity.userId, 1);
        if (charaIds.size() != 1)
            return std::nullopt;

        BootstrapSession session;
        session.userId = identity.userId;
        session.sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        session.npid = identity.npid;
        session.platformAccountId = platformAccountId;
        session.languageId = languageId;
        session.regionId = regionId;
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
        if (!queryUserId.isEmpty()) {
            bool queryValid = false;
            const qint64 parsedQueryUserId = queryUserId.toLongLong(&queryValid);
            if (!queryValid || parsedQueryUserId != userId)
                return std::nullopt;
        }

        QMutexLocker lock(&m_mutex);
        const auto it = m_sessions.constFind(userId);
        if (it == m_sessions.constEnd() || it->userId != userId || it->sessionId != sessionId)
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

void TraceRequest(const QString& api, const QHttpServerRequest& request,
                  const std::optional<QJsonObject>& body = std::nullopt) {
    if (!EnvEnabled("SHADNET_BLOODBORNE_BOOTSTRAP_TRACE"))
        return;

    const qint64 userId = body.has_value() && body->value(QStringLiteral("UserId")).isDouble()
                              ? body->value(QStringLiteral("UserId")).toVariant().toLongLong()
                              : 0;
    qInfo().noquote() << "[BLOODBORNE LOCAL REQUEST]"
                      << "api=" + api << "user_id=" + QString::number(userId)
                      << "method=" + MethodName(request.method()) << "path=" + request.url().path();

    if (!body.has_value())
        return;
    static const std::array<const char*, 4> BlobFields{"BloodData", "TombData",
                                                       "WanderingGhostData", "ShellData"};
    for (const char* field : BlobFields) {
        const QJsonValue value = body->value(QLatin1String(field));
        if (!value.isString())
            continue;
        const QByteArray bytes = value.toString().toUtf8();
        const QByteArray hash =
            QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex().toUpper();
        qInfo().noquote() << "[BLOODBORNE LOCAL BLOB]"
                          << "api=" + api << "field=" + QLatin1String(field)
                          << "size=" + QString::number(bytes.size())
                          << "sha256=" + QString::fromLatin1(hash);
    }
}

void TraceResponse(const QString& api, QHttpServerResponse::StatusCode status,
                   const QJsonObject& body) {
    if (!EnvEnabled("SHADNET_BLOODBORNE_BOOTSTRAP_TRACE"))
        return;
    const QJsonValue resKind = body.value(QStringLiteral("ResKind"));
    qInfo().noquote() << "[BLOODBORNE LOCAL RESPONSE]"
                      << "api=" + api << "status=" + QString::number(static_cast<int>(status))
                      << "res_kind=" + (resKind.isDouble() ? QString::number(resKind.toInt())
                                                           : QStringLiteral("<none>"));
}

QHttpServerResponse JsonResponse(
    const QString& api, const QJsonObject& object,
    QHttpServerResponse::StatusCode status = QHttpServerResponse::StatusCode::Ok) {
    const QByteArray body = QJsonDocument(object).toJson(QJsonDocument::Compact);
    TraceResponse(api, status, object);
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

bool HasArray(const QJsonObject& body, const char* name) {
    return body.value(QLatin1String(name)).isArray();
}

bool HasMessageId(const QJsonObject& body, const char* expected) {
    return body.value(QStringLiteral("MessageId")).toString() == QLatin1String(expected);
}

bool HasSessionFields(const QJsonObject& body) {
    return HasNumber(body, "UserId") && HasString(body, "SessionId") &&
           !body.value(QStringLiteral("SessionId")).toString().isEmpty();
}

QJsonObject SuccessResponse(const char* messageId) {
    QJsonObject response;
    response.insert(QStringLiteral("MessageId"), QLatin1String(messageId));
    response.insert(QStringLiteral("ResKind"), 0);
    return response;
}

QJsonObject EmptyListResponse(const char* messageId, std::initializer_list<const char*> listNames) {
    QJsonObject response = SuccessResponse(messageId);
    for (const char* name : listNames)
        response.insert(QLatin1String(name), QJsonArray{});
    return response;
}

using RequestValidator = std::function<bool(const QJsonObject&)>;
using ResponseBuilder = std::function<QJsonObject(const QJsonObject&)>;

QHttpServerResponse HandleSessionEndpoint(const std::shared_ptr<BootstrapRuntime>& runtime,
                                          const QString& api, const char* requestMessageId,
                                          const QHttpServerRequest& request,
                                          const RequestValidator& validate,
                                          const ResponseBuilder& buildResponse) {
    const auto body = ParseRequest(request);
    TraceRequest(api, request, body);
    if (!body || !HasMessageId(*body, requestMessageId) || !HasSessionFields(*body) ||
        !validate(*body)) {
        return ErrorResponse(api, QHttpServerResponse::StatusCode::BadRequest,
                             QStringLiteral("Invalid %1").arg(QLatin1String(requestMessageId)));
    }
    if (!runtime->ValidateSession(*body, request)) {
        return ErrorResponse(api, QHttpServerResponse::StatusCode::Unauthorized,
                             QStringLiteral("Unknown Bloodborne session"));
    }
    return JsonResponse(api, buildResponse(*body));
}

} // namespace

void RegisterBloodborneBootstrapRoutes(QHttpServer& http, Database& db, SharedState& shared,
                                       const QString& publicBaseUrl,
                                       const QByteArray& serverStatusInfo,
                                       bool referenceProxyEnabled,
                                       const Bloodborne::WelcomeNotice& welcomeNotice) {
    http.route("/bb-eu/ss.info", QHttpServerRequest::Method::Get,
               [serverStatusInfo](const QHttpServerRequest& request) {
                   TraceRequest(QStringLiteral("ss.info"), request);
                   TraceResponse(QStringLiteral("ss.info"), QHttpServerResponse::StatusCode::Ok,
                                 QJsonObject{});
                   return QHttpServerResponse{Bloodborne::ServerStatusInfoContentType,
                                              serverStatusInfo,
                                              QHttpServerResponse::StatusCode::Ok};
               });

    if (referenceProxyEnabled) {
        qInfo() << "Bloodborne bootstrap: local backend routes disabled for reference proxy mode";
        return;
    }

    const auto runtime = std::make_shared<BootstrapRuntime>(db, shared);

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
            const QString platformAccountId =
                body->value(QStringLiteral("PlatformAccountId")).toString();
            const int languageId = body->value(QStringLiteral("LanguageId")).toInt();
            const int regionId = body->value(QStringLiteral("RegionId")).toInt();
            const auto session =
                runtime->StartSession(*identity, platformAccountId, languageId, regionId);
            if (!session) {
                return ErrorResponse(api, QHttpServerResponse::StatusCode::InternalServerError,
                                     QStringLiteral("Could not allocate character ID"));
            }

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
                       !HasSessionFields(*body)) {
                       return ErrorResponse(api, QHttpServerResponse::StatusCode::BadRequest,
                                            QStringLiteral("Invalid ServerTimeGetRequest"));
                   }
                   if (!runtime->ValidateSession(*body, request))
                       return ErrorResponse(api, QHttpServerResponse::StatusCode::Unauthorized,
                                            QStringLiteral("Unknown Bloodborne session"));
                   return JsonResponse(api, Bloodborne::BuildServerTimeResponse());
               });

    http.route(
        "/basic_utils/sync_chara_id", QHttpServerRequest::Method::Post,
        [runtime, &db](const QHttpServerRequest& request) {
            const QString api = QStringLiteral("SyncCharaId");
            const auto body = ParseRequest(request);
            TraceRequest(api, request, body);
            if (!body || !HasMessageId(*body, "SyncCharaIdRequest") || !HasSessionFields(*body) ||
                !HasNumber(*body, "CharaIdNum")) {
                return ErrorResponse(api, QHttpServerResponse::StatusCode::BadRequest,
                                     QStringLiteral("Invalid SyncCharaIdRequest"));
            }
            if (!runtime->ValidateSession(*body, request))
                return ErrorResponse(api, QHttpServerResponse::StatusCode::Unauthorized,
                                     QStringLiteral("Unknown Bloodborne session"));

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
               [runtime, welcomeNotice](const QHttpServerRequest& request) {
                   const QString api = QStringLiteral("NoticeNormalGet");
                   const auto body = ParseRequest(request);
                   TraceRequest(api, request, body);
                   if (!body || !HasMessageId(*body, "NoticeNormalGetRequest") ||
                       !HasSessionFields(*body) || !HasNumber(*body, "Language") ||
                       !HasNumber(*body, "Region")) {
                       return ErrorResponse(api, QHttpServerResponse::StatusCode::BadRequest,
                                            QStringLiteral("Invalid NoticeNormalGetRequest"));
                   }
                   if (!runtime->ValidateSession(*body, request))
                       return ErrorResponse(api, QHttpServerResponse::StatusCode::Unauthorized,
                                            QStringLiteral("Unknown Bloodborne session"));
                   return JsonResponse(api,
                                       Bloodborne::BuildNoticeNormalResponse(welcomeNotice));
               });

    http.route("/basic_utils/get_emergency_notice", QHttpServerRequest::Method::Post,
               [runtime](const QHttpServerRequest& request) {
                   const QString api = QStringLiteral("NoticeEmergencyGet");
                   const auto body = ParseRequest(request);
                   TraceRequest(api, request, body);
                   if (!body || !HasMessageId(*body, "NoticeEmergencyGetRequest") ||
                       !HasSessionFields(*body) || !HasNumber(*body, "Language") ||
                       !HasNumber(*body, "Region") || !HasString(*body, "CheckTime")) {
                       return ErrorResponse(api, QHttpServerResponse::StatusCode::BadRequest,
                                            QStringLiteral("Invalid NoticeEmergencyGetRequest"));
                   }
                   if (!runtime->ValidateSession(*body, request))
                       return ErrorResponse(api, QHttpServerResponse::StatusCode::Unauthorized,
                                            QStringLiteral("Unknown Bloodborne session"));
                   const QString checkTime = body->value(QStringLiteral("CheckTime")).toString();
                   return JsonResponse(api, Bloodborne::BuildNoticeEmergencyResponse(checkTime));
               });

    http.route("/penalty/check_user_priority_move_count", QHttpServerRequest::Method::Post,
               [runtime](const QHttpServerRequest& request) {
                   return HandleSessionEndpoint(
                       runtime, QStringLiteral("UserPropertiesMoveCountCheck"),
                       "UserPropertiesMoveCountCheckRequest", request,
                       [](const QJsonObject& body) { return HasNumber(body, "Count"); },
                       [](const QJsonObject&) {
                           return SuccessResponse("UserPropertiesMoveCountCheckResponse");
                       });
               });

    http.route(
        "/blood_messenger/exist_messages", QHttpServerRequest::Method::Post,
        [runtime](const QHttpServerRequest& request) {
            return HandleSessionEndpoint(
                runtime, QStringLiteral("BloodMessSearchAdd"), "BloodMessSearchAddRequest", request,
                [](const QJsonObject& body) {
                    return HasArray(body, "BloodMessIdList") && HasNumber(body, "CharaId");
                },
                [](const QJsonObject&) {
                    return EmptyListResponse("BloodMessSearchAddResponse",
                                             {"BloodMessEvaluationList", "LostBloodMessIdList"});
                });
        });

    http.route(
        "/messenger_shell/upload", QHttpServerRequest::Method::Post,
        [runtime](const QHttpServerRequest& request) {
            return HandleSessionEndpoint(
                runtime, QStringLiteral("MessengerShellUpload"), "MessengerShellUploadRequest",
                request,
                [](const QJsonObject& body) {
                    return HasNumber(body, "CharaId") && HasString(body, "ShellData") &&
                           HasNumber(body, "ShellDataVersion");
                },
                [](const QJsonObject&) { return SuccessResponse("MessengerShellUploadResponse"); });
        });

    http.route(
        "/wandering_ghost/get", QHttpServerRequest::Method::Post,
        [runtime](const QHttpServerRequest& request) {
            return HandleSessionEndpoint(
                runtime, QStringLiteral("WanderingGhostGet"), "WanderingGhostGetRequest", request,
                [](const QJsonObject& body) {
                    return HasArray(body, "AreaList") && HasNumber(body, "GetMaxCount") &&
                           HasArray(body, "JoinedCharaIdList") &&
                           HasNumber(body, "MatchingLevel") &&
                           HasNumber(body, "WanderingGhostDataVersion");
                },
                [](const QJsonObject&) {
                    return EmptyListResponse("WanderingGhostGetResponse", {"WanderingGhostList"});
                });
        });

    http.route("/chair_messenger/get", QHttpServerRequest::Method::Post,
               [runtime](const QHttpServerRequest& request) {
                   return HandleSessionEndpoint(
                       runtime, QStringLiteral("ChairMessGetList"), "ChairMessGetListRequest",
                       request,
                       [](const QJsonObject& body) {
                           return HasNumber(body, "CharaId") && HasArray(body, "WarpInfoList");
                       },
                       [](const QJsonObject&) {
                           return EmptyListResponse("ChairMessGetListResponse", {"ChairMessList"});
                       });
               });

    http.route("/channel/get_info", QHttpServerRequest::Method::Post,
               [runtime](const QHttpServerRequest& request) {
                   return HandleSessionEndpoint(
                       runtime, QStringLiteral("ChannelGetInfo"), "ChannelGetInfoRequest", request,
                       [](const QJsonObject& body) { return HasArray(body, "ChannelIdList"); },
                       [](const QJsonObject&) {
                           return EmptyListResponse("ChannelGetInfoResponse",
                                                    {"ChannelInfoList", "LostChannelIdList"});
                       });
               });

    http.route("/blood_messenger/evaluation", QHttpServerRequest::Method::Post,
               [runtime](const QHttpServerRequest& request) {
                   return HandleSessionEndpoint(
                       runtime, QStringLiteral("BloodMessGetEvaluate"),
                       "BloodMessGetEvaluateRequest", request,
                       [](const QJsonObject& body) { return HasArray(body, "BloodMessIdList"); },
                       [](const QJsonObject&) {
                           return EmptyListResponse("BloodMessGetEvaluateResponse",
                                                    {"BloodMessEvaluationList"});
                       });
               });

    http.route(
        "/blood_messenger/message_area", QHttpServerRequest::Method::Post,
        [runtime](const QHttpServerRequest& request) {
            return HandleSessionEndpoint(
                runtime, QStringLiteral("BloodMessGetList"), "BloodMessGetListRequest", request,
                [](const QJsonObject& body) {
                    return HasArray(body, "AreaInfoList") && HasNumber(body, "BloodDataVersion") &&
                           HasNumber(body, "CharaDataVersion") && HasNumber(body, "GetMaxCount") &&
                           HasNumber(body, "MessShellInfoVersion");
                },
                [](const QJsonObject&) {
                    return EmptyListResponse("BloodMessGetListResponse", {"BloodMessList"});
                });
        });

    http.route("/tomb_messenger/message_area", QHttpServerRequest::Method::Post,
               [runtime](const QHttpServerRequest& request) {
                   return HandleSessionEndpoint(
                       runtime, QStringLiteral("TombMessGetList"), "TombMessGetListRequest",
                       request,
                       [](const QJsonObject& body) {
                           return HasArray(body, "AreaInfoList") &&
                                  HasNumber(body, "GetMaxCount") &&
                                  HasNumber(body, "MessShellInfoVersion") &&
                                  HasNumber(body, "TombDataVersion");
                       },
                       [](const QJsonObject&) {
                           return EmptyListResponse("TombMessGetListResponse", {"TombMessList"});
                       });
               });

    qInfo().noquote() << "Bloodborne bootstrap routes registered; public base URL" << publicBaseUrl;
}

} // namespace WebApiRoutes
