// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include "webapi_routes_bloodborne.h"

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
#include <QList>
#include <QString>

#include "bloodborne_summon_broker.h"

namespace WebApiRoutes {
namespace {

QHttpServerResponse SummonResponse(const QString& messageId) {
    QJsonObject body;
    body.insert(QStringLiteral("ResKind"), 0);
    body.insert(QStringLiteral("MessageId"), messageId);
    return QHttpServerResponse{"application/json",
                               QJsonDocument(body).toJson(QJsonDocument::Compact),
                               QHttpServerResponse::StatusCode::Ok};
}

QHttpServerResponse SummonListResponse(const QList<QByteArray>& signs) {
    QByteArray body = "{\"SummonDataList\":[";
    for (qsizetype index = 0; index < signs.size(); ++index) {
        if (index != 0) {
            body.append(',');
        }
        body.append(signs[index]);
    }
    body.append("],\"ResKind\":0,\"MessageId\":\"SummonDataGetListResponse\"}");
    return QHttpServerResponse{"application/json", body,
                               QHttpServerResponse::StatusCode::Ok};
}

QHttpServerResponse RawJsonResponse(const QByteArray& body) {
    return QHttpServerResponse{"application/json", body,
                               QHttpServerResponse::StatusCode::Ok};
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

} // namespace

void RegisterBloodborneRoutes(QHttpServer& http) {
    auto broker = std::make_shared<Bloodborne::SummonBroker>();

    http.route("/summon_messenger/create", QHttpServerRequest::Method::Post,
               [broker](const QHttpServerRequest& request) -> QHttpServerResponse {
                   const auto body = ParseRequest(request);
                   if (!body || !Bloodborne::HasRequiredAdvertisementFields(*body)) {
                       return InvalidRequest(QStringLiteral("Invalid summon advertisement"));
                   }
                   const auto result = broker->Advertise(
                       *body, request.body(), QDateTime::currentMSecsSinceEpoch());
                   if (!result.pendingClaim.isEmpty()) {
                       const QByteArray response =
                           Bloodborne::BuildClaimDeliveryResponse(result.pendingClaim);
                       if (response.isEmpty()) {
                           return InvalidRequest(QStringLiteral("Invalid pending summon claim"));
                       }
                       qInfo() << "Bloodborne summon: delivered claim to user"
                               << Integer(*body, QStringLiteral("UserId")) << "session"
                               << body->value(QStringLiteral("SessionId")).toString();
                       return RawJsonResponse(response);
                   }
                   qInfo() << "Bloodborne summon: advertised user"
                           << Integer(*body, QStringLiteral("UserId")) << "session"
                           << body->value(QStringLiteral("SessionId")).toString();
                   return SummonResponse(QStringLiteral("SummonDataCreateResponse"));
               });

    http.route("/summon_messenger/get", QHttpServerRequest::Method::Post,
               [broker](const QHttpServerRequest& request) -> QHttpServerResponse {
                   const auto body = ParseRequest(request);
                   if (!body) {
                       return InvalidRequest(QStringLiteral("Invalid summon search"));
                   }
                   const QList<QByteArray> signs =
                       broker->Search(*body, QDateTime::currentMSecsSinceEpoch());
                   qInfo() << "Bloodborne summon: search for user"
                           << Integer(*body, QStringLiteral("UserId")) << "returned"
                           << signs.size();
                   return SummonListResponse(signs);
               });

    http.route("/summon_messenger/delete", QHttpServerRequest::Method::Post,
               [broker](const QHttpServerRequest& request) -> QHttpServerResponse {
                   const auto body = ParseRequest(request);
                   if (!body) {
                       return InvalidRequest(QStringLiteral("Invalid summon removal"));
                   }
                   qInfo() << "Bloodborne summon: consumed"
                       << broker->Consume(*body, QDateTime::currentMSecsSinceEpoch())
                           << "advertisement(s)";
                   return SummonResponse(QStringLiteral("SummonDataRemoveResponse"));
               });

    http.route("/summon_messenger/request", QHttpServerRequest::Method::Post,
               [broker](const QHttpServerRequest& request) -> QHttpServerResponse {
                   const auto body = ParseRequest(request);
                   if (!body) {
                       return InvalidRequest(QStringLiteral("Invalid summon request"));
                   }
                   const auto result = broker->Claim(
                       *body, request.body(), QDateTime::currentMSecsSinceEpoch());
                   switch (result.status) {
                   case Bloodborne::SummonBroker::ClaimStatus::Claimed:
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
                   return SummonResponse(QStringLiteral("SummonDataSummonResponse"));
               });
}

} // namespace WebApiRoutes