// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include "webapi_routes_bloodborne.h"

#include <cstdlib>
#include <memory>
#include <optional>

#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QHttpHeaders>
#include <QHttpServer>
#include <QHttpServerRequest>
#include <QHttpServerResponse>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QList>
#include <QString>

#include "bloodborne_summon_broker.h"
#include "database.h"

namespace WebApiRoutes {
namespace {

constexpr auto HostPlacementHeader = "X-ShadPS4-Bloodborne-Host-Placement";

QByteArray SummonEnvelopeBody(const QString& messageId) {
    QJsonObject body;
    body.insert(QStringLiteral("ResKind"), 0);
    body.insert(QStringLiteral("MessageId"), messageId);
    return QJsonDocument(body).toJson(QJsonDocument::Compact);
}

QByteArray SummonListBody(const QList<QByteArray>& signs) {
    QByteArray body = "{\"SummonDataList\":[";
    for (qsizetype index = 0; index < signs.size(); ++index) {
        if (index != 0) {
            body.append(',');
        }
        body.append(signs[index]);
    }
    body.append("],\"ResKind\":0,\"MessageId\":\"SummonDataGetListResponse\"}");
    return body;
}

QHttpServerResponse RawJsonResponse(const QByteArray& body, const QByteArray& hostPlacement = {}) {
    QHttpServerResponse response{"application/json", body, QHttpServerResponse::StatusCode::Ok};
    if (!hostPlacement.isEmpty()) {
        QHttpHeaders headers = response.headers();
        if (headers.append(HostPlacementHeader, hostPlacement)) {
            response.setHeaders(std::move(headers));
        }
    }
    return response;
}

QHttpServerResponse InvalidRequest(const QString& reason) {
    QJsonObject body;
    body.insert(QStringLiteral("ResKind"), 1);
    body.insert(QStringLiteral("MessageId"), QString());
    body.insert(QStringLiteral("Error"), reason);
    return QHttpServerResponse{"application/json",
                               QJsonDocument(body).toJson(QJsonDocument::Compact),
                               QHttpServerResponse::StatusCode::BadRequest};
}

std::optional<QJsonObject> ParseRequest(const QHttpServerRequest& request) {
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(request.body(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return std::nullopt;
    }
    return document.object();
}

qint64 Integer(const QJsonObject& object, const QString& key, qint64 fallback = 0) {
    const QJsonValue value = object.value(key);
    return value.isDouble() ? value.toVariant().toLongLong() : fallback;
}

bool EnvEnabled(const char* name) {
    const char* value = std::getenv(name);
    return value != nullptr && value[0] != '\0' && QString::fromUtf8(value).trimmed() != "0";
}

QJsonValue SanitizeTraceValue(const QString& key, const QJsonValue& value) {
    const QString normalized = key.toLower();
    if (normalized.contains(QStringLiteral("session")) ||
        normalized.contains(QStringLiteral("password")) ||
        normalized.contains(QStringLiteral("token")) ||
        normalized.contains(QStringLiteral("authorization")) ||
        key == QStringLiteral("SummonWord")) {
        const QByteArray raw = value.toVariant().toString().toUtf8();
        QJsonObject metadata;
        metadata.insert(QStringLiteral("redacted"), true);
        metadata.insert(QStringLiteral("bytes"), raw.size());
        metadata.insert(
            QStringLiteral("sha256"),
            QString::fromLatin1(QCryptographicHash::hash(raw, QCryptographicHash::Sha256).toHex()));
        return metadata;
    }
    if (key == QStringLiteral("SummonData") || key == QStringLiteral("FormData") ||
        key == QStringLiteral("HostData")) {
        const QByteArray encoded = value.toString().toLatin1();
        const QByteArray decoded = QByteArray::fromBase64(encoded);
        QJsonObject metadata;
        metadata.insert(QStringLiteral("encoded_bytes"), encoded.size());
        metadata.insert(QStringLiteral("decoded_bytes"), decoded.size());
        metadata.insert(QStringLiteral("sha256"),
                        QString::fromLatin1(
                            QCryptographicHash::hash(decoded, QCryptographicHash::Sha256).toHex()));
        return metadata;
    }
    if (value.isObject()) {
        QJsonObject sanitized;
        const QJsonObject object = value.toObject();
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            sanitized.insert(it.key(), SanitizeTraceValue(it.key(), it.value()));
        }
        return sanitized;
    }
    if (value.isArray()) {
        QJsonArray sanitized;
        for (const QJsonValue& item : value.toArray()) {
            sanitized.append(SanitizeTraceValue(QString(), item));
        }
        return sanitized;
    }
    return value;
}

void TraceSummonPayload(const char* route, const char* direction, const QByteArray& payload,
                        const QByteArray& hostPlacement, bool enabled, int status = 200) {
    if (!enabled) {
        return;
    }
    QJsonObject trace;
    trace.insert(QStringLiteral("route"), QString::fromLatin1(route));
    trace.insert(QStringLiteral("direction"), QString::fromLatin1(direction));
    trace.insert(QStringLiteral("status"), status);
    trace.insert(QStringLiteral("body_bytes"), payload.size());
    trace.insert(
        QStringLiteral("body_sha256"),
        QString::fromLatin1(QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex()));
    trace.insert(QStringLiteral("host_placement_bytes"), hostPlacement.size());
    if (!hostPlacement.isEmpty()) {
        trace.insert(
            QStringLiteral("host_placement_sha256"),
            QString::fromLatin1(
                QCryptographicHash::hash(hostPlacement, QCryptographicHash::Sha256).toHex()));
    }
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(payload, &error);
    if (error.error == QJsonParseError::NoError && document.isObject()) {
        trace.insert(QStringLiteral("json"), SanitizeTraceValue(QString(), document.object()));
    }
    qInfo().noquote() << "[BLOODBORNE SUMMON TRACE]"
                      << QJsonDocument(trace).toJson(QJsonDocument::Compact);
}

void TraceSummonRequest(const char* route, const QHttpServerRequest& request, bool enabled) {
    TraceSummonPayload(route, "request", request.body(), request.value(HostPlacementHeader),
                       enabled, 0);
}

} // namespace

void RegisterBloodborneRoutes(QHttpServer& http, bool seamlessCoop, const QString& locationMode,
                              bool summonTrace, Database* websiteMetricsDatabase) {
    seamlessCoop = seamlessCoop || EnvEnabled("SHADNET_BLOODBORNE_SEAMLESS_COOP");
    summonTrace = summonTrace || EnvEnabled("SHADNET_BLOODBORNE_SUMMON_TRACE") ||
                  EnvEnabled("SHADNET_BLOODBORNE_RE_TRACE");
    Bloodborne::SummonBroker::Options options;
    options.seamlessCoop = seamlessCoop;
    bool validLocationMode = false;
    options.locationMode = Bloodborne::ParseSummonLocationMode(locationMode, &validLocationMode);
    options.trace = summonTrace;
    auto broker = std::make_shared<Bloodborne::SummonBroker>(options);

    if (!validLocationMode) {
        qWarning() << "Invalid summon LocationMode" << locationMode << "; using Vanilla";
    }

    qInfo() << "Bloodborne summon routes registered; seamless co-op"
            << (broker->IsSeamlessCoopEnabled() ? "enabled" : "disabled") << "anywhere summons"
            << (broker->IsSeamlessAnywhereSummonsEnabled() ? "enabled" : "disabled")
            << "location mode" << Bloodborne::SummonLocationModeName(broker->GetLocationMode())
            << "trace" << (summonTrace ? "enabled" : "disabled");

    http.route("/summon_messenger/create", QHttpServerRequest::Method::Post,
               [broker, websiteMetricsDatabase,
                summonTrace](const QHttpServerRequest& request) -> QHttpServerResponse {
                   TraceSummonRequest("create", request, summonTrace);
                   const auto body = ParseRequest(request);
                   if (!body || !Bloodborne::HasRequiredAdvertisementFields(*body)) {
                       return InvalidRequest(QStringLiteral("Invalid summon advertisement"));
                   }
                   const auto result = broker->Advertise(*body, request.body(),
                                                         QDateTime::currentMSecsSinceEpoch());
                   if (websiteMetricsDatabase != nullptr) {
                       websiteMetricsDatabase->RecordBloodborneWebsiteEvent(
                           Integer(*body, QStringLiteral("UserId")),
                           BloodborneWebsiteEvent::SummonAdvertised);
                   }
                   if (!result.pendingClaim.isEmpty()) {
                       const QByteArray response =
                           Bloodborne::BuildClaimDeliveryResponse(result.pendingClaim);
                       if (response.isEmpty()) {
                           return InvalidRequest(QStringLiteral("Invalid pending summon claim"));
                       }
                       qInfo() << "Bloodborne summon: delivered claim to user"
                               << Integer(*body, QStringLiteral("UserId")) << "session"
                               << body->value(QStringLiteral("SessionId")).toString()
                               << "host-placement-bytes" << result.pendingHostPlacement.size();
                       TraceSummonPayload("create", "response", response,
                                          result.pendingHostPlacement, summonTrace);
                       return RawJsonResponse(response, result.pendingHostPlacement);
                   }
                   if (!result.pendingHostPlacement.isEmpty()) {
                       qInfo() << "Bloodborne summon: preparing cross-map user"
                               << Integer(*body, QStringLiteral("UserId")) << "session"
                               << body->value(QStringLiteral("SessionId")).toString()
                               << "destination placement bytes"
                               << result.pendingHostPlacement.size();
                       const QByteArray response =
                           SummonEnvelopeBody(QStringLiteral("SummonDataCreateResponse"));
                       TraceSummonPayload("create", "response", response,
                                          result.pendingHostPlacement, summonTrace);
                       return RawJsonResponse(response, result.pendingHostPlacement);
                   }
                   qInfo() << "Bloodborne summon: advertised user"
                           << Integer(*body, QStringLiteral("UserId")) << "session"
                           << body->value(QStringLiteral("SessionId")).toString();
                   const QByteArray response =
                       SummonEnvelopeBody(QStringLiteral("SummonDataCreateResponse"));
                   TraceSummonPayload("create", "response", response, {}, summonTrace);
                   return RawJsonResponse(response);
               });

    http.route("/summon_messenger/get", QHttpServerRequest::Method::Post,
               [broker, summonTrace](const QHttpServerRequest& request) -> QHttpServerResponse {
                   TraceSummonRequest("get", request, summonTrace);
                   const auto body = ParseRequest(request);
                   if (!body) {
                       return InvalidRequest(QStringLiteral("Invalid summon search"));
                   }
                   const QList<QByteArray> signs =
                       broker->Search(*body, QDateTime::currentMSecsSinceEpoch(),
                                      request.value(HostPlacementHeader));
                   qInfo() << "Bloodborne summon: search for user"
                           << Integer(*body, QStringLiteral("UserId")) << "returned"
                           << signs.size();
                   const QByteArray response = SummonListBody(signs);
                   TraceSummonPayload("get", "response", response, {}, summonTrace);
                   return RawJsonResponse(response);
               });

    http.route("/summon_messenger/delete", QHttpServerRequest::Method::Post,
               [broker, summonTrace](const QHttpServerRequest& request) -> QHttpServerResponse {
                   TraceSummonRequest("delete", request, summonTrace);
                   const auto body = ParseRequest(request);
                   if (!body) {
                       return InvalidRequest(QStringLiteral("Invalid summon removal"));
                   }
                   const auto result = broker->Consume(*body, QDateTime::currentMSecsSinceEpoch());
                   qInfo() << "Bloodborne summon: consumed" << result.consumed << "retained"
                           << result.retained << "advertisement(s) host-placement-bytes"
                           << result.pendingHostPlacement.size();
                   const QByteArray responseBody =
                       SummonEnvelopeBody(QStringLiteral("SummonDataRemoveResponse"));
                   TraceSummonPayload("delete", "response", responseBody,
                                      result.pendingHostPlacement, summonTrace);
                   QHttpServerResponse response = RawJsonResponse(responseBody);
                   if (result.pendingHostPlacement.isEmpty()) {
                       return response;
                   }
                   QHttpHeaders headers = response.headers();
                   if (headers.append(HostPlacementHeader, result.pendingHostPlacement)) {
                       response.setHeaders(std::move(headers));
                   }
                   return response;
               });

    http.route("/summon_messenger/request", QHttpServerRequest::Method::Post,
               [broker, websiteMetricsDatabase,
                summonTrace](const QHttpServerRequest& request) -> QHttpServerResponse {
                   TraceSummonRequest("request", request, summonTrace);
                   const auto body = ParseRequest(request);
                   if (!body) {
                       return InvalidRequest(QStringLiteral("Invalid summon request"));
                   }
                   const auto result =
                       broker->Claim(*body, request.body(), QDateTime::currentMSecsSinceEpoch(),
                                     request.value(HostPlacementHeader));
                   switch (result.status) {
                   case Bloodborne::SummonBroker::ClaimStatus::Claimed:
                       if (websiteMetricsDatabase != nullptr) {
                           websiteMetricsDatabase->RecordBloodborneWebsiteEvent(
                               Integer(*body, QStringLiteral("UserId")),
                               BloodborneWebsiteEvent::SummonClaimed);
                       }
                       qInfo() << "Bloodborne summon: claimed session" << result.targetSessionId
                               << "user" << result.targetUserId;
                       break;
                   case Bloodborne::SummonBroker::ClaimStatus::AlreadyClaimed:
                       qInfo() << "Bloodborne summon: repeated claim for session"
                               << result.targetSessionId;
                       break;
                   case Bloodborne::SummonBroker::ClaimStatus::NotFound:
                       qWarning() << "Bloodborne summon: claim target not found";
                       break;
                   case Bloodborne::SummonBroker::ClaimStatus::Conflict:
                       qWarning() << "Bloodborne summon: conflicting claim for session"
                                  << result.targetSessionId;
                       break;
                   }
                   const QByteArray response =
                       SummonEnvelopeBody(QStringLiteral("SummonDataSummonResponse"));
                   TraceSummonPayload("request", "response", response, {}, summonTrace);
                   return RawJsonResponse(response);
               });
}

} // namespace WebApiRoutes
