// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include "bloodborne_website.h"

#include <algorithm>
#include <cstring>
#include <optional>
#include <utility>

#include <QBuffer>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QHttpHeaders>
#include <QHttpServer>
#include <QHttpServerRequest>
#include <QHttpServerResponder>
#include <QHttpServerResponse>
#include <QImage>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QTcpServer>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>

#include "account_registration.h"
#include "client_session.h"
#include "config.h"
#include "database.h"

namespace {

using StatusCode = QHttpServerResponse::StatusCode;

constexpr qsizetype MaxJsonBody = 16 * 1024;
constexpr qsizetype MaxAvatarBody = 2 * 1024 * 1024;
constexpr qint64 WebSessionLifetimeSeconds = 7 * 24 * 60 * 60;
constexpr int MaxPlayersPerPage = 100;

StatusCode Status(int code) {
    return static_cast<StatusCode>(code);
}

QByteArray HeaderValue(const QHttpServerRequest& request, const QByteArray& wanted) {
    return request.value(wanted);
}

QHttpServerResponse Harden(QHttpServerResponse response, bool noStore = true) {
    QHttpHeaders headers = response.headers();
    headers.append(QByteArrayLiteral("X-Content-Type-Options"), QByteArrayLiteral("nosniff"));
    headers.append(QByteArrayLiteral("X-Frame-Options"), QByteArrayLiteral("DENY"));
    headers.append(QByteArrayLiteral("Referrer-Policy"), QByteArrayLiteral("no-referrer"));
    headers.append(QByteArrayLiteral("Permissions-Policy"),
                   QByteArrayLiteral("camera=(), microphone=(), geolocation=()"));
    headers.append(
        QByteArrayLiteral("Content-Security-Policy"),
        QByteArrayLiteral("default-src 'self'; base-uri 'none'; object-src 'none'; "
                          "frame-ancestors 'none'; form-action 'self'; img-src 'self' data:; "
                          "style-src 'self'; script-src 'self'; connect-src 'self'"));
    headers.append(QByteArrayLiteral("Cache-Control"),
                   noStore ? QByteArrayLiteral("no-store")
                           : QByteArrayLiteral("public, max-age=86400"));
    response.setHeaders(std::move(headers));
    return response;
}

QHttpServerResponse JsonResponse(const QJsonObject& object, StatusCode status = Status(200)) {
    return Harden(QHttpServerResponse{QByteArrayLiteral("application/json; charset=utf-8"),
                                      QJsonDocument(object).toJson(QJsonDocument::Compact),
                                      status});
}

QHttpServerResponse JsonData(const QJsonValue& data, StatusCode status = Status(200)) {
    QJsonObject root;
    root.insert(QStringLiteral("ok"), true);
    root.insert(QStringLiteral("data"), data);
    return JsonResponse(root, status);
}

QHttpServerResponse JsonError(StatusCode status, const QString& code, const QString& message) {
    QJsonObject error;
    error.insert(QStringLiteral("code"), code);
    error.insert(QStringLiteral("message"), message);
    QJsonObject root;
    root.insert(QStringLiteral("ok"), false);
    root.insert(QStringLiteral("error"), error);
    return JsonResponse(root, status);
}

std::optional<QJsonObject> ParseJsonObject(const QHttpServerRequest& request) {
    if (request.body().isEmpty() || request.body().size() > MaxJsonBody)
        return std::nullopt;
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(request.body(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return std::nullopt;
    return document.object();
}

QByteArray RandomToken() {
    QByteArray bytes(32, Qt::Uninitialized);
    for (qsizetype offset = 0; offset < bytes.size(); offset += 4) {
        const quint32 random = QRandomGenerator::system()->generate();
        const qsizetype count = std::min<qsizetype>(4, bytes.size() - offset);
        memcpy(bytes.data() + offset, &random, static_cast<size_t>(count));
    }
    return bytes.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
}

QByteArray HashToken(const QByteArray& token) {
    return QCryptographicHash::hash(token, QCryptographicHash::Sha256);
}

QByteArray CsrfToken(const QByteArray& sessionToken) {
    return QCryptographicHash::hash(QByteArrayLiteral("thr-csrf-v1:") + sessionToken,
                                    QCryptographicHash::Sha256)
        .toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
}

QByteArray CookieValue(const QHttpServerRequest& request, const QByteArray& name) {
    const QByteArray cookieHeader = HeaderValue(request, QByteArrayLiteral("Cookie"));
    for (QByteArray item : cookieHeader.split(';')) {
        item = item.trimmed();
        const qsizetype equals = item.indexOf('=');
        if (equals > 0 && item.left(equals) == name)
            return item.mid(equals + 1);
    }
    return {};
}

bool IsHttpsRequest(const QHttpServerRequest& request) {
    return HeaderValue(request, QByteArrayLiteral("X-Forwarded-Proto"))
                   .compare(QByteArrayLiteral("https"), Qt::CaseInsensitive) == 0 ||
           request.url().scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0;
}

QByteArray SessionCookie(const QByteArray& token, bool secure, bool clear = false) {
    QByteArray cookie = QByteArrayLiteral("thr_session=") + token +
                        QByteArrayLiteral("; Path=/; ") +
                        QByteArrayLiteral("HttpOnly; SameSite=Lax; ");
    if (secure)
        cookie += QByteArrayLiteral("Secure; ");
    if (clear)
        cookie += QByteArrayLiteral("Max-Age=0");
    else
        cookie += QByteArrayLiteral("Max-Age=604800");
    return cookie;
}

QHttpServerResponse WithCookie(QHttpServerResponse response, const QByteArray& cookie) {
    QHttpHeaders headers = response.headers();
    headers.append(QByteArrayLiteral("Set-Cookie"), cookie);
    response.setHeaders(std::move(headers));
    return response;
}

bool SameOrigin(const QHttpServerRequest& request) {
    const QByteArray originHeader = HeaderValue(request, QByteArrayLiteral("Origin"));
    if (originHeader.isEmpty())
        return true;
    const QUrl origin(QString::fromUtf8(originHeader));
    if (!origin.isValid() || origin.host().isEmpty())
        return false;
    QByteArray host = HeaderValue(request, QByteArrayLiteral("X-Forwarded-Host"));
    if (host.isEmpty())
        host = HeaderValue(request, QByteArrayLiteral("Host"));
    return !host.isEmpty() &&
           origin.authority().compare(QString::fromUtf8(host), Qt::CaseInsensitive) == 0;
}

QByteArray ReadResource(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}

QString EscapeLike(QString text) {
    text.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    text.replace(QStringLiteral("%"), QStringLiteral("\\%"));
    text.replace(QStringLiteral("_"), QStringLiteral("\\_"));
    return text;
}

bool ValidAvatarFile(const QString& fileName) {
    if (!fileName.endsWith(QStringLiteral(".png"), Qt::CaseInsensitive))
        return false;
    const QString uuidText = fileName.left(fileName.size() - 4);
    const QUuid uuid(uuidText);
    return !uuid.isNull() &&
           uuid.toString(QUuid::WithoutBraces).compare(uuidText, Qt::CaseInsensitive) == 0;
}

} // namespace

class BloodborneWebsiteServer::Impl {
public:
    explicit Impl(BloodborneWebsiteServer* owner) : m_owner(owner) {}

    struct AuthenticatedSession {
        qint64 userId = 0;
        QString username;
        QByteArray rawToken;
        QByteArray csrf;
    };

    bool Start(ConfigManager* config, const QString& dbPath, SharedState* shared) {
        m_config = config;
        m_shared = shared;
        if (!m_config->IsBloodborneWebsiteEnabled()) {
            qInfo() << "Bloodborne website disabled; listener not started";
            return true;
        }

        bool portOk = false;
        const quint16 requestedPort = m_config->GetBloodborneWebsitePort().toUShort(&portOk);
        if (!portOk || m_config->GetBloodborneWebsitePort().isEmpty()) {
            qWarning() << "Bloodborne website port is invalid:"
                       << m_config->GetBloodborneWebsitePort();
            return false;
        }
        if (requestedPort != 0 && requestedPort == m_config->GetWebApiPort().toUShort()) {
            qWarning() << "Bloodborne website port must differ from WebApiPort";
            return false;
        }

        QDir dataBase = QFileInfo(dbPath).absoluteDir();
        if (dataBase.dirName().compare(QStringLiteral("db"), Qt::CaseInsensitive) == 0)
            dataBase.cdUp();
        m_dataRoot = dataBase.filePath(QStringLiteral("data/bloodborne-website"));
        m_avatarDirectory = QDir(m_dataRoot).filePath(QStringLiteral("avatars"));
        if (!QDir().mkpath(m_avatarDirectory)) {
            qWarning() << "Bloodborne website could not create data directory:" << m_dataRoot;
            return false;
        }

        m_db = std::make_unique<Database>(QStringLiteral("bloodborne_website_main"));
        if (!m_db->Open(dbPath)) {
            qWarning() << "Bloodborne website failed to open database:" << dbPath;
            return false;
        }
        CloseInterruptedPlayerSessions();
        PurgeExpiredWebSessions();

        m_http = std::make_unique<QHttpServer>(m_owner);
        RegisterRoutes();
        m_tcp = std::make_unique<QTcpServer>(m_owner);

        QHostAddress address;
        const QString host = m_config->GetHost();
        if (host.isEmpty() || host == QStringLiteral("0.0.0.0"))
            address = QHostAddress::AnyIPv4;
        else
            address = QHostAddress(host);
        if (!m_tcp->listen(address, requestedPort)) {
            qWarning() << "Bloodborne website failed to bind" << host << ":" << requestedPort << "-"
                       << m_tcp->errorString();
            Stop();
            return false;
        }
        if (!m_http->bind(m_tcp.get())) {
            qWarning() << "Bloodborne website failed to attach HTTP listener";
            Stop();
            return false;
        }

        qInfo().nospace().noquote()
            << "Bloodborne website listening on: " << host << ":" << m_tcp->serverPort();
        qInfo() << "Bloodborne website registration"
                << (m_config->IsBloodborneWebsiteRegistrationEnabled() ? "enabled" : "disabled");
        return true;
    }

    void Stop() {
        if (m_tcp && m_tcp->isListening())
            m_tcp->close();
        ClosePlayerSessionsAtShutdown();
        // QHttpServer::bind() reparents the TCP server to the HTTP server.
        // Destroy the child first so both unique_ptr instances remain safe.
        m_tcp.reset();
        m_http.reset();
        m_db.reset();
    }

    bool IsListening() const {
        return m_tcp && m_tcp->isListening();
    }

    quint16 ListeningPort() const {
        return m_tcp ? m_tcp->serverPort() : 0;
    }

    void RecordPlayerAuthenticated(qint64 userId, const QString& username, bool publicVisible) {
        if (!m_db || userId <= 0)
            return;
        const qint64 now = QDateTime::currentSecsSinceEpoch();
        QSqlDatabase db = m_db->Conn();
        QSqlQuery closeExisting(db);
        closeExisting.prepare(QStringLiteral(
            "UPDATE bloodborne_player_session SET ended_at=started_at,duration_seconds=0,"
            "interrupted=1 WHERE user_id=? AND ended_at IS NULL"));
        closeExisting.addBindValue(userId);
        closeExisting.exec();

        QSqlQuery insert(db);
        insert.prepare(QStringLiteral(
            "INSERT INTO bloodborne_player_session(user_id,started_at,public_visible) "
            "VALUES(?,?,?)"));
        insert.addBindValue(userId);
        insert.addBindValue(now);
        insert.addBindValue(publicVisible ? 1 : 0);
        if (!insert.exec()) {
            qWarning() << "Bloodborne website failed to begin player session:"
                       << insert.lastError().text();
            return;
        }
        if (publicVisible)
            InsertActivity(userId, QStringLiteral("player_connected"), now);
        Q_UNUSED(username);
    }

    void RecordPlayerDisconnected(qint64 userId, const QString& username, bool publicVisible) {
        if (!m_db || userId <= 0)
            return;
        const qint64 now = QDateTime::currentSecsSinceEpoch();
        QSqlQuery query(m_db->Conn());
        query.prepare(QStringLiteral(
            "UPDATE bloodborne_player_session SET ended_at=?,"
            "duration_seconds=MAX(0,?-started_at) WHERE player_session_id=("
            "SELECT player_session_id FROM bloodborne_player_session WHERE user_id=? "
            "AND ended_at IS NULL ORDER BY started_at DESC LIMIT 1)"));
        query.addBindValue(now);
        query.addBindValue(now);
        query.addBindValue(userId);
        if (!query.exec()) {
            qWarning() << "Bloodborne website failed to close player session:"
                       << query.lastError().text();
        }
        if (publicVisible)
            InsertActivity(userId, QStringLiteral("player_disconnected"), now);
        Q_UNUSED(username);
    }

private:
    void RegisterRoutes() {
        const auto shell = [this](const QHttpServerRequest&) { return StaticShell(); };
        m_http->route(QStringLiteral("/"), QHttpServerRequest::Method::Get, shell);
        m_http->route(QStringLiteral("/register"), QHttpServerRequest::Method::Get, shell);
        m_http->route(QStringLiteral("/login"), QHttpServerRequest::Method::Get, shell);
        m_http->route(QStringLiteral("/players"), QHttpServerRequest::Method::Get, shell);
        m_http->route(QStringLiteral("/account"), QHttpServerRequest::Method::Get, shell);
        m_http->route(QStringLiteral("/player/<arg>"), QHttpServerRequest::Method::Get,
                      [this](const QString&, const QHttpServerRequest&) { return StaticShell(); });

        m_http->route(QStringLiteral("/assets/site.css"), QHttpServerRequest::Method::Get,
                      [](const QHttpServerRequest&) {
                          return StaticResource(QStringLiteral(":/website/site.css"),
                                                QByteArrayLiteral("text/css; charset=utf-8"));
                      });
        m_http->route(QStringLiteral("/assets/site.js"), QHttpServerRequest::Method::Get,
                      [](const QHttpServerRequest&) {
                          return StaticResource(QStringLiteral(":/website/site.js"),
                                                QByteArrayLiteral("application/javascript; "
                                                                  "charset=utf-8"));
                      });
        m_http->route(QStringLiteral("/assets/requiem-hero.jpg"), QHttpServerRequest::Method::Get,
                      [](const QHttpServerRequest&) {
                          return StaticResource(QStringLiteral(":/website/requiem-hero.jpg"),
                                                QByteArrayLiteral("image/jpeg"));
                      });
        m_http->route(QStringLiteral("/assets/requiem-emblem.png"), QHttpServerRequest::Method::Get,
                      [](const QHttpServerRequest&) {
                          return StaticResource(QStringLiteral(":/website/requiem-emblem.png"),
                                                QByteArrayLiteral("image/png"));
                      });
        m_http->route(QStringLiteral("/avatars/<arg>"), QHttpServerRequest::Method::Get,
                      [this](const QString& fileName, const QHttpServerRequest&) {
                          return Avatar(fileName);
                      });

        m_http->route(QStringLiteral("/api/status"), QHttpServerRequest::Method::Get,
                      [this](const QHttpServerRequest&) { return ApiStatus(); });
        m_http->route(QStringLiteral("/api/players"), QHttpServerRequest::Method::Get,
                      [this](const QHttpServerRequest& request) { return ApiPlayers(request); });
        m_http->route(QStringLiteral("/api/players/<arg>"), QHttpServerRequest::Method::Get,
                      [this](const QString& username, const QHttpServerRequest&) {
                          return ApiPlayer(username);
                      });
        m_http->route(QStringLiteral("/api/activity"), QHttpServerRequest::Method::Get,
                      [this](const QHttpServerRequest& request) { return ApiActivity(request); });
        m_http->route(QStringLiteral("/api/register"), QHttpServerRequest::Method::Post,
                      [this](const QHttpServerRequest& request) { return ApiRegister(request); });
        m_http->route(QStringLiteral("/api/login"), QHttpServerRequest::Method::Post,
                      [this](const QHttpServerRequest& request) { return ApiLogin(request); });
        m_http->route(QStringLiteral("/api/logout"), QHttpServerRequest::Method::Post,
                      [this](const QHttpServerRequest& request) { return ApiLogout(request); });
        m_http->route(QStringLiteral("/api/account"), QHttpServerRequest::Method::Get,
                      [this](const QHttpServerRequest& request) { return ApiAccount(request); });
        m_http->route(QStringLiteral("/api/account/avatar"), QHttpServerRequest::Method::Post,
                      [this](const QHttpServerRequest& request) { return ApiAvatar(request); });

        m_http->setMissingHandler(
            m_owner, [](const QHttpServerRequest& request, QHttpServerResponder& responder) {
                if (request.url().path().startsWith(QStringLiteral("/api/"))) {
                    responder.sendResponse(JsonError(Status(404), QStringLiteral("not_found"),
                                                     QStringLiteral("Endpoint not found")));
                } else {
                    responder.sendResponse(JsonError(Status(404), QStringLiteral("not_found"),
                                                     QStringLiteral("Page not found")));
                }
            });
    }

    static QHttpServerResponse StaticResource(const QString& path, const QByteArray& contentType) {
        const QByteArray bytes = ReadResource(path);
        if (bytes.isEmpty())
            return JsonError(Status(404), QStringLiteral("asset_not_found"),
                             QStringLiteral("Asset not found"));
        return Harden(QHttpServerResponse{contentType, bytes, Status(200)}, false);
    }

    QHttpServerResponse StaticShell() const {
        const QByteArray bytes = ReadResource(QStringLiteral(":/website/index.html"));
        if (bytes.isEmpty())
            return JsonError(Status(500), QStringLiteral("website_unavailable"),
                             QStringLiteral("Website unavailable"));
        return Harden(
            QHttpServerResponse{QByteArrayLiteral("text/html; charset=utf-8"), bytes, Status(200)});
    }

    QHttpServerResponse Avatar(const QString& fileName) const {
        if (!ValidAvatarFile(fileName))
            return JsonError(Status(404), QStringLiteral("avatar_not_found"),
                             QStringLiteral("Avatar not found"));
        const QString path = QDir(m_avatarDirectory).filePath(fileName);
        const QFileInfo info(path);
        if (!info.isFile() || info.isSymLink() ||
            info.absolutePath() != QFileInfo(m_avatarDirectory).absoluteFilePath()) {
            return JsonError(Status(404), QStringLiteral("avatar_not_found"),
                             QStringLiteral("Avatar not found"));
        }
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return JsonError(Status(404), QStringLiteral("avatar_not_found"),
                             QStringLiteral("Avatar not found"));
        return Harden(
            QHttpServerResponse{QByteArrayLiteral("image/png"), file.readAll(), Status(200)},
            false);
    }

    QHttpServerResponse ApiStatus() const {
        int online = 0;
        {
            QReadLocker lock(&m_shared->clientsLock);
            for (auto it = m_shared->clients.cbegin(); it != m_shared->clients.cend(); ++it) {
                if (!it->appearOffline)
                    ++online;
            }
        }

        int activeCoopRooms = 0;
        {
            QReadLocker lock(&m_shared->matching.roomsLock);
            for (auto it = m_shared->matching.rooms.cbegin(); it != m_shared->matching.rooms.cend();
                 ++it) {
                if (it->members.size() >= 2)
                    ++activeCoopRooms;
            }
        }

        qint64 registered = 0;
        qint64 messages = 0;
        QSqlQuery query(m_db->Conn());
        if (query.exec(QStringLiteral("SELECT COUNT(*) FROM account")) && query.next())
            registered = query.value(0).toLongLong();
        if (query.exec(QStringLiteral("SELECT COUNT(*) FROM bloodborne_blood_message")) &&
            query.next()) {
            messages = query.value(0).toLongLong();
        }

        QJsonObject definitions;
        definitions.insert(QStringLiteral("huntersOnline"),
                           QStringLiteral("authenticated visible TCP clients"));
        definitions.insert(QStringLiteral("registeredHunters"),
                           QStringLiteral("rows in the shadNet account table"));
        definitions.insert(QStringLiteral("coOpSessions"),
                           QStringLiteral("active Matching2 rooms with at least two members"));
        definitions.insert(QStringLiteral("messages"),
                           QStringLiteral("currently stored Blood Messenger messages"));

        QJsonObject data;
        data.insert(QStringLiteral("name"), QStringLiteral("The Hunter's Requiem"));
        data.insert(QStringLiteral("huntersOnline"), online);
        data.insert(QStringLiteral("registeredHunters"), registered);
        data.insert(QStringLiteral("coOpSessions"), activeCoopRooms);
        data.insert(QStringLiteral("messages"), messages);
        data.insert(QStringLiteral("registrationEnabled"),
                    m_config->IsBloodborneWebsiteRegistrationEnabled());
        data.insert(QStringLiteral("definitions"), definitions);
        return JsonData(data);
    }

    QHttpServerResponse ApiPlayers(const QHttpServerRequest& request) const {
        const QUrlQuery queryItems(request.url());
        const QString search = queryItems.queryItemValue(QStringLiteral("search")).trimmed();
        const bool onlineOnly =
            queryItems.queryItemValue(QStringLiteral("online")) == QStringLiteral("true");
        bool limitOk = false;
        int limit = queryItems.queryItemValue(QStringLiteral("limit")).toInt(&limitOk);
        if (!limitOk)
            limit = 50;
        limit = std::clamp(limit, 1, MaxPlayersPerPage);

        QString sql = QStringLiteral("SELECT a.user_id,a.username,t.creation,t.last_login,"
                                     "COALESCE(p.avatar_file,'') FROM account a "
                                     "LEFT JOIN account_timestamp t ON t.user_id=a.user_id "
                                     "LEFT JOIN bloodborne_web_profile p ON p.user_id=a.user_id ");
        if (!search.isEmpty())
            sql += QStringLiteral("WHERE a.username LIKE ? ESCAPE '\\' COLLATE NOCASE ");
        sql += QStringLiteral("ORDER BY a.username COLLATE NOCASE LIMIT 200");

        QSqlQuery query(m_db->Conn());
        query.prepare(sql);
        if (!search.isEmpty())
            query.addBindValue(QStringLiteral("%") + EscapeLike(search) + QStringLiteral("%"));
        if (!query.exec())
            return JsonError(Status(500), QStringLiteral("database_error"),
                             QStringLiteral("Unable to list players"));

        QJsonArray players;
        while (query.next() && players.size() < limit) {
            const auto player = PlayerObject(
                query.value(0).toLongLong(), query.value(1).toString(), query.value(2).toLongLong(),
                query.value(3).toLongLong(), query.value(4).toString());
            if (!player || (onlineOnly && !player->value(QStringLiteral("online")).toBool()))
                continue;
            players.append(*player);
        }

        QJsonObject data;
        data.insert(QStringLiteral("players"), players);
        data.insert(QStringLiteral("count"), players.size());
        return JsonData(data);
    }

    QHttpServerResponse ApiPlayer(const QString& username) const {
        if (username.isEmpty() || username.size() > 64)
            return JsonError(Status(404), QStringLiteral("player_not_found"),
                             QStringLiteral("Player not found"));
        QSqlQuery query(m_db->Conn());
        query.prepare(QStringLiteral(
            "SELECT a.user_id,a.username,t.creation,t.last_login,COALESCE(p.avatar_file,'') "
            "FROM account a LEFT JOIN account_timestamp t ON t.user_id=a.user_id "
            "LEFT JOIN bloodborne_web_profile p ON p.user_id=a.user_id "
            "WHERE a.username=? COLLATE NOCASE"));
        query.addBindValue(username);
        if (!query.exec() || !query.next())
            return JsonError(Status(404), QStringLiteral("player_not_found"),
                             QStringLiteral("Player not found"));
        const auto player = PlayerObject(query.value(0).toLongLong(), query.value(1).toString(),
                                         query.value(2).toLongLong(), query.value(3).toLongLong(),
                                         query.value(4).toString());
        if (!player)
            return JsonError(Status(500), QStringLiteral("database_error"),
                             QStringLiteral("Unable to load player"));
        return JsonData(*player);
    }

    QHttpServerResponse ApiActivity(const QHttpServerRequest& request) const {
        bool ok = false;
        int limit = QUrlQuery(request.url()).queryItemValue(QStringLiteral("limit")).toInt(&ok);
        if (!ok)
            limit = 20;
        limit = std::clamp(limit, 1, 20);
        QSqlQuery query(m_db->Conn());
        query.prepare(
            QStringLiteral("SELECT x.event_type,COALESCE(a.username,''),x.created_at "
                           "FROM bloodborne_activity x LEFT JOIN account a ON a.user_id=x.user_id "
                           "ORDER BY x.created_at DESC,x.activity_id DESC LIMIT ?"));
        query.addBindValue(limit);
        if (!query.exec())
            return JsonError(Status(500), QStringLiteral("database_error"),
                             QStringLiteral("Unable to load activity"));
        QJsonArray activity;
        while (query.next()) {
            QJsonObject item;
            item.insert(QStringLiteral("type"), query.value(0).toString());
            item.insert(QStringLiteral("username"), query.value(1).toString());
            item.insert(QStringLiteral("occurredAt"), query.value(2).toLongLong());
            activity.append(item);
        }
        QJsonObject data;
        data.insert(QStringLiteral("activity"), activity);
        return JsonData(data);
    }

    QHttpServerResponse ApiRegister(const QHttpServerRequest& request) {
        if (!m_config->IsBloodborneWebsiteRegistrationEnabled())
            return JsonError(Status(403), QStringLiteral("registration_closed"),
                             QStringLiteral("Registration is closed"));
        if (!SameOrigin(request))
            return JsonError(Status(403), QStringLiteral("origin_rejected"),
                             QStringLiteral("Request origin rejected"));
        if (RateLimited(request, QStringLiteral("register"), 3, 60))
            return JsonError(Status(429), QStringLiteral("rate_limited"),
                             QStringLiteral("Please wait before trying again"));
        const auto body = ParseJsonObject(request);
        if (!body)
            return JsonError(Status(400), QStringLiteral("invalid_request"),
                             QStringLiteral("Invalid registration request"));

        const QString username = body->value(QStringLiteral("username")).toString().trimmed();
        const QString password = body->value(QStringLiteral("password")).toString();
        const QString confirmation = body->value(QStringLiteral("confirmPassword")).toString();
        if (!ClientSession::IsValidNpid(username))
            return JsonError(Status(400), QStringLiteral("invalid_username"),
                             QStringLiteral("Username must be 3-16 letters, numbers, - or _"));
        if (password.toUtf8().size() < 8 || password.toUtf8().size() > 128)
            return JsonError(Status(400), QStringLiteral("invalid_password"),
                             QStringLiteral("Password must be 8-128 UTF-8 bytes"));
        if (password != confirmation)
            return JsonError(Status(400), QStringLiteral("password_mismatch"),
                             QStringLiteral("Passwords do not match"));

        const QString internalEmail = username.toLower() + QStringLiteral("-") +
                                      QUuid::createUuid().toString(QUuid::WithoutBraces) +
                                      QStringLiteral("@website.invalid");
        const AccountRegistrationRequest registration{
            username, password, internalEmail,
            QStringLiteral("https://shadps4.net/shadnet/avatars/default_01.png")};
        const AccountRegistrationError error =
            RegisterShadNetAccount(registration, *m_config, *m_db);
        if (error != AccountRegistrationError::None) {
            if (error == AccountRegistrationError::ExistingUsername)
                return JsonError(Status(409), QStringLiteral("username_unavailable"),
                                 QStringLiteral("Username is unavailable"));
            return JsonError(Status(400), QStringLiteral("registration_failed"),
                             QStringLiteral("Unable to create account"));
        }
        qInfo().noquote() << "Web account created user=" + username;
        QJsonObject data;
        data.insert(QStringLiteral("username"), username);
        return JsonData(data, Status(201));
    }

    QHttpServerResponse ApiLogin(const QHttpServerRequest& request) {
        if (!SameOrigin(request))
            return JsonError(Status(403), QStringLiteral("origin_rejected"),
                             QStringLiteral("Request origin rejected"));
        if (RateLimited(request, QStringLiteral("login"), 8, 300))
            return JsonError(Status(429), QStringLiteral("rate_limited"),
                             QStringLiteral("Please wait before trying again"));
        const auto body = ParseJsonObject(request);
        if (!body)
            return JsonError(Status(400), QStringLiteral("invalid_request"),
                             QStringLiteral("Invalid login request"));
        const QString username = body->value(QStringLiteral("username")).toString().trimmed();
        const QString password = body->value(QStringLiteral("password")).toString();
        if (username.size() > 64 || password.toUtf8().size() > 256)
            return JsonError(Status(401), QStringLiteral("invalid_credentials"),
                             QStringLiteral("Invalid username or password"));
        const auto user = m_db->CheckUser(username, password, QString(), false);
        if (!user || user->banned)
            return JsonError(Status(401), QStringLiteral("invalid_credentials"),
                             QStringLiteral("Invalid username or password"));

        const QByteArray token = RandomToken();
        const qint64 now = QDateTime::currentSecsSinceEpoch();
        QSqlQuery query(m_db->Conn());
        query.prepare(QStringLiteral(
            "INSERT INTO bloodborne_web_session(token_hash,user_id,created_at,last_seen_at,"
            "expires_at) VALUES(?,?,?,?,?)"));
        query.addBindValue(HashToken(token));
        query.addBindValue(static_cast<qlonglong>(user->userId));
        query.addBindValue(now);
        query.addBindValue(now);
        query.addBindValue(now + WebSessionLifetimeSeconds);
        if (!query.exec())
            return JsonError(Status(500), QStringLiteral("login_failed"),
                             QStringLiteral("Unable to start web session"));
        qInfo().noquote() << "Web login successful user=" + user->username;

        QJsonObject data;
        data.insert(QStringLiteral("username"), user->username);
        data.insert(QStringLiteral("csrfToken"), QString::fromLatin1(CsrfToken(token)));
        return WithCookie(JsonData(data), SessionCookie(token, IsHttpsRequest(request)));
    }

    QHttpServerResponse ApiLogout(const QHttpServerRequest& request) {
        const auto session = Authenticate(request);
        if (!session)
            return JsonError(Status(401), QStringLiteral("authentication_required"),
                             QStringLiteral("Authentication required"));
        if (!SameOrigin(request) || !ValidCsrf(request, *session))
            return JsonError(Status(403), QStringLiteral("csrf_rejected"),
                             QStringLiteral("Request verification failed"));
        QSqlQuery query(m_db->Conn());
        query.prepare(QStringLiteral("DELETE FROM bloodborne_web_session WHERE token_hash=?"));
        query.addBindValue(HashToken(session->rawToken));
        query.exec();
        return WithCookie(JsonData(QJsonObject{}),
                          SessionCookie({}, IsHttpsRequest(request), true));
    }

    QHttpServerResponse ApiAccount(const QHttpServerRequest& request) {
        const auto session = Authenticate(request);
        if (!session)
            return JsonError(Status(401), QStringLiteral("authentication_required"),
                             QStringLiteral("Authentication required"));
        QSqlQuery query(m_db->Conn());
        query.prepare(QStringLiteral(
            "SELECT a.user_id,a.username,t.creation,t.last_login,COALESCE(p.avatar_file,'') "
            "FROM account a LEFT JOIN account_timestamp t ON t.user_id=a.user_id "
            "LEFT JOIN bloodborne_web_profile p ON p.user_id=a.user_id WHERE a.user_id=?"));
        query.addBindValue(session->userId);
        if (!query.exec() || !query.next())
            return JsonError(Status(500), QStringLiteral("database_error"),
                             QStringLiteral("Unable to load account"));
        const auto player = PlayerObject(query.value(0).toLongLong(), query.value(1).toString(),
                                         query.value(2).toLongLong(), query.value(3).toLongLong(),
                                         query.value(4).toString());
        if (!player)
            return JsonError(Status(500), QStringLiteral("database_error"),
                             QStringLiteral("Unable to load account"));
        QJsonObject data = *player;
        data.insert(QStringLiteral("csrfToken"), QString::fromLatin1(session->csrf));
        return JsonData(data);
    }

    QHttpServerResponse ApiAvatar(const QHttpServerRequest& request) {
        const auto session = Authenticate(request);
        if (!session)
            return JsonError(Status(401), QStringLiteral("authentication_required"),
                             QStringLiteral("Authentication required"));
        if (!SameOrigin(request) || !ValidCsrf(request, *session))
            return JsonError(Status(403), QStringLiteral("csrf_rejected"),
                             QStringLiteral("Request verification failed"));
        if (request.body().isEmpty())
            return JsonError(Status(400), QStringLiteral("invalid_avatar"),
                             QStringLiteral("Avatar image is required"));
        if (request.body().size() > MaxAvatarBody)
            return JsonError(Status(413), QStringLiteral("avatar_too_large"),
                             QStringLiteral("Avatar must be 2 MB or smaller"));

        const QByteArray contentType =
            HeaderValue(request, QByteArrayLiteral("Content-Type")).toLower();
        if (!contentType.startsWith(QByteArrayLiteral("image/png")) &&
            !contentType.startsWith(QByteArrayLiteral("image/jpeg")) &&
            !contentType.startsWith(QByteArrayLiteral("image/webp"))) {
            return JsonError(Status(415), QStringLiteral("invalid_avatar_type"),
                             QStringLiteral("Avatar must be PNG, JPEG or WebP"));
        }

        QBuffer input;
        input.setData(request.body());
        input.open(QIODevice::ReadOnly);
        QImageReader reader(&input);
        reader.setDecideFormatFromContent(true);
        const QByteArray format = reader.format().toLower();
        if (format != QByteArrayLiteral("png") && format != QByteArrayLiteral("jpeg") &&
            format != QByteArrayLiteral("jpg") && format != QByteArrayLiteral("webp")) {
            return JsonError(Status(415), QStringLiteral("invalid_avatar_type"),
                             QStringLiteral("Avatar content is not a supported image"));
        }
        const QSize sourceSize = reader.size();
        if (!sourceSize.isValid() || sourceSize.width() > 8192 || sourceSize.height() > 8192 ||
            static_cast<qint64>(sourceSize.width()) * sourceSize.height() > 40'000'000) {
            return JsonError(Status(400), QStringLiteral("invalid_avatar_dimensions"),
                             QStringLiteral("Avatar dimensions are invalid"));
        }
        QImage image = reader.read();
        if (image.isNull())
            return JsonError(Status(400), QStringLiteral("invalid_avatar"),
                             QStringLiteral("Avatar content could not be decoded"));
        if (image.width() > 512 || image.height() > 512) {
            image = image.scaled(512, 512, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        image = image.convertToFormat(QImage::Format_ARGB32);

        QByteArray normalized;
        QBuffer output(&normalized);
        output.open(QIODevice::WriteOnly);
        if (!image.save(&output, "PNG"))
            return JsonError(Status(500), QStringLiteral("avatar_processing_failed"),
                             QStringLiteral("Unable to process avatar"));

        const QString fileName =
            QUuid::createUuid().toString(QUuid::WithoutBraces) + QStringLiteral(".png");
        const QString path = QDir(m_avatarDirectory).filePath(fileName);
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(normalized) != normalized.size() ||
            !file.commit()) {
            return JsonError(Status(500), QStringLiteral("avatar_storage_failed"),
                             QStringLiteral("Unable to store avatar"));
        }

        QString previous;
        QSqlQuery old(m_db->Conn());
        old.prepare(
            QStringLiteral("SELECT avatar_file FROM bloodborne_web_profile WHERE user_id=?"));
        old.addBindValue(session->userId);
        if (old.exec() && old.next())
            previous = old.value(0).toString();

        QSqlQuery update(m_db->Conn());
        update.prepare(QStringLiteral(
            "INSERT INTO bloodborne_web_profile(user_id,avatar_file,updated_at) VALUES(?,?,?) "
            "ON CONFLICT(user_id) DO UPDATE SET avatar_file=excluded.avatar_file,"
            "updated_at=excluded.updated_at"));
        update.addBindValue(session->userId);
        update.addBindValue(fileName);
        update.addBindValue(QDateTime::currentSecsSinceEpoch());
        if (!update.exec()) {
            QFile::remove(path);
            return JsonError(Status(500), QStringLiteral("avatar_storage_failed"),
                             QStringLiteral("Unable to store avatar"));
        }
        if (ValidAvatarFile(previous) && previous != fileName)
            QFile::remove(QDir(m_avatarDirectory).filePath(previous));

        qInfo().noquote() << "Web avatar updated user=" + session->username
                          << "bytes=" + QString::number(normalized.size());
        QJsonObject data;
        data.insert(QStringLiteral("avatarUrl"), QStringLiteral("/avatars/") + fileName);
        return JsonData(data);
    }

    std::optional<AuthenticatedSession> Authenticate(const QHttpServerRequest& request) {
        const QByteArray token = CookieValue(request, QByteArrayLiteral("thr_session"));
        if (token.size() < 32 || token.size() > 128)
            return std::nullopt;
        const qint64 now = QDateTime::currentSecsSinceEpoch();
        QSqlQuery query(m_db->Conn());
        query.prepare(QStringLiteral(
            "SELECT a.user_id,a.username,a.banned FROM bloodborne_web_session s "
            "JOIN account a ON a.user_id=s.user_id WHERE s.token_hash=? AND s.expires_at>?"));
        query.addBindValue(HashToken(token));
        query.addBindValue(now);
        if (!query.exec() || !query.next() || query.value(2).toBool())
            return std::nullopt;

        AuthenticatedSession session;
        session.userId = query.value(0).toLongLong();
        session.username = query.value(1).toString();
        session.rawToken = token;
        session.csrf = CsrfToken(token);

        QSqlQuery touch(m_db->Conn());
        touch.prepare(
            QStringLiteral("UPDATE bloodborne_web_session SET last_seen_at=? WHERE token_hash=?"));
        touch.addBindValue(now);
        touch.addBindValue(HashToken(token));
        touch.exec();
        return session;
    }

    bool ValidCsrf(const QHttpServerRequest& request, const AuthenticatedSession& session) const {
        return QCryptographicHash::hash(HeaderValue(request, QByteArrayLiteral("X-CSRF-Token")),
                                        QCryptographicHash::Sha256) ==
               QCryptographicHash::hash(session.csrf, QCryptographicHash::Sha256);
    }

    bool RateLimited(const QHttpServerRequest& request, const QString& action, int maximum,
                     int windowSeconds) {
        const qint64 now = QDateTime::currentSecsSinceEpoch();
        const QString key = action + QStringLiteral(":") + request.remoteAddress().toString();
        QMutexLocker lock(&m_rateMutex);
        QList<qint64>& events = m_rateEvents[key];
        while (!events.isEmpty() && events.front() <= now - windowSeconds)
            events.pop_front();
        if (events.size() >= maximum)
            return true;
        events.append(now);
        return false;
    }

    std::optional<QJsonObject> PlayerObject(qint64 userId, const QString& username,
                                            qint64 registeredAt, qint64 accountLastLogin,
                                            const QString& avatarFile) const {
        bool online = false;
        {
            QReadLocker lock(&m_shared->clientsLock);
            const auto client = m_shared->clients.constFind(userId);
            online = client != m_shared->clients.cend() && !client->appearOffline;
        }

        QSqlQuery sessions(m_db->Conn());
        sessions.prepare(
            QStringLiteral("SELECT COUNT(*),COALESCE(SUM(duration_seconds),0),"
                           "MAX(CASE WHEN ended_at IS NULL THEN started_at END),"
                           "MAX(COALESCE(ended_at,started_at)) FROM bloodborne_player_session "
                           "WHERE user_id=?"));
        sessions.addBindValue(userId);
        if (!sessions.exec() || !sessions.next())
            return std::nullopt;
        const qint64 totalSessions = sessions.value(0).toLongLong();
        qint64 totalSeconds = sessions.value(1).toLongLong();
        const qint64 currentStartedAt = sessions.value(2).toLongLong();
        qint64 lastSeen = std::max(accountLastLogin, sessions.value(3).toLongLong());
        qint64 currentSeconds = 0;
        if (online && currentStartedAt > 0) {
            currentSeconds =
                std::max<qint64>(0, QDateTime::currentSecsSinceEpoch() - currentStartedAt);
            totalSeconds += currentSeconds;
        }

        qint64 messages = 0;
        qint64 bloodstains = 0;
        qint64 ghosts = 0;
        qint64 summonsAdvertised = 0;
        qint64 summonClaims = 0;
        QSqlQuery stats(m_db->Conn());
        stats.prepare(QStringLiteral(
            "SELECT messages_created,bloodstains_created,ghosts_generated,summons_advertised,"
            "summon_claims FROM bloodborne_player_stats WHERE user_id=?"));
        stats.addBindValue(userId);
        if (stats.exec() && stats.next()) {
            messages = stats.value(0).toLongLong();
            bloodstains = stats.value(1).toLongLong();
            ghosts = stats.value(2).toLongLong();
            summonsAdvertised = stats.value(3).toLongLong();
            summonClaims = stats.value(4).toLongLong();
        }

        int activeMatchingRooms = 0;
        {
            QReadLocker lock(&m_shared->matching.roomsLock);
            for (auto room = m_shared->matching.rooms.cbegin();
                 room != m_shared->matching.rooms.cend(); ++room) {
                for (auto member = room->members.cbegin(); member != room->members.cend();
                     ++member) {
                    if (member->userId == userId) {
                        ++activeMatchingRooms;
                        break;
                    }
                }
            }
        }

        QJsonObject community;
        community.insert(QStringLiteral("messagesCreated"), messages);
        community.insert(QStringLiteral("bloodstainsCreated"), bloodstains);
        community.insert(QStringLiteral("ghostsGenerated"), ghosts);
        QJsonObject multiplayer;
        multiplayer.insert(QStringLiteral("activeMatchingRooms"), activeMatchingRooms);
        multiplayer.insert(QStringLiteral("summonsAdvertised"), summonsAdvertised);
        multiplayer.insert(QStringLiteral("summonClaims"), summonClaims);
        multiplayer.insert(QStringLiteral("historicalSuccessfulCoopSessions"), QJsonValue());

        QJsonObject player;
        player.insert(QStringLiteral("userId"), userId);
        player.insert(QStringLiteral("username"), username);
        player.insert(QStringLiteral("avatarUrl"), ValidAvatarFile(avatarFile)
                                                       ? QStringLiteral("/avatars/") + avatarFile
                                                       : QString());
        player.insert(QStringLiteral("online"), online);
        player.insert(QStringLiteral("registeredAt"), registeredAt);
        player.insert(QStringLiteral("lastSeen"),
                      online ? QDateTime::currentSecsSinceEpoch() : lastSeen);
        player.insert(QStringLiteral("totalSessions"), totalSessions);
        player.insert(QStringLiteral("totalOnlineSeconds"), totalSeconds);
        player.insert(QStringLiteral("currentSessionSeconds"), currentSeconds);
        player.insert(QStringLiteral("community"), community);
        player.insert(QStringLiteral("multiplayer"), multiplayer);
        return player;
    }

    void InsertActivity(qint64 userId, const QString& eventType, qint64 timestamp) {
        static const QSet<QString> allowed = {
            QStringLiteral("player_connected"), QStringLiteral("player_disconnected"),
            QStringLiteral("message_created"),  QStringLiteral("bloodstain_created"),
            QStringLiteral("ghost_created"),    QStringLiteral("summon_advertised"),
            QStringLiteral("summon_claimed"),
        };
        if (!allowed.contains(eventType))
            return;
        QSqlQuery query(m_db->Conn());
        query.prepare(QStringLiteral(
            "INSERT INTO bloodborne_activity(user_id,event_type,created_at) VALUES(?,?,?)"));
        query.addBindValue(userId);
        query.addBindValue(eventType);
        query.addBindValue(timestamp);
        if (!query.exec())
            qWarning() << "Bloodborne website activity write failed:" << query.lastError().text();
    }

    void CloseInterruptedPlayerSessions() {
        QSqlQuery query(m_db->Conn());
        if (!query.exec(QStringLiteral(
                "UPDATE bloodborne_player_session SET ended_at=started_at,duration_seconds=0,"
                "interrupted=1 WHERE ended_at IS NULL"))) {
            qWarning() << "Bloodborne website stale session cleanup failed:"
                       << query.lastError().text();
        }
    }

    void PurgeExpiredWebSessions() {
        QSqlQuery query(m_db->Conn());
        query.prepare(QStringLiteral("DELETE FROM bloodborne_web_session WHERE expires_at<=?"));
        query.addBindValue(QDateTime::currentSecsSinceEpoch());
        if (!query.exec())
            qWarning() << "Bloodborne website session cleanup failed:" << query.lastError().text();
    }

    void ClosePlayerSessionsAtShutdown() {
        if (!m_db)
            return;
        const qint64 now = QDateTime::currentSecsSinceEpoch();
        QSqlQuery query(m_db->Conn());
        query.prepare(
            QStringLiteral("UPDATE bloodborne_player_session SET ended_at=?,"
                           "duration_seconds=MAX(0,?-started_at) WHERE ended_at IS NULL"));
        query.addBindValue(now);
        query.addBindValue(now);
        if (!query.exec()) {
            qWarning() << "Bloodborne website session shutdown write failed:"
                       << query.lastError().text();
        }
    }

    BloodborneWebsiteServer* m_owner = nullptr;
    ConfigManager* m_config = nullptr;
    SharedState* m_shared = nullptr;
    std::unique_ptr<Database> m_db;
    std::unique_ptr<QHttpServer> m_http;
    std::unique_ptr<QTcpServer> m_tcp;
    QString m_dataRoot;
    QString m_avatarDirectory;
    QMutex m_rateMutex;
    QHash<QString, QList<qint64>> m_rateEvents;
};

BloodborneWebsiteServer::BloodborneWebsiteServer(QObject* parent)
    : QObject(parent), m_impl(std::make_unique<Impl>(this)) {}

BloodborneWebsiteServer::~BloodborneWebsiteServer() {
    Stop();
}

bool BloodborneWebsiteServer::Start(ConfigManager* config, const QString& dbPath,
                                    SharedState* shared) {
    return m_impl->Start(config, dbPath, shared);
}

void BloodborneWebsiteServer::Stop() {
    m_impl->Stop();
}

bool BloodborneWebsiteServer::IsListening() const {
    return m_impl->IsListening();
}

quint16 BloodborneWebsiteServer::ListeningPort() const {
    return m_impl->ListeningPort();
}

void BloodborneWebsiteServer::OnPlayerAuthenticated(qint64 userId, const QString& username,
                                                    bool publicVisible) {
    m_impl->RecordPlayerAuthenticated(userId, username, publicVisible);
}

void BloodborneWebsiteServer::OnPlayerDisconnected(qint64 userId, const QString& username,
                                                   bool publicVisible) {
    m_impl->RecordPlayerDisconnected(userId, username, publicVisible);
}
