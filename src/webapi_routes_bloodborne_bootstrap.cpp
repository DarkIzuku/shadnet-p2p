// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include "webapi_routes_bloodborne_bootstrap.h"

#include <array>
#include <cmath>
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
#include <QStringList>
#include <QUrlQuery>
#include <QUuid>

#include "bloodborne_bootstrap.h"
#include "bloodborne_chalice_service.h"
#include "bloodborne_online_service.h"
#include "bloodborne_ssinfo_reference.h"
#include "client_session.h"
#include "database.h"

namespace WebApiRoutes {
namespace {

constexpr qsizetype MaxJsonRequestBytes = 2 * 1024 * 1024;

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
                                                    const QHttpServerRequest& request,
                                                    QString* failureReason = nullptr) const {
        const QJsonValue userValue = body.value(QStringLiteral("UserId"));
        const QString sessionId = body.value(QStringLiteral("SessionId")).toString();
        if (!userValue.isDouble()) {
            if (failureReason != nullptr)
                *failureReason = QStringLiteral("envelope.UserId is not a JSON number");
            return std::nullopt;
        }
        if (sessionId.isEmpty()) {
            if (failureReason != nullptr)
                *failureReason = QStringLiteral("envelope.SessionId is missing or empty");
            return std::nullopt;
        }

        const qint64 userId = userValue.toVariant().toLongLong();
        const QString queryUserId =
            QUrlQuery(request.url()).queryItemValue(QStringLiteral("user_id"));
        if (!queryUserId.isEmpty()) {
            bool queryValid = false;
            const qint64 parsedQueryUserId = queryUserId.toLongLong(&queryValid);
            if (!queryValid || parsedQueryUserId != userId) {
                if (failureReason != nullptr) {
                    *failureReason = QStringLiteral(
                        "envelope query user_id is invalid or does not match body UserId");
                }
                return std::nullopt;
            }
        }

        QMutexLocker lock(&m_mutex);
        const auto it = m_sessions.constFind(userId);
        if (it == m_sessions.constEnd()) {
            if (failureReason != nullptr)
                *failureReason = QStringLiteral("envelope.UserId has no active Bloodborne session");
            return std::nullopt;
        }
        if (it->userId != userId || it->sessionId != sessionId) {
            if (failureReason != nullptr)
                *failureReason =
                    QStringLiteral("envelope.SessionId does not match the active session");
            return std::nullopt;
        }
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
    static const std::array<const char*, 5> BlobFields{
        "BloodData", "TombData", "WanderingGhostData", "ShellData", "FormData"};
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
    const QString error = body.value(QStringLiteral("Error")).toString();
    qInfo().noquote() << "[BLOODBORNE LOCAL RESPONSE]"
                      << "api=" + api << "status=" + QString::number(static_cast<int>(status))
                      << "res_kind=" + (resKind.isDouble() ? QString::number(resKind.toInt())
                                                           : QStringLiteral("<none>"))
                      << "error=" + (error.isEmpty() ? QStringLiteral("<none>") : error);
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
    if (request.body().isEmpty() || request.body().size() > MaxJsonRequestBytes)
        return std::nullopt;
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

bool IsIntegerInRange(const QJsonValue& value, double minimum, double maximum) {
    if (!value.isDouble())
        return false;
    const double number = value.toDouble();
    return std::isfinite(number) && std::floor(number) == number && number >= minimum &&
           number <= maximum;
}

bool IsUnsigned64JsonInteger(const QJsonValue& value) {
    if (!value.isDouble())
        return false;
    const double number = value.toDouble();
    constexpr double Uint64LimitExclusive = 18446744073709551616.0; // 2^64
    return std::isfinite(number) && std::floor(number) == number && number >= 0 &&
           number < Uint64LimitExclusive;
}

QString ShapeValue(const QJsonValue& value) {
    if (value.isUndefined())
        return QStringLiteral("<missing>");
    if (value.isNull())
        return QStringLiteral("null");
    if (value.isBool())
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    if (value.isDouble())
        return QString::number(value.toDouble(), 'g', 17);
    if (value.isString())
        return QStringLiteral("string(length=%1)").arg(value.toString().size());
    if (value.isArray())
        return QStringLiteral("array(count=%1)").arg(value.toArray().size());
    if (value.isObject())
        return QStringLiteral("object");
    return QStringLiteral("<unknown>");
}

void TraceWanderingGhostGetRequest(const QHttpServerRequest& request,
                                   const std::optional<QJsonObject>& body) {
    if (!EnvEnabled("SHADNET_BLOODBORNE_BOOTSTRAP_TRACE"))
        return;

    const QByteArray hash =
        QCryptographicHash::hash(request.body(), QCryptographicHash::Sha256).toHex().toUpper();
    const QString queryUserId = QUrlQuery(request.url()).queryItemValue(QStringLiteral("user_id"));
    const QString contentType = QString::fromLatin1(request.value("Content-Type"));

    QStringList keys;
    QString messageId = QStringLiteral("<unavailable>");
    QString bodyUserId = QStringLiteral("<unavailable>");
    QString sessionIdPresent = QStringLiteral("false");
    QString areaList = QStringLiteral("<unavailable>");
    QString joinedList = QStringLiteral("<unavailable>");
    QString getMaxCount = QStringLiteral("<unavailable>");
    QString matchingLevel = QStringLiteral("<unavailable>");
    QString dataVersion = QStringLiteral("<unavailable>");
    if (body.has_value()) {
        keys = body->keys();
        const QJsonValue messageValue = body->value(QStringLiteral("MessageId"));
        messageId = messageValue.isString() ? messageValue.toString() : ShapeValue(messageValue);
        bodyUserId = ShapeValue(body->value(QStringLiteral("UserId")));
        const QJsonValue sessionValue = body->value(QStringLiteral("SessionId"));
        sessionIdPresent = sessionValue.isString() && !sessionValue.toString().isEmpty()
                               ? QStringLiteral("true")
                               : QStringLiteral("false");
        areaList = ShapeValue(body->value(QStringLiteral("AreaList")));
        joinedList = ShapeValue(body->value(QStringLiteral("JoinedCharaIdList")));
        getMaxCount = ShapeValue(body->value(QStringLiteral("GetMaxCount")));
        matchingLevel = ShapeValue(body->value(QStringLiteral("MatchingLevel")));
        dataVersion = ShapeValue(body->value(QStringLiteral("WanderingGhostDataVersion")));
    }

    qInfo().noquote() << "[BLOODBORNE WG GET REQUEST]"
                      << "query_user_id=" +
                             (queryUserId.isEmpty() ? QStringLiteral("<none>") : queryUserId)
                      << "body_user_id=" + bodyUserId
                      << "content_type=" +
                             (contentType.isEmpty() ? QStringLiteral("<none>") : contentType)
                      << "body_size=" + QString::number(request.body().size())
                      << "body_sha256=" + QString::fromLatin1(hash)
                      << "keys=" + keys.join(QLatin1Char(',')) << "MessageId=" + messageId
                      << "SessionId_present=" + sessionIdPresent << "AreaList=" + areaList
                      << "JoinedCharaIdList=" + joinedList << "GetMaxCount=" + getMaxCount
                      << "MatchingLevel=" + matchingLevel
                      << "WanderingGhostDataVersion=" + dataVersion;

    if (!body.has_value() || !body->value(QStringLiteral("AreaList")).isArray())
        return;
    const QJsonArray areas = body->value(QStringLiteral("AreaList")).toArray();
    for (qsizetype index = 0; index < areas.size(); ++index) {
        if (!areas.at(index).isObject()) {
            qInfo().noquote() << "[BLOODBORNE WG GET AREA]"
                              << "index=" + QString::number(index)
                              << "value=" + ShapeValue(areas.at(index));
            continue;
        }
        const QJsonObject area = areas.at(index).toObject();
        qInfo().noquote() << "[BLOODBORNE WG GET AREA]"
                          << "index=" + QString::number(index)
                          << "AreaId=" + ShapeValue(area.value(QStringLiteral("AreaId")))
                          << "AreaRegionId=" +
                                 ShapeValue(area.value(QStringLiteral("AreaRegionId")))
                          << "ChannelId=" + ShapeValue(area.value(QStringLiteral("ChannelId")))
                          << "GetCount=" + ShapeValue(area.value(QStringLiteral("GetCount")));
    }

    if (!body->value(QStringLiteral("JoinedCharaIdList")).isArray())
        return;
    const QJsonArray joinedValues = body->value(QStringLiteral("JoinedCharaIdList")).toArray();
    for (qsizetype index = 0; index < joinedValues.size(); ++index) {
        const QJsonValue value = joinedValues.at(index);
        if (!value.isObject())
            continue;
        const QByteArray compactJson =
            QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact);
        qInfo().noquote() << "[BLOODBORNE WG JOINED CHARA]"
                          << "index=" + QString::number(index)
                          << "json=" + QString::fromUtf8(compactJson);
    }
}

QHttpServerResponse WanderingGhostGetBadRequest(const QString& api, const QString& reason) {
    const QString detail = QStringLiteral("WanderingGhostGet validation failed: %1").arg(reason);
    qWarning().noquote() << "[BLOODBORNE WG GET VALIDATION] result=rejected status=400"
                         << "reason=" + detail;
    return ErrorResponse(api, QHttpServerResponse::StatusCode::BadRequest, detail);
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

using OnlineHandler =
    std::function<Bloodborne::OnlineResult(qint64 userId, const QJsonObject& request)>;

QHttpServerResponse HandleOnlineEndpoint(const std::shared_ptr<BootstrapRuntime>& runtime,
                                         const QString& api, const char* requestMessageId,
                                         const QHttpServerRequest& request,
                                         const OnlineHandler& handler) {
    const auto body = ParseRequest(request);
    TraceRequest(api, request, body);
    if (!body || !HasMessageId(*body, requestMessageId) || !HasSessionFields(*body)) {
        return ErrorResponse(api, QHttpServerResponse::StatusCode::BadRequest,
                             QStringLiteral("Invalid %1").arg(QLatin1String(requestMessageId)));
    }
    const auto session = runtime->ValidateSession(*body, request);
    if (!session) {
        return ErrorResponse(api, QHttpServerResponse::StatusCode::Unauthorized,
                             QStringLiteral("Unknown Bloodborne session"));
    }

    const Bloodborne::OnlineResult result = handler(session->userId, *body);
    if (result.IsSuccess())
        return JsonResponse(api, result.response);

    QHttpServerResponse::StatusCode status = QHttpServerResponse::StatusCode::BadRequest;
    switch (result.error) {
    case Bloodborne::OnlineError::NotFound:
        status = QHttpServerResponse::StatusCode::NotFound;
        break;
    case Bloodborne::OnlineError::Forbidden:
        status = QHttpServerResponse::StatusCode::Forbidden;
        break;
    case Bloodborne::OnlineError::Database:
        status = QHttpServerResponse::StatusCode::InternalServerError;
        break;
    case Bloodborne::OnlineError::InvalidRequest:
    case Bloodborne::OnlineError::None:
        break;
    }
    return ErrorResponse(api, status, result.detail);
}

QHttpServerResponse HandleWanderingGhostGetEndpoint(
    const std::shared_ptr<BootstrapRuntime>& runtime,
    const std::shared_ptr<Bloodborne::OnlineService>& online, const QHttpServerRequest& request) {
    const QString api = QStringLiteral("WanderingGhostGet");
    const auto body = ParseRequest(request);
    TraceRequest(api, request, body);
    TraceWanderingGhostGetRequest(request, body);

    if (!body) {
        if (request.body().isEmpty())
            return WanderingGhostGetBadRequest(api, QStringLiteral("field=envelope.body expected="
                                                                   "non-empty JSON object "
                                                                   "value=empty"));
        if (request.body().size() > MaxJsonRequestBytes) {
            return WanderingGhostGetBadRequest(
                api, QStringLiteral("field=envelope.body expected=at most %1 bytes value=%2 bytes")
                         .arg(MaxJsonRequestBytes)
                         .arg(request.body().size()));
        }
        return WanderingGhostGetBadRequest(
            api, QStringLiteral("field=envelope.body expected=JSON object value=invalid_json"));
    }
    const QJsonValue messageId = body->value(QStringLiteral("MessageId"));
    if (!HasMessageId(*body, "WanderingGhostGetRequest")) {
        return WanderingGhostGetBadRequest(
            api, QStringLiteral("field=envelope.MessageId expected=WanderingGhostGetRequest "
                                "value=%1")
                     .arg(messageId.isString() ? messageId.toString() : ShapeValue(messageId)));
    }
    if (!body->value(QStringLiteral("UserId")).isDouble()) {
        return WanderingGhostGetBadRequest(
            api, QStringLiteral("field=envelope.UserId expected=JSON number value=%1")
                     .arg(ShapeValue(body->value(QStringLiteral("UserId")))));
    }
    const QJsonValue sessionId = body->value(QStringLiteral("SessionId"));
    if (!sessionId.isString() || sessionId.toString().isEmpty()) {
        return WanderingGhostGetBadRequest(
            api, QStringLiteral("field=envelope.SessionId expected=non-empty string value=%1")
                     .arg(ShapeValue(sessionId)));
    }

    QString sessionFailure;
    const auto session = runtime->ValidateSession(*body, request, &sessionFailure);
    if (!session) {
        qWarning().noquote() << "[BLOODBORNE WG GET VALIDATION] result=rejected status=401"
                             << "reason=" + sessionFailure;
        return ErrorResponse(api, QHttpServerResponse::StatusCode::Unauthorized,
                             QStringLiteral("Unknown Bloodborne session"));
    }

    const Bloodborne::OnlineResult result = online->GetWanderingGhosts(session->userId, *body);
    if (result.IsSuccess()) {
        qInfo().noquote() << "[BLOODBORNE WG GET VALIDATION] result=accepted";
        return JsonResponse(api, result.response);
    }

    QHttpServerResponse::StatusCode status = QHttpServerResponse::StatusCode::BadRequest;
    if (result.error == Bloodborne::OnlineError::Database)
        status = QHttpServerResponse::StatusCode::InternalServerError;
    qWarning().noquote() << "[BLOODBORNE WG GET VALIDATION] result=rejected"
                         << "status=" + QString::number(static_cast<int>(status))
                         << "reason=" + result.detail;
    return ErrorResponse(api, status, result.detail);
}

} // namespace

void RegisterBloodborneBootstrapRoutes(QHttpServer& http, Database& db, SharedState& shared,
                                       const QString& publicBaseUrl,
                                       const QByteArray& serverStatusInfo,
                                       bool referenceProxyEnabled,
                                       const Bloodborne::WelcomeNotice& welcomeNotice,
                                       const Bloodborne::WelcomeMessage& welcomeMessage,
                                       int ghostLifetimeSeconds, bool websiteMetricsEnabled) {
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
    const auto online = std::make_shared<Bloodborne::OnlineService>(db, ghostLifetimeSeconds,
                                                                    websiteMetricsEnabled);
    const auto chalices = std::make_shared<Bloodborne::ChaliceService>(db);

    http.route(
        "/basic_utils/login", QHttpServerRequest::Method::Post,
        [runtime, welcomeMessage](const QHttpServerRequest& request) {
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

            const QJsonObject response = Bloodborne::BuildLoginResponse(
                identity->userId, languageId, session->sessionId, welcomeMessage);
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
                   return JsonResponse(api, Bloodborne::BuildNoticeNormalResponse(welcomeNotice));
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
                       [](const QJsonObject& body) {
                           return IsIntegerInRange(body.value(QStringLiteral("Count")), 0,
                                                   0x7FFFFFFF);
                       },
                       [](const QJsonObject&) {
                           return SuccessResponse("UserPropertiesMoveCountCheckResponse");
                       });
               });

    http.route("/penalty/notify_user_properties_move_count", QHttpServerRequest::Method::Post,
               [runtime](const QHttpServerRequest& request) {
                   return HandleSessionEndpoint(
                       runtime, QStringLiteral("UserPropertiesMoveCount"),
                       "UserPropertiesMoveCountRequest", request,
                       [](const QJsonObject& body) {
                           return IsIntegerInRange(body.value(QStringLiteral("Count")), 0,
                                                   0x7FFFFFFF);
                       },
                       [](const QJsonObject&) {
                           return SuccessResponse("UserPropertiesMoveCountResponse");
                       });
               });

    http.route("/channel/upload", QHttpServerRequest::Method::Post,
               [runtime, chalices](const QHttpServerRequest& request) {
                   return HandleOnlineEndpoint(runtime, QStringLiteral("ChannelUpload"),
                                               "ChannelUploadRequest", request,
                                               [chalices](qint64 userId, const QJsonObject& body) {
                                                   return chalices->Upload(userId, body);
                                               });
               });

    http.route("/channel/share", QHttpServerRequest::Method::Post,
               [runtime, chalices](const QHttpServerRequest& request) {
                   return HandleOnlineEndpoint(runtime, QStringLiteral("ChannelShare"),
                                               "ChannelShareRequest", request,
                                               [chalices](qint64 userId, const QJsonObject& body) {
                                                   return chalices->Share(userId, body);
                                               });
               });

    http.route("/channel/search", QHttpServerRequest::Method::Post,
               [runtime, chalices](const QHttpServerRequest& request) {
                   return HandleOnlineEndpoint(runtime, QStringLiteral("ChannelSearch"),
                                               "ChannelSearchRequest", request,
                                               [chalices](qint64 userId, const QJsonObject& body) {
                                                   return chalices->Search(userId, body);
                                               });
               });

    http.route("/channel/word_search", QHttpServerRequest::Method::Post,
               [runtime, chalices](const QHttpServerRequest& request) {
                   return HandleOnlineEndpoint(runtime, QStringLiteral("ChannelWordSearch"),
                                               "ChannelWordSearchRequest", request,
                                               [chalices](qint64 userId, const QJsonObject& body) {
                                                   return chalices->WordSearch(userId, body);
                                               });
               });

    http.route("/channel/get_details_info", QHttpServerRequest::Method::Post,
               [runtime, chalices](const QHttpServerRequest& request) {
                   return HandleOnlineEndpoint(runtime, QStringLiteral("ChannelGetDetailsInfo"),
                                               "ChannelGetDetailsInfoRequest", request,
                                               [chalices](qint64 userId, const QJsonObject& body) {
                                                   return chalices->GetDetailsInfo(userId, body);
                                               });
               });

    http.route("/channel/random_join", QHttpServerRequest::Method::Post,
               [runtime, chalices](const QHttpServerRequest& request) {
                   return HandleOnlineEndpoint(runtime, QStringLiteral("ChannelRandomJoin"),
                                               "ChannelRandomJoinRequest", request,
                                               [chalices](qint64 userId, const QJsonObject& body) {
                                                   return chalices->RandomJoin(userId, body);
                                               });
               });

    const auto pendingChaliceMaterial = [](const QHttpServerRequest& request) {
        const QString api = QStringLiteral("ChannelMaterialPending");
        const auto body = ParseRequest(request);
        TraceRequest(api, request, body);
        const QString messageId =
            body ? body->value(QStringLiteral("MessageId")).toString() : QString();
        const qint64 userId = body && body->value(QStringLiteral("UserId")).isDouble()
                                  ? body->value(QStringLiteral("UserId")).toInteger()
                                  : 0;
        qWarning().noquote() << "[BLOODBORNE CHALICE PENDING]"
                             << "path=" + request.url().path()
                             << "user_id=" + QString::number(userId)
                             << "message_id=" +
                                    (messageId.isEmpty() ? QStringLiteral("<missing>") : messageId);
        return ErrorResponse(api, QHttpServerResponse::StatusCode::NotImplemented,
                             QStringLiteral("Chalice material contract not captured"));
    };
    http.route("/channel/add_material", QHttpServerRequest::Method::Post, pendingChaliceMaterial);
    http.route("/channel/add_material_complete_notify", QHttpServerRequest::Method::Post,
               pendingChaliceMaterial);
    http.route("/channel/notify_add_material_complete", QHttpServerRequest::Method::Post,
               pendingChaliceMaterial);

    http.route("/blood_messenger/exist_messages", QHttpServerRequest::Method::Post,
               [runtime, online](const QHttpServerRequest& request) {
                   return HandleOnlineEndpoint(runtime, QStringLiteral("BloodMessSearchAdd"),
                                               "BloodMessSearchAddRequest", request,
                                               [online](qint64 userId, const QJsonObject& body) {
                                                   return online->SearchBloodMessages(userId, body);
                                               });
               });

    http.route("/messenger_shell/upload", QHttpServerRequest::Method::Post,
               [runtime, online](const QHttpServerRequest& request) {
                   return HandleOnlineEndpoint(runtime, QStringLiteral("MessengerShellUpload"),
                                               "MessengerShellUploadRequest", request,
                                               [online](qint64 userId, const QJsonObject& body) {
                                                   return online->UploadMessengerShell(userId,
                                                                                       body);
                                               });
               });

    http.route("/wandering_ghost/create", QHttpServerRequest::Method::Post,
               [runtime, online](const QHttpServerRequest& request) {
                   return HandleOnlineEndpoint(runtime, QStringLiteral("WanderingGhostCreate"),
                                               "WanderingGhostCreateRequest", request,
                                               [online](qint64 userId, const QJsonObject& body) {
                                                   return online->CreateWanderingGhost(userId,
                                                                                       body);
                                               });
               });

    http.route("/wandering_ghost/get", QHttpServerRequest::Method::Post,
               [runtime, online](const QHttpServerRequest& request) {
                   return HandleWanderingGhostGetEndpoint(runtime, online, request);
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

    http.route("/chair_messenger/update", QHttpServerRequest::Method::Post,
               [runtime](const QHttpServerRequest& request) {
                   return HandleSessionEndpoint(
                       runtime, QStringLiteral("ChairMessRespawnPointNotice"),
                       "ChairMessRespawnPointNoticeRequest", request,
                       [](const QJsonObject& body) {
                           return IsUnsigned64JsonInteger(body.value(QStringLiteral("CharaId"))) &&
                                  IsIntegerInRange(body.value(QStringLiteral("ChannelId")), 0,
                                                   0x7FFFFFFF) &&
                                  IsIntegerInRange(body.value(QStringLiteral("WarpInfoId")), 0,
                                                   0x7FFFFFFF);
                       },
                       [](const QJsonObject&) {
                           return SuccessResponse("ChairMessRespawnPointNoticeResponse");
                       });
               });

    http.route("/channel/get_info", QHttpServerRequest::Method::Post,
               [runtime, chalices](const QHttpServerRequest& request) {
                   return HandleOnlineEndpoint(runtime, QStringLiteral("ChannelGetInfo"),
                                               "ChannelGetInfoRequest", request,
                                               [chalices](qint64 userId, const QJsonObject& body) {
                                                   return chalices->GetInfo(userId, body);
                                               });
               });

    http.route("/blood_messenger/evaluation", QHttpServerRequest::Method::Post,
               [runtime, online](const QHttpServerRequest& request) {
                   return HandleOnlineEndpoint(runtime, QStringLiteral("BloodMessGetEvaluate"),
                                               "BloodMessGetEvaluateRequest", request,
                                               [online](qint64 userId, const QJsonObject& body) {
                                                   return online->GetBloodEvaluations(userId, body);
                                               });
               });

    http.route("/blood_messenger/create", QHttpServerRequest::Method::Post,
               [runtime, online](const QHttpServerRequest& request) {
                   return HandleOnlineEndpoint(runtime, QStringLiteral("BloodMessCreate"),
                                               "BloodMessCreateRequest", request,
                                               [online](qint64 userId, const QJsonObject& body) {
                                                   return online->CreateBloodMessages(userId, body);
                                               });
               });

    http.route("/blood_messenger/evaluate_message", QHttpServerRequest::Method::Post,
               [runtime, online](const QHttpServerRequest& request) {
                   return HandleOnlineEndpoint(
                       runtime, QStringLiteral("BloodMessEvaluate"), "BloodMessEvaluateRequest",
                       request, [online](qint64 userId, const QJsonObject& body) {
                           return online->EvaluateBloodMessage(userId, body);
                       });
               });

    http.route("/blood_messenger/delete", QHttpServerRequest::Method::Post,
               [runtime, online](const QHttpServerRequest& request) {
                   return HandleOnlineEndpoint(runtime, QStringLiteral("BloodMessRemove"),
                                               "BloodMessRemoveRequest", request,
                                               [online](qint64 userId, const QJsonObject& body) {
                                                   return online->DeleteBloodMessage(userId, body);
                                               });
               });

    http.route("/blood_messenger/message_area", QHttpServerRequest::Method::Post,
               [runtime, online](const QHttpServerRequest& request) {
                   return HandleOnlineEndpoint(runtime, QStringLiteral("BloodMessGetList"),
                                               "BloodMessGetListRequest", request,
                                               [online](qint64 userId, const QJsonObject& body) {
                                                   return online->GetBloodMessages(userId, body);
                                               });
               });

    http.route("/tomb_messenger/create", QHttpServerRequest::Method::Post,
               [runtime, online](const QHttpServerRequest& request) {
                   return HandleOnlineEndpoint(runtime, QStringLiteral("TombMessCreate"),
                                               "TombMessCreateRequest", request,
                                               [online](qint64 userId, const QJsonObject& body) {
                                                   return online->CreateTombMessage(userId, body);
                                               });
               });

    http.route("/tomb_messenger/message_area", QHttpServerRequest::Method::Post,
               [runtime, online](const QHttpServerRequest& request) {
                   return HandleOnlineEndpoint(runtime, QStringLiteral("TombMessGetList"),
                                               "TombMessGetListRequest", request,
                                               [online](qint64 userId, const QJsonObject& body) {
                                                   return online->GetTombMessages(userId, body);
                                               });
               });

    http.route("/tomb_messenger/death_vision_get", QHttpServerRequest::Method::Post,
               [runtime, online](const QHttpServerRequest& request) {
                   return HandleOnlineEndpoint(runtime, QStringLiteral("DeathVisionGet"),
                                               "DeathVisionGetRequest", request,
                                               [online](qint64 userId, const QJsonObject& body) {
                                                   return online->GetDeathVision(userId, body);
                                               });
               });

    qInfo().noquote() << "Bloodborne bootstrap routes registered; public base URL" << publicBaseUrl;
}

} // namespace WebApiRoutes
