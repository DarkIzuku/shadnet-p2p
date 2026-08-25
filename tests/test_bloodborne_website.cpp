// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include <iostream>

#include <QBuffer>
#include <QColor>
#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QHostAddress>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QSqlQuery>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>
#include <QUuid>

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
  // QNetworkAccessManager owns the reply. Its destructor releases it after the
  // copied result is complete; deleting a just-finished reply synchronously is
  // unsafe on some Qt event dispatchers.
  return result;
}

HttpResult SendRaw(quint16 port, const QByteArray &target) {
  QTcpSocket socket;
  QByteArray received;
  QEventLoop loop;
  QTimer timeout;
  timeout.setSingleShot(true);
  QObject::connect(&socket, &QTcpSocket::connected, &socket, [&] {
    socket.write(QByteArrayLiteral("GET ") + target +
                 QByteArrayLiteral(" HTTP/1.1\r\nHost: 127.0.0.1\r\n"
                                   "Connection: close\r\n\r\n"));
  });
  QObject::connect(&socket, &QTcpSocket::readyRead, &socket,
                   [&] { received += socket.readAll(); });
  QObject::connect(&socket, &QTcpSocket::disconnected, &loop,
                   &QEventLoop::quit);
  QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
  socket.connectToHost(QHostAddress::LocalHost, port);
  timeout.start(5000);
  loop.exec();
  received += socket.readAll();

  HttpResult result;
  result.finished = socket.state() == QAbstractSocket::UnconnectedState;
  const qsizetype firstSpace = received.indexOf(' ');
  if (firstSpace >= 0)
    result.status = received.mid(firstSpace + 1, 3).toInt();
  const qsizetype bodyOffset = received.indexOf(QByteArrayLiteral("\r\n\r\n"));
  if (bodyOffset >= 0)
    result.body = received.mid(bodyOffset + 4);
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

void WriteConfig(const QString &path, bool enabled, bool registrationEnabled,
                 bool externalAssetsEnabled = true,
                 const QString &externalAssetsPath =
                     QStringLiteral("website-assets-missing-for-tests")) {
  QSettings settings(path, QSettings::IniFormat);
  settings.setValue(QStringLiteral("Host"), QStringLiteral("127.0.0.1"));
  settings.setValue(QStringLiteral("WebApiPort"), QStringLiteral("31315"));
  settings.setValue(QStringLiteral("BloodborneWebsiteEnabled"), enabled);
  settings.setValue(QStringLiteral("BloodborneWebsitePort"),
                    QStringLiteral("0"));
  settings.setValue(QStringLiteral("BloodborneWebsiteRegistrationEnabled"),
                    registrationEnabled);
  settings.setValue(QStringLiteral("BloodborneWebsiteExternalAssetsEnabled"),
                    externalAssetsEnabled);
  settings.setValue(QStringLiteral("BloodborneWebsiteExternalAssetsPath"),
                    externalAssetsPath);
  settings.sync();
}

bool WriteFile(const QString &path, const QByteArray &contents) {
  if (!QDir().mkpath(QFileInfo(path).absolutePath()))
    return false;
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return false;
  return file.write(contents) == contents.size();
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
  restarted.Stop();

  QSqlQuery migrations(verification.Conn());
  CHECK(migrations.exec(
      QStringLiteral("SELECT COUNT(*) FROM migration WHERE migration_id=5")));
  CHECK(migrations.next());
  CHECK(migrations.value(0).toInt() == 1);

  const QString applicationDirectory = QCoreApplication::applicationDirPath();
  QTemporaryDir externalDirectory(
      QDir(applicationDirectory)
          .filePath(QStringLiteral("website-external-assets-test-XXXXXX")));
  CHECK(externalDirectory.isValid());
  const QString externalRoot = externalDirectory.path();
  const QString relativeExternalRoot =
      QDir(applicationDirectory).relativeFilePath(externalRoot);
  CHECK(!QDir::isAbsolutePath(relativeExternalRoot));

  const QByteArray externalIndex = QByteArrayLiteral(
      "<!doctype html><html><body>external-index-v1</body></html>");
  const QByteArray externalCss =
      QByteArrayLiteral("body{color:#ddd}/* external-css-v1 */");
  const QByteArray externalJs =
      QByteArrayLiteral("window.externalAssetVersion='v1';");
  const QByteArray externalImage =
      QByteArray::fromHex("ffd8ff0045585445524e414c00ffd9");
  CHECK(WriteFile(QDir(externalRoot).filePath(QStringLiteral("index.html")),
                  externalIndex));
  CHECK(
      WriteFile(QDir(externalRoot).filePath(QStringLiteral("assets/site.css")),
                externalCss));
  CHECK(WriteFile(QDir(externalRoot).filePath(QStringLiteral("assets/site.js")),
                  externalJs));
  CHECK(WriteFile(
      QDir(externalRoot)
          .filePath(QStringLiteral("assets/backgrounds/requiem-hero.jpg")),
      externalImage));
  CHECK(WriteFile(
      QDir(externalRoot).filePath(QStringLiteral("assets/requiem-emblem.png")),
      QByteArray::fromHex("89504e470d0a1a0a00010203")));

  const QList<QPair<QString, QByteArray>> mimeCases = {
      {QStringLiteral("sample.html"),
       QByteArrayLiteral("text/html; charset=utf-8")},
      {QStringLiteral("sample.css"),
       QByteArrayLiteral("text/css; charset=utf-8")},
      {QStringLiteral("sample.js"),
       QByteArrayLiteral("application/javascript; charset=utf-8")},
      {QStringLiteral("sample.json"),
       QByteArrayLiteral("application/json; charset=utf-8")},
      {QStringLiteral("sample.png"), QByteArrayLiteral("image/png")},
      {QStringLiteral("sample.jpg"), QByteArrayLiteral("image/jpeg")},
      {QStringLiteral("sample.jpeg"), QByteArrayLiteral("image/jpeg")},
      {QStringLiteral("sample.webp"), QByteArrayLiteral("image/webp")},
      {QStringLiteral("sample.svg"), QByteArrayLiteral("image/svg+xml")},
      {QStringLiteral("sample.ico"), QByteArrayLiteral("image/x-icon")},
      {QStringLiteral("sample.woff"), QByteArrayLiteral("font/woff")},
      {QStringLiteral("sample.woff2"), QByteArrayLiteral("font/woff2")},
      {QStringLiteral("sample.unknown"),
       QByteArrayLiteral("application/octet-stream")},
  };
  for (const auto &[name, unused] : mimeCases) {
    Q_UNUSED(unused);
    CHECK(WriteFile(
        QDir(externalRoot).filePath(QStringLiteral("assets/mime/") + name),
        QByteArrayLiteral("mime-body")));
  }

  const QString externalConfigPath =
      temporary.filePath(QStringLiteral("external-assets.cfg"));
  WriteConfig(externalConfigPath, true, true, true, relativeExternalRoot);
  ConfigManager externalConfig;
  CHECK(externalConfig.Load(externalConfigPath));
  shared.config = &externalConfig;
  BloodborneWebsiteServer externalServer;
  CHECK(externalServer.Start(&externalConfig, dbPath, &shared));

  const HttpResult externalHome =
      Send(Url(externalServer, QStringLiteral("/")), QByteArrayLiteral("GET"));
  CHECK(externalHome.status == 200);
  CHECK(externalHome.body == externalIndex);
  CHECK(HasHeader(externalHome, QByteArrayLiteral("Content-Type"),
                  QByteArrayLiteral("text/html; charset=utf-8")));
  CHECK(HasHeader(externalHome, QByteArrayLiteral("Cache-Control"),
                  QByteArrayLiteral("no-cache")));
  CHECK(HasHeader(externalHome, QByteArrayLiteral("X-Content-Type-Options"),
                  QByteArrayLiteral("nosniff")));
  CHECK(Send(Url(externalServer, QStringLiteral("/register")),
             QByteArrayLiteral("GET"))
            .body == externalIndex);

  const HttpResult externalStyle =
      Send(Url(externalServer, QStringLiteral("/assets/site.css")),
           QByteArrayLiteral("GET"));
  CHECK(externalStyle.status == 200);
  CHECK(externalStyle.body == externalCss);
  CHECK(HasHeader(externalStyle, QByteArrayLiteral("Content-Type"),
                  QByteArrayLiteral("text/css; charset=utf-8")));
  CHECK(Send(Url(externalServer, QStringLiteral("/assets/site.js")),
             QByteArrayLiteral("GET"))
            .body == externalJs);
  CHECK(Send(Url(externalServer,
                 QStringLiteral("/assets/backgrounds/requiem-hero.jpg")),
             QByteArrayLiteral("GET"))
            .body == externalImage);
  CHECK(Send(Url(externalServer, QStringLiteral("/assets/requiem-hero.jpg")),
             QByteArrayLiteral("GET"))
            .body == externalImage);

  for (const auto &[name, contentType] : mimeCases) {
    const HttpResult result =
        Send(Url(externalServer, QStringLiteral("/assets/mime/") + name),
             QByteArrayLiteral("GET"));
    CHECK(result.status == 200);
    CHECK(HasHeader(result, QByteArrayLiteral("Content-Type"), contentType));
  }

  CHECK(WriteFile(QDir(externalRoot).filePath(QStringLiteral("index.html")),
                  QByteArrayLiteral("external-index-v2")));
  CHECK(
      WriteFile(QDir(externalRoot).filePath(QStringLiteral("assets/site.css")),
                QByteArrayLiteral("external-css-v2")));
  CHECK(WriteFile(QDir(externalRoot).filePath(QStringLiteral("assets/site.js")),
                  QByteArrayLiteral("external-js-v2")));
  CHECK(Send(Url(externalServer, QStringLiteral("/")), QByteArrayLiteral("GET"))
            .body == QByteArrayLiteral("external-index-v2"));
  CHECK(Send(Url(externalServer, QStringLiteral("/assets/site.css")),
             QByteArrayLiteral("GET"))
            .body == QByteArrayLiteral("external-css-v2"));
  CHECK(Send(Url(externalServer, QStringLiteral("/assets/site.js")),
             QByteArrayLiteral("GET"))
            .body == QByteArrayLiteral("external-js-v2"));

  CHECK(Send(Url(externalServer, QStringLiteral("/assets/")),
             QByteArrayLiteral("GET"))
            .status == 404);
  CHECK(Send(Url(externalServer, QStringLiteral("/assets/missing.png")),
             QByteArrayLiteral("GET"))
            .status == 404);

  const QString outsideName =
      QStringLiteral("website-outside-%1.txt")
          .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
  const QString outsidePath = QDir(applicationDirectory).filePath(outsideName);
  const QByteArray outsideSecret = QByteArrayLiteral("must-not-be-served");
  CHECK(WriteFile(outsidePath, outsideSecret));
  const HttpResult plainTraversal =
      SendRaw(externalServer.ListeningPort(),
              QByteArrayLiteral("/../") + outsideName.toUtf8());
  CHECK(plainTraversal.status == 400 || plainTraversal.status == 404);
  CHECK(!plainTraversal.body.contains(outsideSecret));
  const HttpResult encodedTraversal =
      SendRaw(externalServer.ListeningPort(),
              QByteArrayLiteral("/%2e%2e/") + outsideName.toUtf8());
  CHECK(encodedTraversal.status == 400 || encodedTraversal.status == 404);
  CHECK(!encodedTraversal.body.contains(outsideSecret));
  CHECK(QFile::remove(outsidePath));
  externalServer.Stop();

  const QString embeddedOnlyConfigPath =
      temporary.filePath(QStringLiteral("embedded-only.cfg"));
  WriteConfig(embeddedOnlyConfigPath, true, true, false, relativeExternalRoot);
  ConfigManager embeddedOnlyConfig;
  CHECK(embeddedOnlyConfig.Load(embeddedOnlyConfigPath));
  shared.config = &embeddedOnlyConfig;
  BloodborneWebsiteServer embeddedOnlyServer;
  CHECK(embeddedOnlyServer.Start(&embeddedOnlyConfig, dbPath, &shared));
  const HttpResult embeddedOnlyHome = Send(
      Url(embeddedOnlyServer, QStringLiteral("/")), QByteArrayLiteral("GET"));
  CHECK(embeddedOnlyHome.status == 200);
  CHECK(embeddedOnlyHome.body.contains("The Hunter's Requiem"));
  CHECK(!embeddedOnlyHome.body.contains("external-index"));
  CHECK(HasHeader(embeddedOnlyHome, QByteArrayLiteral("Cache-Control"),
                  QByteArrayLiteral("no-store")));
  CHECK(Send(Url(embeddedOnlyServer, QStringLiteral("/assets/site.css")),
             QByteArrayLiteral("GET"))
            .body.contains("--bg:"));
  embeddedOnlyServer.Stop();
  return 0;
}
