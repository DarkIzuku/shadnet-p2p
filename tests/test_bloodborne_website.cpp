// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include <iostream>

#include <QBuffer>
#include <QColor>
#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTimer>

#include "bloodborne_website.h"
#include "client_session.h"
#include "config.h"
#include "database.h"

namespace {

bool Check(bool condition, const char *expression, int line) {
  if (!condition)
    std::cerr << "check failed at line " << line << ": " << expression << '\n';
  return condition;
}

#define CHECK(expression)                                                      \
  do {                                                                         \
    if (!Check((expression), #expression, __LINE__))                           \
      return 1;                                                                \
  } while (false)

struct HttpResult {
  bool finished = false;
  int status = 0;
  QByteArray body;
  QByteArray cookie;
  QList<QPair<QByteArray, QByteArray>> headers;
};

HttpResult Send(const QUrl &url, const QByteArray &method,
                const QByteArray &body = {}, const QByteArray &contentType = {},
                const QByteArray &cookie = {}, const QByteArray &csrf = {}) {
  QNetworkAccessManager manager;
  QNetworkRequest request(url);
  request.setRawHeader(QByteArrayLiteral("Accept"),
                       QByteArrayLiteral("application/json"));
  if (!contentType.isEmpty())
    request.setRawHeader(QByteArrayLiteral("Content-Type"), contentType);
  if (!cookie.isEmpty())
    request.setRawHeader(QByteArrayLiteral("Cookie"), cookie);
  if (!csrf.isEmpty())
    request.setRawHeader(QByteArrayLiteral("X-CSRF-Token"), csrf);
  QNetworkReply *reply = manager.sendCustomRequest(request, method, body);
  QEventLoop loop;
  QTimer timeout;
  timeout.setSingleShot(true);
  QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
  QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
  timeout.start(5000);
  loop.exec();

  HttpResult result;
  result.finished = reply->isFinished();
  if (result.finished) {
    result.status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    result.body = reply->readAll();
    result.headers = reply->rawHeaderPairs();
    const QByteArray setCookie =
        reply->rawHeader(QByteArrayLiteral("Set-Cookie"));
    result.cookie = setCookie.left(setCookie.indexOf(';'));
  } else {
    reply->abort();
  }
  delete reply;
  return result;
}

QJsonObject Root(const HttpResult &result) {
  return QJsonDocument::fromJson(result.body).object();
}

QJsonObject Data(const HttpResult &result) {
  return Root(result).value(QStringLiteral("data")).toObject();
}

QJsonObject Error(const HttpResult &result) {
  return Root(result).value(QStringLiteral("error")).toObject();
}

QUrl Url(const BloodborneWebsiteServer &server, const QString &path) {
  return QUrl(QStringLiteral("http://127.0.0.1:%1%2")
                  .arg(server.ListeningPort())
                  .arg(path));
}

HttpResult Json(const BloodborneWebsiteServer &server, const QString &path,
                const QJsonObject &body, const QByteArray &cookie = {},
                const QByteArray &csrf = {}) {
  return Send(Url(server, path), QByteArrayLiteral("POST"),
              QJsonDocument(body).toJson(QJsonDocument::Compact),
              QByteArrayLiteral("application/json"), cookie, csrf);
}

void WriteConfig(const QString &path, bool enabled, bool registrationEnabled) {
  QSettings settings(path, QSettings::IniFormat);
  settings.setValue(QStringLiteral("Host"), QStringLiteral("127.0.0.1"));
  settings.setValue(QStringLiteral("WebApiPort"), QStringLiteral("31315"));
  settings.setValue(QStringLiteral("BloodborneWebsiteEnabled"), enabled);
  settings.setValue(QStringLiteral("BloodborneWebsitePort"),
                    QStringLiteral("0"));
  settings.setValue(QStringLiteral("BloodborneWebsiteRegistrationEnabled"),
                    registrationEnabled);
  settings.sync();
}

QByteArray PngImage() {
  QImage image(32, 24, QImage::Format_ARGB32);
  image.fill(QColor(117, 31, 38));
  QByteArray bytes;
  QBuffer buffer(&bytes);
  buffer.open(QIODevice::WriteOnly);
  image.save(&buffer, "PNG");
  return bytes;
}

bool HasHeader(const HttpResult &result, const QByteArray &name,
               const QByteArray &value) {
  for (const auto &header : result.headers) {
    if (header.first.compare(name, Qt::CaseInsensitive) == 0 &&
        header.second.contains(value)) {
      return true;
    }
  }
  return false;
}

} // namespace

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);
  QTemporaryDir temporary;
  CHECK(temporary.isValid());
  const QString dbPath = temporary.filePath(QStringLiteral("db/shadnet.db"));

  SharedState shared;
  const QString disabledPath =
      temporary.filePath(QStringLiteral("disabled.cfg"));
  WriteConfig(disabledPath, false, true);
  ConfigManager disabledConfig;
  disabledConfig.Load(disabledPath);
  shared.config = &disabledConfig;
  BloodborneWebsiteServer disabled;
  CHECK(disabled.Start(&disabledConfig, dbPath, &shared));
  CHECK(!disabled.IsListening());
  CHECK(!QFileInfo::exists(
      temporary.filePath(QStringLiteral("data/bloodborne-website"))));

  const QString closedPath = temporary.filePath(QStringLiteral("closed.cfg"));
  WriteConfig(closedPath, true, false);
  ConfigManager closedConfig;
  closedConfig.Load(closedPath);
  shared.config = &closedConfig;
  BloodborneWebsiteServer closed;
  CHECK(closed.Start(&closedConfig, dbPath, &shared));
  CHECK(closed.IsListening());
  const HttpResult closedRegister = Json(
      closed, QStringLiteral("/api/register"),
      {{QStringLiteral("username"), QStringLiteral("ClosedHunter")},
       {QStringLiteral("password"), QStringLiteral("HunterPass123")},
       {QStringLiteral("confirmPassword"), QStringLiteral("HunterPass123")}});
  CHECK(closedRegister.status == 403);
  CHECK(Error(closedRegister).value(QStringLiteral("code")).toString() ==
        QStringLiteral("registration_closed"));
  closed.Stop();

  const QString configPath = temporary.filePath(QStringLiteral("website.cfg"));
  WriteConfig(configPath, true, true);
  ConfigManager config;
  config.Load(configPath);
  shared.config = &config;
  BloodborneWebsiteServer server;
  CHECK(server.Start(&config, dbPath, &shared));
  CHECK(server.IsListening());

  const HttpResult home =
      Send(Url(server, QStringLiteral("/")), QByteArrayLiteral("GET"));
  CHECK(home.finished);
  CHECK(home.status == 200);
  CHECK(home.body.contains("The Hunter's Requiem"));
  CHECK(home.body.contains("/assets/requiem-emblem.png"));
  CHECK(HasHeader(home, QByteArrayLiteral("X-Content-Type-Options"),
                  QByteArrayLiteral("nosniff")));
  CHECK(HasHeader(home, QByteArrayLiteral("X-Frame-Options"),
                  QByteArrayLiteral("DENY")));

  const HttpResult script = Send(Url(server, QStringLiteral("/assets/site.js")),
                                 QByteArrayLiteral("GET"));
  CHECK(script.status == 200);
  CHECK(script.body.contains(
      "An unofficial community server for wandering hunters."));
  CHECK(script.body.contains(
      "Un servidor comunitario no oficial para cazadores errantes."));
  CHECK(script.body.contains("textContent"));

  const HttpResult initialStatus = Send(
      Url(server, QStringLiteral("/api/status")), QByteArrayLiteral("GET"));
  CHECK(initialStatus.status == 200);
  CHECK(Data(initialStatus)
            .value(QStringLiteral("registrationEnabled"))
            .toBool());
  CHECK(Data(initialStatus)
            .value(QStringLiteral("registeredHunters"))
            .toInt(-1) == 0);

  const QJsonObject registration = {
      {QStringLiteral("username"), QStringLiteral("Izuku")},
      {QStringLiteral("password"), QStringLiteral("HunterPass123")},
      {QStringLiteral("confirmPassword"), QStringLiteral("HunterPass123")},
  };
  const HttpResult registered =
      Json(server, QStringLiteral("/api/register"), registration);
  CHECK(registered.status == 201);
  CHECK(Data(registered).value(QStringLiteral("username")).toString() ==
        QStringLiteral("Izuku"));
  const HttpResult duplicate =
      Json(server, QStringLiteral("/api/register"), registration);
  CHECK(duplicate.status == 409);
  CHECK(Error(duplicate).value(QStringLiteral("code")).toString() ==
        QStringLiteral("username_unavailable"));

  Database verification(QStringLiteral("website_test_verification"));
  CHECK(verification.Open(dbPath));
  const auto account =
      verification.CheckUser(QStringLiteral("Izuku"),
                             QStringLiteral("HunterPass123"), QString(), false);
  CHECK(account.has_value());
  CHECK(!account->salt.isEmpty());
  CHECK(!account->hash.isEmpty());
  CHECK(!account->token.isEmpty());

  const HttpResult badLogin =
      Json(server, QStringLiteral("/api/login"),
           {{QStringLiteral("username"), QStringLiteral("Izuku")},
            {QStringLiteral("password"), QStringLiteral("wrong-password")}});
  CHECK(badLogin.status == 401);
  const HttpResult login =
      Json(server, QStringLiteral("/api/login"),
           {{QStringLiteral("username"), QStringLiteral("Izuku")},
            {QStringLiteral("password"), QStringLiteral("HunterPass123")}});
  CHECK(login.status == 200);
  CHECK(!login.cookie.isEmpty());
  CHECK(HasHeader(login, QByteArrayLiteral("Set-Cookie"),
                  QByteArrayLiteral("HttpOnly")));
  CHECK(HasHeader(login, QByteArrayLiteral("Set-Cookie"),
                  QByteArrayLiteral("SameSite=Lax")));
  const QByteArray csrf =
      Data(login).value(QStringLiteral("csrfToken")).toString().toLatin1();
  CHECK(!csrf.isEmpty());

  const HttpResult anonymousAccount = Send(
      Url(server, QStringLiteral("/api/account")), QByteArrayLiteral("GET"));
  CHECK(anonymousAccount.status == 401);
  const HttpResult webAccount =
      Send(Url(server, QStringLiteral("/api/account")),
           QByteArrayLiteral("GET"), {}, {}, login.cookie);
  CHECK(webAccount.status == 200);
  CHECK(Data(webAccount).value(QStringLiteral("username")).toString() ==
        QStringLiteral("Izuku"));

  const HttpResult invalidAvatar =
      Send(Url(server, QStringLiteral("/api/account/avatar")),
           QByteArrayLiteral("POST"), QByteArrayLiteral("not-an-image"),
           QByteArrayLiteral("image/png"), login.cookie, csrf);
  CHECK(invalidAvatar.status == 415 || invalidAvatar.status == 400);
  const HttpResult invalidAvatarType =
      Send(Url(server, QStringLiteral("/api/account/avatar")),
           QByteArrayLiteral("POST"), QByteArrayLiteral("GIF89a"),
           QByteArrayLiteral("image/gif"), login.cookie, csrf);
  CHECK(invalidAvatarType.status == 415);
  const QByteArray oversized(2 * 1024 * 1024 + 1, 'x');
  const HttpResult oversizedAvatar =
      Send(Url(server, QStringLiteral("/api/account/avatar")),
           QByteArrayLiteral("POST"), oversized, QByteArrayLiteral("image/png"),
           login.cookie, csrf);
  CHECK(oversizedAvatar.status == 413);
  const HttpResult avatar =
      Send(Url(server, QStringLiteral("/api/account/avatar")),
           QByteArrayLiteral("POST"), PngImage(),
           QByteArrayLiteral("image/png"), login.cookie, csrf);
  CHECK(avatar.status == 200);
  const QString avatarUrl =
      Data(avatar).value(QStringLiteral("avatarUrl")).toString();
  CHECK(avatarUrl.startsWith(QStringLiteral("/avatars/")));
  const HttpResult avatarFile =
      Send(Url(server, avatarUrl), QByteArrayLiteral("GET"));
  CHECK(avatarFile.status == 200);
  CHECK(avatarFile.body.startsWith("\x89PNG"));
  CHECK(Send(Url(server, QStringLiteral("/avatars/not-a-uuid.png")),
             QByteArrayLiteral("GET"))
            .status == 404);

  {
    QWriteLocker lock(&shared.clientsLock);
    SharedState::ClientEntry entry;
    entry.npid = QStringLiteral("Izuku");
    entry.appearOffline = false;
    shared.clients.insert(account->userId, entry);
  }
  server.OnPlayerAuthenticated(account->userId, QStringLiteral("Izuku"), true);
  const HttpResult players =
      Send(Url(server, QStringLiteral("/api/players?search=Izu")),
           QByteArrayLiteral("GET"));
  CHECK(players.status == 200);
  const QJsonArray playerList =
      Data(players).value(QStringLiteral("players")).toArray();
  CHECK(playerList.size() == 1);
  CHECK(playerList.at(0).toObject().value(QStringLiteral("online")).toBool());
  CHECK(playerList.at(0)
            .toObject()
            .value(QStringLiteral("username"))
            .toString() == QStringLiteral("Izuku"));

  const HttpResult activity = Send(Url(server, QStringLiteral("/api/activity")),
                                   QByteArrayLiteral("GET"));
  CHECK(activity.status == 200);
  const QJsonArray events =
      Data(activity).value(QStringLiteral("activity")).toArray();
  CHECK(!events.isEmpty());
  CHECK(events.at(0).toObject().value(QStringLiteral("type")).toString() ==
        QStringLiteral("player_connected"));
  CHECK(!activity.body.contains("HunterPass123"));
  CHECK(!activity.body.contains("751229"));

  {
    QWriteLocker lock(&shared.clientsLock);
    shared.clients.remove(account->userId);
  }
  server.OnPlayerDisconnected(account->userId, QStringLiteral("Izuku"), true);
  const HttpResult offlinePlayers =
      Send(Url(server, QStringLiteral("/api/players?search=Izuku")),
           QByteArrayLiteral("GET"));
  const QJsonObject offlinePlayer = Data(offlinePlayers)
                                        .value(QStringLiteral("players"))
                                        .toArray()
                                        .at(0)
                                        .toObject();
  CHECK(!offlinePlayer.value(QStringLiteral("online")).toBool());
  CHECK(offlinePlayer.value(QStringLiteral("totalSessions")).toInt() == 1);

  // Web sessions are persisted as token hashes and remain valid across a normal
  // website listener restart.
  server.Stop();
  BloodborneWebsiteServer restarted;
  CHECK(restarted.Start(&config, dbPath, &shared));
  const HttpResult persisted =
      Send(Url(restarted, QStringLiteral("/api/account")),
           QByteArrayLiteral("GET"), {}, {}, login.cookie);
  CHECK(persisted.status == 200);

  const HttpResult csrfRejected = Send(
      Url(restarted, QStringLiteral("/api/logout")), QByteArrayLiteral("POST"),
      {}, {}, login.cookie, QByteArrayLiteral("incorrect"));
  CHECK(csrfRejected.status == 403);
  const HttpResult logout =
      Send(Url(restarted, QStringLiteral("/api/logout")),
           QByteArrayLiteral("POST"), {}, {}, login.cookie, csrf);
  CHECK(logout.status == 200);
  const HttpResult loggedOut =
      Send(Url(restarted, QStringLiteral("/api/account")),
           QByteArrayLiteral("GET"), {}, {}, login.cookie);
  CHECK(loggedOut.status == 401);

  QSqlQuery migrations(verification.Conn());
  CHECK(migrations.exec(
      QStringLiteral("SELECT COUNT(*) FROM migration WHERE migration_id=5")));
  CHECK(migrations.next());
  CHECK(migrations.value(0).toInt() == 1);
  return 0;
}
