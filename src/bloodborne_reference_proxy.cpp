// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include "bloodborne_reference_proxy.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <memory>
#include <utility>

#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QHttpServerRequest>
#include <QHttpServerResponder>
#include <QHttpServerResponse>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMap>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QTextStream>
#include <QUrlQuery>

namespace Bloodborne {
namespace {

struct CapturedBody {
    QByteArray storedBody;
    QString extension;
    QString consoleJson;
    QJsonValue schema;
};

struct EndpointSummary {
    QString method;
    QString path;
    quint64 calls = 0;
    quint64 firstSequence = 0;
    quint64 lastSequence = 0;
    QSet<int> responseStatuses;
    QJsonValue requestSchema;
    QJsonValue responseSchema;
};

bool EnvEnabled(const char* name) {
    const char* value = std::getenv(name);
    return value != nullptr && value[0] != '\0' && QString::fromUtf8(value).trimmed() != "0";
}

bool IsSensitiveKey(const QString& key) {
    const QString lower = key.toLower();
    return lower.contains(QStringLiteral("authorizationcode")) ||
           lower.contains(QStringLiteral("password")) || lower.contains(QStringLiteral("token")) ||
           lower.contains(QStringLiteral("bearer"));
}

QJsonValue SanitizeJson(const QJsonValue& value, const QString& key = {}) {
    if (!key.isEmpty() && IsSensitiveKey(key))
        return QStringLiteral("<redacted>");

    if (value.isObject()) {
        QJsonObject result;
        const QJsonObject object = value.toObject();
        for (auto it = object.constBegin(); it != object.constEnd(); ++it)
            result.insert(it.key(), SanitizeJson(it.value(), it.key()));
        return result;
    }
    if (value.isArray()) {
        QJsonArray result;
        const QJsonArray array = value.toArray();
        for (const QJsonValue& item : array)
            result.append(SanitizeJson(item));
        return result;
    }
    return value;
}

QJsonValue JsonSchema(const QJsonValue& value) {
    if (value.isObject()) {
        QJsonObject result;
        const QJsonObject object = value.toObject();
        for (auto it = object.constBegin(); it != object.constEnd(); ++it)
            result.insert(it.key(), JsonSchema(it.value()));
        return result;
    }
    if (value.isArray()) {
        const QJsonArray array = value.toArray();
        QJsonArray result;
        result.append(array.isEmpty() ? QJsonValue(QStringLiteral("<empty>"))
                                      : JsonSchema(array.front()));
        return result;
    }
    if (value.isBool())
        return QStringLiteral("boolean");
    if (value.isDouble())
        return QStringLiteral("number");
    if (value.isString())
        return QStringLiteral("string");
    if (value.isNull())
        return QStringLiteral("null");
    return QStringLiteral("undefined");
}

QByteArray JsonBytes(const QJsonValue& value, QJsonDocument::JsonFormat format) {
    if (value.isObject())
        return QJsonDocument(value.toObject()).toJson(format);
    if (value.isArray())
        return QJsonDocument(value.toArray()).toJson(format);
    QJsonObject wrapper;
    wrapper.insert(QStringLiteral("value"), value);
    return QJsonDocument(wrapper).toJson(format);
}

QString SanitizeText(QString text) {
    static const QRegularExpression KeyValueSecret(
        QStringLiteral("(?i)(AuthorizationCode|password|token)\\s*([=:])\\s*([^&\\s,}]+)"));
    static const QRegularExpression BearerSecret(
        QStringLiteral("(?i)bearer\\s+[A-Za-z0-9._~+\\-/]+=*"));
    text.replace(KeyValueSecret, QStringLiteral("\\1\\2<redacted>"));
    text.replace(BearerSecret, QStringLiteral("Bearer <redacted>"));
    return text;
}

bool IsTextContentType(const QByteArray& contentType) {
    const QByteArray lower = contentType.toLower();
    return lower.startsWith("text/") || lower.contains("json") || lower.contains("xml") ||
           lower.contains("javascript") || lower.contains("x-www-form-urlencoded");
}

CapturedBody CaptureBody(const QByteArray& body, const QByteArray& contentType) {
    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error == QJsonParseError::NoError && !document.isNull() &&
        (document.isObject() || document.isArray())) {
        const QJsonValue root =
            document.isObject() ? QJsonValue(document.object()) : QJsonValue(document.array());
        const QJsonValue sanitized = SanitizeJson(root);
        return {JsonBytes(sanitized, QJsonDocument::Indented), QStringLiteral("json"),
                QString::fromUtf8(JsonBytes(sanitized, QJsonDocument::Compact)), JsonSchema(root)};
    }

    if (IsTextContentType(contentType)) {
        return {SanitizeText(QString::fromUtf8(body)).toUtf8(),
                QStringLiteral("txt"),
                {},
                QStringLiteral("text")};
    }
    return {body, QStringLiteral("bin"), {}, QStringLiteral("binary")};
}

QByteArray Sha256(const QByteArray& body) {
    return QCryptographicHash::hash(body, QCryptographicHash::Sha256).toHex().toUpper();
}

QByteArray MethodName(QHttpServerRequest::Method method) {
    switch (method) {
    case QHttpServerRequest::Method::Get:
        return "GET";
    case QHttpServerRequest::Method::Put:
        return "PUT";
    case QHttpServerRequest::Method::Delete:
        return "DELETE";
    case QHttpServerRequest::Method::Post:
        return "POST";
    case QHttpServerRequest::Method::Head:
        return "HEAD";
    case QHttpServerRequest::Method::Options:
        return "OPTIONS";
    case QHttpServerRequest::Method::Patch:
        return "PATCH";
    case QHttpServerRequest::Method::Connect:
        return "CONNECT";
    case QHttpServerRequest::Method::Trace:
        return "TRACE";
    default:
        return {};
    }
}

QString SanitizeQuery(const QUrl& url) {
    QUrlQuery sanitized;
    const auto items = QUrlQuery(url).queryItems(QUrl::FullyDecoded);
    for (const auto& [key, value] : items)
        sanitized.addQueryItem(key, IsSensitiveKey(key) ? QStringLiteral("<redacted>") : value);
    return sanitized.toString(QUrl::FullyEncoded);
}

QString SequenceName(quint64 sequence) {
    return QStringLiteral("%1").arg(sequence, 4, 10, QLatin1Char('0'));
}

bool WriteFile(const QString& path, const QByteArray& body) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    return file.write(body) == body.size();
}

QString SchemaText(const QJsonValue& schema) {
    if (schema.isUndefined() || schema.isNull())
        return QStringLiteral("<non-json>");
    return QString::fromUtf8(JsonBytes(schema, QJsonDocument::Compact));
}

} // namespace

class ReferenceProxy::Impl {
public:
    Impl(ReferenceProxy* owner, Options options)
        : m_owner(owner), m_options(std::move(options)),
          m_traceEnabled(EnvEnabled("SHADNET_BLOODBORNE_REFERENCE_TRACE")) {}

    bool Initialize(QString* error) {
        if (!m_options.upstreamUrl.isValid() || m_options.upstreamUrl.host().isEmpty() ||
            (m_options.upstreamUrl.scheme() != QStringLiteral("http") &&
             m_options.upstreamUrl.scheme() != QStringLiteral("https"))) {
            if (error != nullptr)
                *error = QStringLiteral("upstream URL must be a valid http(s) URL");
            return false;
        }

        if (!QDir().mkpath(m_options.captureRoot)) {
            if (error != nullptr)
                *error = QStringLiteral("cannot create capture root %1").arg(m_options.captureRoot);
            return false;
        }

        const QString timestamp =
            QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz'Z'"));
        QString candidate = QDir(m_options.captureRoot).filePath(timestamp);
        for (int suffix = 1; QDir(candidate).exists(); ++suffix) {
            candidate = QDir(m_options.captureRoot)
                            .filePath(timestamp +
                                      QStringLiteral("-%1").arg(suffix, 2, 10, QLatin1Char('0')));
        }
        if (!QDir().mkpath(candidate)) {
            if (error != nullptr)
                *error = QStringLiteral("cannot create capture directory %1").arg(candidate);
            return false;
        }
        m_captureDirectory = QDir(candidate).absolutePath();

        QFile manifest(QDir(m_captureDirectory).filePath(QStringLiteral("manifest.jsonl")));
        if (!manifest.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            if (error != nullptr)
                *error =
                    QStringLiteral("cannot create capture manifest in %1").arg(m_captureDirectory);
            return false;
        }
        manifest.close();
        WriteSummary();
        if (error != nullptr)
            error->clear();
        qInfo().noquote() << "Bloodborne reference capture directory:" << m_captureDirectory;
        return true;
    }

    QString CaptureDirectory() const {
        return m_captureDirectory;
    }

    void Forward(const QHttpServerRequest& request, QHttpServerResponder&& responder) {
        const quint64 sequence = ++m_sequence;
        const QByteArray method = MethodName(request.method());
        const QString path = request.url().path();
        const QString query = request.url().query(QUrl::FullyEncoded);
        const QString sanitizedQuery = SanitizeQuery(request.url());
        const QByteArray requestBody = request.body();
        const QByteArray requestContentType = request.value("Content-Type");
        const QByteArray requestHash = Sha256(requestBody);
        const CapturedBody requestCapture = CaptureBody(requestBody, requestContentType);
        const QString requestFileName =
            SequenceName(sequence) + QStringLiteral("-request.") + requestCapture.extension;
        if (!WriteFile(QDir(m_captureDirectory).filePath(requestFileName),
                       requestCapture.storedBody)) {
            qWarning().noquote() << "Bloodborne reference proxy: could not write"
                                 << requestFileName;
        }

        if (m_traceEnabled) {
            qInfo().noquote() << "[BLOODBORNE REFERENCE REQUEST]"
                              << "seq=" + QString::number(sequence)
                              << "method=" + QString::fromLatin1(method) << "path=" + path
                              << "query=" + sanitizedQuery
                              << "body_size=" + QString::number(requestBody.size());
            if (!requestCapture.consoleJson.isEmpty())
                qInfo().noquote() << "[BLOODBORNE REFERENCE REQUEST JSON]"
                                  << "seq=" + QString::number(sequence)
                                  << requestCapture.consoleJson;
        }

        QUrl target = m_options.upstreamUrl;
        QString targetPath = target.path();
        if (targetPath.endsWith('/'))
            targetPath.chop(1);
        target.setPath(targetPath + path);
        target.setQuery(query);

        QNetworkRequest upstreamRequest{target};
        upstreamRequest.setTransferTimeout(m_options.transferTimeoutMs);
        for (const QByteArray& header : std::array<QByteArray, 4>{
                 QByteArrayLiteral("Content-Type"), QByteArrayLiteral("User-Agent"),
                 QByteArrayLiteral("Accept"), QByteArrayLiteral("Authorization")}) {
            const QByteArray value = request.value(header);
            if (!value.isEmpty())
                upstreamRequest.setRawHeader(header, value);
        }

        const auto responderPtr = std::make_shared<QHttpServerResponder>(std::move(responder));
        const auto elapsed = std::make_shared<QElapsedTimer>();
        elapsed->start();
        QNetworkReply* reply = m_network.sendCustomRequest(upstreamRequest, method, requestBody);
        QObject::connect(
            reply, &QNetworkReply::finished, m_owner,
            [this, reply, responderPtr, elapsed, sequence, method, path, sanitizedQuery,
             requestBody, requestContentType, requestHash, requestFileName,
             requestSchema = requestCapture.schema]() mutable {
                int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                QByteArray responseBody;
                QByteArray responseContentType = reply->rawHeader("Content-Type");
                QString networkError;

                if (status > 0) {
                    responseBody = reply->readAll();
                    if (responseContentType.isEmpty())
                        responseContentType = QByteArrayLiteral("application/octet-stream");
                } else {
                    networkError = reply->errorString();
                    status = reply->error() == QNetworkReply::TimeoutError ? 504 : 502;
                    QJsonObject errorBody;
                    errorBody.insert(
                        QStringLiteral("error"),
                        QStringLiteral("Bloodborne reference upstream request failed"));
                    errorBody.insert(QStringLiteral("detail"), networkError);
                    responseBody = QJsonDocument(errorBody).toJson(QJsonDocument::Compact);
                    responseContentType = QByteArrayLiteral("application/json");
                }

                const qint64 elapsedMs = elapsed->elapsed();
                const QByteArray responseHash = Sha256(responseBody);
                const CapturedBody responseCapture = CaptureBody(responseBody, responseContentType);
                const QString responseFileName = SequenceName(sequence) +
                                                 QStringLiteral("-response.") +
                                                 responseCapture.extension;
                if (!WriteFile(QDir(m_captureDirectory).filePath(responseFileName),
                               responseCapture.storedBody)) {
                    qWarning().noquote()
                        << "Bloodborne reference proxy: could not write" << responseFileName;
                }

                AppendManifest(sequence, method, path, sanitizedQuery, requestBody.size(), status,
                               responseBody.size(), requestHash, responseHash, requestContentType,
                               responseContentType, elapsedMs, requestFileName, responseFileName,
                               networkError);
                UpdateSummary(sequence, method, path, status, requestSchema,
                              responseCapture.schema);

                if (m_traceEnabled) {
                    qInfo().noquote()
                        << "[BLOODBORNE REFERENCE RESPONSE]"
                        << "seq=" + QString::number(sequence) << "status=" + QString::number(status)
                        << "body_size=" + QString::number(responseBody.size())
                        << "sha256=" + QString::fromLatin1(responseHash)
                        << "elapsed_ms=" + QString::number(elapsedMs);
                    if (!responseCapture.consoleJson.isEmpty())
                        qInfo().noquote()
                            << "[BLOODBORNE REFERENCE RESPONSE JSON]"
                            << "seq=" + QString::number(sequence) << responseCapture.consoleJson;
                }

                responderPtr->sendResponse(
                    QHttpServerResponse{responseContentType, responseBody,
                                        static_cast<QHttpServerResponse::StatusCode>(status)});
                reply->deleteLater();
            });
    }

private:
    void AppendManifest(quint64 sequence, const QByteArray& method, const QString& path,
                        const QString& query, qsizetype requestSize, int responseStatus,
                        qsizetype responseSize, const QByteArray& requestHash,
                        const QByteArray& responseHash, const QByteArray& requestContentType,
                        const QByteArray& responseContentType, qint64 elapsedMs,
                        const QString& requestFile, const QString& responseFile,
                        const QString& networkError) {
        QJsonObject entry;
        entry.insert(QStringLiteral("sequence"), static_cast<double>(sequence));
        entry.insert(QStringLiteral("method"), QString::fromLatin1(method));
        entry.insert(QStringLiteral("path"), path);
        entry.insert(QStringLiteral("query"), query);
        entry.insert(QStringLiteral("request_size"), static_cast<double>(requestSize));
        entry.insert(QStringLiteral("response_status"), responseStatus);
        entry.insert(QStringLiteral("response_size"), static_cast<double>(responseSize));
        entry.insert(QStringLiteral("request_sha256"), QString::fromLatin1(requestHash));
        entry.insert(QStringLiteral("response_sha256"), QString::fromLatin1(responseHash));
        entry.insert(QStringLiteral("content_type"), QString::fromLatin1(responseContentType));
        entry.insert(QStringLiteral("request_content_type"),
                     QString::fromLatin1(requestContentType));
        entry.insert(QStringLiteral("elapsed_ms"), static_cast<double>(elapsedMs));
        entry.insert(QStringLiteral("request_file"), requestFile);
        entry.insert(QStringLiteral("response_file"), responseFile);
        if (!networkError.isEmpty())
            entry.insert(QStringLiteral("network_error"), networkError);

        QFile manifest(QDir(m_captureDirectory).filePath(QStringLiteral("manifest.jsonl")));
        if (!manifest.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text) ||
            manifest.write(QJsonDocument(entry).toJson(QJsonDocument::Compact) + '\n') < 0) {
            qWarning() << "Bloodborne reference proxy: could not append capture manifest";
        }
    }

    void UpdateSummary(quint64 sequence, const QByteArray& method, const QString& path, int status,
                       const QJsonValue& requestSchema, const QJsonValue& responseSchema) {
        const QString key = QString::fromLatin1(method) + QLatin1Char(' ') + path;
        EndpointSummary& summary = m_summaries[key];
        if (summary.calls == 0) {
            summary.method = QString::fromLatin1(method);
            summary.path = path;
            summary.firstSequence = sequence;
            summary.requestSchema = requestSchema;
            summary.responseSchema = responseSchema;
        }
        ++summary.calls;
        summary.lastSequence = sequence;
        summary.responseStatuses.insert(status);
        if ((summary.requestSchema.isUndefined() || summary.requestSchema.isNull()) &&
            !requestSchema.isUndefined()) {
            summary.requestSchema = requestSchema;
        }
        if ((summary.responseSchema.isUndefined() || summary.responseSchema.isNull()) &&
            !responseSchema.isUndefined()) {
            summary.responseSchema = responseSchema;
        }
        WriteSummary();
    }

    void WriteSummary() const {
        QJsonArray endpoints;
        QString text = QStringLiteral("Bloodborne reference capture summary\n\n");
        for (const EndpointSummary& summary : m_summaries) {
            QJsonObject endpoint;
            endpoint.insert(QStringLiteral("method"), summary.method);
            endpoint.insert(QStringLiteral("path"), summary.path);
            endpoint.insert(QStringLiteral("calls"), static_cast<double>(summary.calls));
            endpoint.insert(QStringLiteral("first_sequence"),
                            static_cast<double>(summary.firstSequence));
            endpoint.insert(QStringLiteral("last_sequence"),
                            static_cast<double>(summary.lastSequence));
            QJsonArray statuses;
            QList<int> sortedStatuses = summary.responseStatuses.values();
            std::sort(sortedStatuses.begin(), sortedStatuses.end());
            for (int status : sortedStatuses)
                statuses.append(status);
            endpoint.insert(QStringLiteral("response_statuses"), statuses);
            endpoint.insert(QStringLiteral("request_schema"), summary.requestSchema);
            endpoint.insert(QStringLiteral("response_schema"), summary.responseSchema);
            endpoints.append(endpoint);

            text += summary.method + QLatin1Char(' ') + summary.path + QLatin1Char('\n');
            text += QStringLiteral("  calls=%1 first_sequence=%2 last_sequence=%3\n")
                        .arg(summary.calls)
                        .arg(summary.firstSequence)
                        .arg(summary.lastSequence);
            QStringList statusText;
            for (int status : sortedStatuses)
                statusText.append(QString::number(status));
            text +=
                QStringLiteral("  response_statuses=%1\n").arg(statusText.join(QLatin1Char(',')));
            text += QStringLiteral("  request_schema=%1\n").arg(SchemaText(summary.requestSchema));
            text +=
                QStringLiteral("  response_schema=%1\n\n").arg(SchemaText(summary.responseSchema));
        }

        QJsonObject root;
        root.insert(QStringLiteral("title"),
                    QStringLiteral("Bloodborne reference capture summary"));
        root.insert(QStringLiteral("endpoints"), endpoints);
        WriteFile(QDir(m_captureDirectory).filePath(QStringLiteral("summary.json")),
                  QJsonDocument(root).toJson(QJsonDocument::Indented));
        WriteFile(QDir(m_captureDirectory).filePath(QStringLiteral("summary.txt")), text.toUtf8());
    }

    ReferenceProxy* m_owner;
    Options m_options;
    bool m_traceEnabled = false;
    QString m_captureDirectory;
    quint64 m_sequence = 0;
    QNetworkAccessManager m_network;
    QMap<QString, EndpointSummary> m_summaries;
};

ReferenceProxy::ReferenceProxy(Options options, QObject* parent)
    : QObject(parent), m_impl(std::make_unique<Impl>(this, std::move(options))) {}

ReferenceProxy::~ReferenceProxy() = default;

bool ReferenceProxy::Initialize(QString* error) {
    return m_impl->Initialize(error);
}

void ReferenceProxy::Forward(const QHttpServerRequest& request, QHttpServerResponder&& responder) {
    m_impl->Forward(request, std::move(responder));
}

QString ReferenceProxy::CaptureDirectory() const {
    return m_impl->CaptureDirectory();
}

bool ReferenceProxy::IsReferenceBackendPath(const QString& path) {
    if (path == QStringLiteral("/bb-eu/ss.info") ||
        path.startsWith(QStringLiteral("/summon_messenger/"))) {
        return false;
    }
    static const std::array<QString, 8> Prefixes{
        QStringLiteral("/basic_utils/"),     QStringLiteral("/penalty/"),
        QStringLiteral("/blood_messenger/"), QStringLiteral("/messenger_shell/"),
        QStringLiteral("/wandering_ghost/"), QStringLiteral("/chair_messenger/"),
        QStringLiteral("/channel/"),         QStringLiteral("/tomb_messenger/"),
    };
    return std::any_of(Prefixes.cbegin(), Prefixes.cend(),
                       [&path](const QString& prefix) { return path.startsWith(prefix); });
}

} // namespace Bloodborne
