// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include "webapi_routes_activity.h"

#include <utility>

#include <QCryptographicHash>
#include <QDebug>
#include <QHttpServer>
#include <QHttpServerRequest>
#include <QHttpServerResponse>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>

#include "database.h"
#include "webapi_auth.h"
#include "webapi_routes_common.h"

namespace WebApiRoutes {
namespace {

constexpr quint32 FeedInvalidBody = 2105601;

bool IsSelf(const QString& userKey, const WebApiAuth::AuthResult& auth) {
    return userKey.compare(QStringLiteral("me"), Qt::CaseInsensitive) == 0 ||
           userKey.compare(auth.npid, Qt::CaseInsensitive) == 0 ||
           userKey == QString::number(*auth.userId);
}

bool IsJsonContentType(const QByteArray& rawContentType) {
    const QByteArray contentType = rawContentType.split(';').front().trimmed();
    return contentType.compare(QByteArrayLiteral("application/json"), Qt::CaseInsensitive) == 0;
}

bool IsSensitiveField(const QString& key) {
    const QString folded = key.toCaseFolded();
    return folded.contains(QStringLiteral("authorization")) ||
           folded.contains(QStringLiteral("token")) ||
           folded.contains(QStringLiteral("password")) ||
           folded.contains(QStringLiteral("passphrase")) ||
           folded.contains(QStringLiteral("secret")) ||
           folded.contains(QStringLiteral("session")) ||
           folded.contains(QStringLiteral("credential")) ||
           folded.contains(QStringLiteral("cookie"));
}

QString JsonTypeName(const QJsonValue& value) {
    switch (value.type()) {
    case QJsonValue::Null:
        return QStringLiteral("null");
    case QJsonValue::Bool:
        return QStringLiteral("bool");
    case QJsonValue::Double:
        return QStringLiteral("number");
    case QJsonValue::String:
        return QStringLiteral("string");
    case QJsonValue::Array:
        return QStringLiteral("array");
    case QJsonValue::Object:
        return QStringLiteral("object");
    case QJsonValue::Undefined:
        return QStringLiteral("undefined");
    }
    return QStringLiteral("unknown");
}

QJsonValue SanitizeValue(const QJsonValue& value, const QString& key = {}) {
    if (IsSensitiveField(key)) {
        return QStringLiteral("[REDACTED]");
    }
    if (value.isObject()) {
        QJsonObject sanitized;
        const QJsonObject object = value.toObject();
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            sanitized.insert(it.key(), SanitizeValue(it.value(), it.key()));
        }
        return sanitized;
    }
    if (value.isArray()) {
        QJsonArray sanitized;
        for (const QJsonValue& item : value.toArray()) {
            sanitized.append(SanitizeValue(item));
        }
        return sanitized;
    }
    if (value.isString()) {
        static const QRegularExpression bearer(QStringLiteral(R"((?i)\bbearer\s+[^\s,;]+)"));
        if (bearer.match(value.toString()).hasMatch()) {
            return QStringLiteral("[REDACTED]");
        }
    }
    return value;
}

void TraceFeed(const QString& npid, const QByteArray& body, const QJsonDocument& document) {
    QJsonObject fieldTypes;
    if (document.isObject()) {
        const QJsonObject object = document.object();
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            fieldTypes.insert(it.key(), JsonTypeName(it.value()));
        }
    } else {
        fieldTypes.insert(QStringLiteral("$"), QStringLiteral("array"));
    }

    const QJsonValue root =
        document.isObject() ? QJsonValue(document.object()) : QJsonValue(document.array());
    QJsonDocument sanitized;
    const QJsonValue sanitizedRoot = SanitizeValue(root);
    if (sanitizedRoot.isObject()) {
        sanitized.setObject(sanitizedRoot.toObject());
    } else {
        sanitized.setArray(sanitizedRoot.toArray());
    }

    qInfo().noquote() << "[BLOODBORNE FEED TRACE]"
                      << "npid=" + npid << "body_bytes=" + QString::number(body.size())
                      << "sha256=" +
                             QString::fromLatin1(
                                 QCryptographicHash::hash(body, QCryptographicHash::Sha256).toHex())
                      << "fields=" + QString::fromUtf8(
                                         QJsonDocument(fieldTypes).toJson(QJsonDocument::Compact))
                      << "json=" + QString::fromUtf8(sanitized.toJson(QJsonDocument::Compact));
}

} // namespace

void RegisterActivityFeedRoutes(QHttpServer& http, Database& db, bool bloodborneFeedTrace) {
    http.route(
        "/v1/users/<arg>/feed", QHttpServerRequest::Method::Post,
        [&db, bloodborneFeedTrace](const QString& userKey,
                                   const QHttpServerRequest& request) -> QHttpServerResponse {
            auto auth = WebApiAuth::Authenticate(request, db);
            if (!auth.userId.has_value()) {
                return std::move(auth.errorResponse);
            }
            if (!IsSelf(userKey, auth)) {
                return JsonError(QHttpServerResponse::StatusCode::Forbidden,
                                 UP_ACCESS_DENIED_OWNERSHIP,
                                 QStringLiteral("Access denied by resource ownership"));
            }
            if (!IsJsonContentType(request.value("Content-Type"))) {
                return JsonError(QHttpServerResponse::StatusCode::UnsupportedMediaType,
                                 FeedInvalidBody,
                                 QStringLiteral("Content-Type must be application/json"));
            }

            const QByteArray body = request.body();
            QJsonParseError parseError{};
            const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
            if (body.isEmpty() || parseError.error != QJsonParseError::NoError ||
                document.isNull()) {
                return JsonError(QHttpServerResponse::StatusCode::BadRequest, FeedInvalidBody,
                                 QStringLiteral("Request body must contain valid JSON"));
            }

            if (bloodborneFeedTrace) {
                TraceFeed(auth.npid, body, document);
            }

            // Captures show that Bloodborne checks only the HTTP status and then
            // destroys the request. Until a response schema is observed, return
            // a successful empty body instead of inventing PSN activity fields.
            return QHttpServerResponse{"application/json", QByteArray{},
                                       QHttpServerResponse::StatusCode::Ok};
        });
}

} // namespace WebApiRoutes
