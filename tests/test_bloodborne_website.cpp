// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include <iostream>

#include <QBuffer>
#include <QColor>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QHash>
#include <QHostAddress>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QSqlQuery>
#include <QStringList>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QThread>
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

HttpResult JsonMethod(const BloodborneWebsiteServer &server,
                      const QString &path, const QByteArray &method,
                      const QJsonObject &body, const QByteArray &cookie = {},
                      const QByteArray &csrf = {}) {
  return Send(Url(server, path), method,
              QJsonDocument(body).toJson(QJsonDocument::Compact),
              QByteArrayLiteral("application/json"), cookie, csrf);
}

QByteArray MultipartBody(const QByteArray &boundary,
                         const QList<QPair<QByteArray, QByteArray>> &fields,
                         const QByteArray &filename,
                         const QByteArray &fileContents) {
  QByteArray body;
  for (const auto &field : fields) {
    body += QByteArrayLiteral("--") + boundary + QByteArrayLiteral("\r\n");
    body += QByteArrayLiteral("Content-Disposition: form-data; name=\"") +
            field.first + QByteArrayLiteral("\"\r\n\r\n") + field.second +
            QByteArrayLiteral("\r\n");
  }
  body += QByteArrayLiteral("--") + boundary + QByteArrayLiteral("\r\n");
  body += QByteArrayLiteral(
              "Content-Disposition: form-data; name=\"file\"; filename=\"") +
          filename + QByteArrayLiteral("\"\r\n");
  // Deliberately misleading: the server must inspect neither nor trust this
  // value.
  body += QByteArrayLiteral("Content-Type: image/png\r\n\r\n") + fileContents +
          QByteArrayLiteral("\r\n--") + boundary + QByteArrayLiteral("--\r\n");
  return body;
}

HttpResult UploadDownload(
    const BloodborneWebsiteServer &server, const QByteArray &cookie,
    const QByteArray &csrf, const QByteArray &filename,
    const QByteArray &fileContents,
    const QList<QPair<QByteArray, QByteArray>> &fields = {
        {QByteArrayLiteral("displayName"), QByteArrayLiteral("Windows Server")},
        {QByteArrayLiteral("version"), QByteArrayLiteral("1.2.3")},
        {QByteArrayLiteral("category"), QByteArrayLiteral("Server")},
        {QByteArrayLiteral("description"),
         QByteArrayLiteral("Stable Windows package")},
        {QByteArrayLiteral("isActive"), QByteArrayLiteral("true")},
    }) {
  const QByteArray boundary =
      QByteArrayLiteral("----HunterRequiemBoundary7MA4YWxk");
  return Send(Url(server, QStringLiteral("/api/admin/downloads")),
              QByteArrayLiteral("POST"),
              MultipartBody(boundary, fields, filename, fileContents),
              QByteArrayLiteral("multipart/form-data; boundary=") + boundary,
              cookie, csrf);
}

void WriteConfig(const QString &path, bool enabled, bool registrationEnabled,
                 bool externalAssetsEnabled = true,
                 const QString &externalAssetsPath =
                     QStringLiteral("website-assets-missing-for-tests"),
                 bool chatEnabled = true, int chatMaxMessageLength = 400,
                 int chatHistoryLimit = 100, int chatResetHours = 24,
                 int downloadMaxFileSizeMiB = 8) {
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
  settings.setValue(QStringLiteral("BloodborneWebsiteChatEnabled"),
                    chatEnabled);
  settings.setValue(QStringLiteral("BloodborneWebsiteChatMaxMessageLength"),
                    chatMaxMessageLength);
  settings.setValue(QStringLiteral("BloodborneWebsiteChatHistoryLimit"),
                    chatHistoryLimit);
  settings.setValue(QStringLiteral("BloodborneWebsiteChatResetHours"),
                    chatResetHours);
  settings.setValue(QStringLiteral("BloodborneWebsiteDownloadMaxFileSizeMiB"),
                    downloadMaxFileSizeMiB);
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

  const QString chatDisabledPath =
      temporary.filePath(QStringLiteral("chat-disabled.cfg"));
  WriteConfig(chatDisabledPath, true, true, true,
              QStringLiteral("website-assets-missing-for-tests"), false);
  ConfigManager chatDisabledConfig;
  chatDisabledConfig.Load(chatDisabledPath);
  shared.config = &chatDisabledConfig;
  BloodborneWebsiteServer chatDisabled;
  CHECK(chatDisabled.Start(&chatDisabledConfig, dbPath, &shared));
  const HttpResult disabledChatRead =
      Send(Url(chatDisabled, QStringLiteral("/api/chat/messages")),
           QByteArrayLiteral("GET"));
  CHECK(disabledChatRead.status == 503);
  CHECK(Error(disabledChatRead).value(QStringLiteral("code")).toString() ==
        QStringLiteral("chat_disabled"));
  const HttpResult disabledChatWrite =
      Json(chatDisabled, QStringLiteral("/api/chat/messages"),
           {{QStringLiteral("message"), QStringLiteral("hidden")}});
  CHECK(disabledChatWrite.status == 503);
  {
    QSqlQuery disabledChatCount(
        QSqlDatabase::database(QStringLiteral("bloodborne_website_main")));
    CHECK(disabledChatCount.exec(
        QStringLiteral("SELECT COUNT(*) FROM bloodborne_web_chat_message")));
    CHECK(disabledChatCount.next());
    CHECK(disabledChatCount.value(0).toInt() == 0);
  }
  chatDisabled.Stop();

  const QString configPath = temporary.filePath(QStringLiteral("website.cfg"));
  WriteConfig(configPath, true, true, true,
              QStringLiteral("website-assets-missing-for-tests"), true, 400, 3,
              24);
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
  CHECK(home.body.contains("/assets/favicon.png"));
  CHECK(home.body.contains("/communion"));
  CHECK(home.body.contains("/downloads"));
  CHECK(home.body.contains("/admin/downloads"));
  CHECK(home.body.contains("skip-link"));
  for (const QString &pagePath :
       {QStringLiteral("/players"), QStringLiteral("/player/Izuku"),
        QStringLiteral("/register"), QStringLiteral("/login"),
        QStringLiteral("/account"), QStringLiteral("/communion")}) {
    CHECK(Send(Url(server, pagePath), QByteArrayLiteral("GET")).status == 200);
  }
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
  CHECK(script.body.contains("Hunter's Communion"));
  CHECK(script.body.contains("Comunión de Cazadores"));
  CHECK(script.body.contains("/api/chat/messages"));
  CHECK(script.body.contains("/api/chalices"));
  CHECK(script.body.contains("Chalice Dungeons"));
  CHECK(script.body.contains("Mazmorras de Cáliz"));
  CHECK(script.body.contains("Map data not yet decoded"));
  CHECK(script.body.contains("Los datos del mapa aún no están decodificados"));
  CHECK(script.body.contains(
      "Official files shared by this server's administrators."));
  CHECK(script.body.contains(
      "Archivos oficiales compartidos por los administradores"));
  CHECK(script.body.contains("/api/admin/downloads"));
  CHECK(script.body.contains("state.account?.isAdmin"));
  CHECK(script.body.contains("password-eye"));
  CHECK(script.body.contains("IntersectionObserver"));
  CHECK(script.body.contains("startingDownload"));
  CHECK(script.body.contains("aria-current"));

  const HttpResult style = Send(Url(server, QStringLiteral("/assets/site.css")),
                                QByteArrayLiteral("GET"));
  CHECK(style.status == 200);
  CHECK(style.body.contains(".chat-panel"));
  CHECK(style.body.contains(".chalice-table"));
  CHECK(style.body.contains(".dungeon-map-placeholder"));
  CHECK(style.body.contains(".downloads-grid"));
  CHECK(style.body.contains(".admin-download-form"));
  CHECK(style.body.contains(".password-eye"));
  CHECK(style.body.contains(".motion-item"));
  CHECK(style.body.contains("prefers-reduced-motion"));
  CHECK(style.body.contains(":focus-visible"));
  CHECK(style.body.contains(".button.is-copied"));
  CHECK(Send(Url(server, QStringLiteral("/assets/favicon.png")),
             QByteArrayLiteral("GET"))
            .status == 200);
  CHECK(
      Send(Url(server, QStringLiteral("/downloads")), QByteArrayLiteral("GET"))
          .status == 200);
  CHECK(Send(Url(server, QStringLiteral("/admin/downloads")),
             QByteArrayLiteral("GET"))
            .status == 401);

  const HttpResult initialChat =
      Send(Url(server, QStringLiteral("/api/chat/messages")),
           QByteArrayLiteral("GET"));
  CHECK(initialChat.status == 200);
  CHECK(
      Data(initialChat).value(QStringLiteral("messages")).toArray().isEmpty());
  CHECK(Data(initialChat).value(QStringLiteral("historyLimit")).toInt() == 3);
  CHECK(Data(initialChat).value(QStringLiteral("maxMessageLength")).toInt() ==
        400);

  const HttpResult initialStatus = Send(
      Url(server, QStringLiteral("/api/status")), QByteArrayLiteral("GET"));
  CHECK(initialStatus.status == 200);
  CHECK(Data(initialStatus)
            .value(QStringLiteral("registrationEnabled"))
            .toBool());
  CHECK(Data(initialStatus)
            .value(QStringLiteral("registeredHunters"))
            .toInt(-1) == 0);
  CHECK(Data(initialStatus).value(QStringLiteral("chatEnabled")).toBool());

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
  CHECK(verification.SetAdmin(account->userId, true));
  {
    QSqlQuery migration(verification.Conn());
    CHECK(migration.exec(QStringLiteral(
        "SELECT COUNT(*) FROM migration WHERE migration_id=9 AND "
        "description='The Hunter Requiem downloads catalog'")));
    CHECK(migration.next());
    CHECK(migration.value(0).toInt() == 1);
    CHECK(migration.exec(QStringLiteral(
        "SELECT COUNT(*) FROM pragma_table_info('bloodborne_web_download') "
        "WHERE name IN "
        "('id','display_name','stored_filename','original_filename',"
        "'version','description','category','file_size','sha256','created_at',"
        "'updated_at','is_active','download_count')")));
    CHECK(migration.next());
    CHECK(migration.value(0).toInt() == 13);
  }

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
  CHECK(Data(webAccount).value(QStringLiteral("isAdmin")).toBool());

  const QJsonObject normalRegistration = {
      {QStringLiteral("username"), QStringLiteral("NormalHunter")},
      {QStringLiteral("password"), QStringLiteral("NormalPass123")},
      {QStringLiteral("confirmPassword"), QStringLiteral("NormalPass123")},
  };
  CHECK(Json(server, QStringLiteral("/api/register"), normalRegistration)
            .status == 201);
  const HttpResult normalLogin =
      Json(server, QStringLiteral("/api/login"),
           {{QStringLiteral("username"), QStringLiteral("NormalHunter")},
            {QStringLiteral("password"), QStringLiteral("NormalPass123")}});
  CHECK(normalLogin.status == 200);
  const QByteArray normalCsrf = Data(normalLogin)
                                    .value(QStringLiteral("csrfToken"))
                                    .toString()
                                    .toLatin1();
  const HttpResult normalAccount =
      Send(Url(server, QStringLiteral("/api/account")),
           QByteArrayLiteral("GET"), {}, {}, normalLogin.cookie);
  CHECK(normalAccount.status == 200);
  CHECK(!Data(normalAccount).value(QStringLiteral("isAdmin")).toBool());
  CHECK(Send(Url(server, QStringLiteral("/admin/downloads")),
             QByteArrayLiteral("GET"), {}, {}, normalLogin.cookie)
            .status == 403);
  CHECK(Send(Url(server, QStringLiteral("/admin/downloads")),
             QByteArrayLiteral("GET"), {}, {}, login.cookie)
            .status == 200);

  const QByteArray packageBytes =
      QByteArrayLiteral("MZ\x00shadNet Windows package\nnot-a-browser-image");
  CHECK(UploadDownload(server, {}, {}, QByteArrayLiteral("visitor.zip"),
                       packageBytes)
            .status == 401);
  CHECK(UploadDownload(server, normalLogin.cookie, normalCsrf,
                       QByteArrayLiteral("normal.zip"), packageBytes)
            .status == 403);
  CHECK(UploadDownload(server, login.cookie, QByteArrayLiteral("wrong-csrf"),
                       QByteArrayLiteral("wrong.zip"), packageBytes)
            .status == 403);
  const HttpResult traversal =
      UploadDownload(server, login.cookie, csrf,
                     QByteArrayLiteral("../escape.zip"), packageBytes);
  CHECK(traversal.status == 400);
  CHECK(Error(traversal).value(QStringLiteral("code")).toString() ==
        QStringLiteral("unsafe_filename"));
  CHECK(QDir(temporary.filePath(QStringLiteral("data/downloads")))
            .entryList(QDir::Files | QDir::NoDotAndDotDot)
            .isEmpty());

  const HttpResult uploaded =
      UploadDownload(server, login.cookie, csrf,
                     QByteArrayLiteral("server<script>.zip"), packageBytes);
  CHECK(uploaded.status == 201);
  const QJsonObject uploadedDownload = Data(uploaded);
  const qint64 downloadId =
      uploadedDownload.value(QStringLiteral("id")).toVariant().toLongLong();
  CHECK(downloadId > 0);
  CHECK(uploadedDownload.value(QStringLiteral("originalFilename")).toString() ==
        QStringLiteral("server_script_.zip"));
  const QString packageSha = QString::fromLatin1(
      QCryptographicHash::hash(packageBytes, QCryptographicHash::Sha256)
          .toHex());
  CHECK(uploadedDownload.value(QStringLiteral("sha256")).toString() ==
        packageSha);
  CHECK(uploadedDownload.value(QStringLiteral("fileSize"))
            .toVariant()
            .toLongLong() == packageBytes.size());
  CHECK(!uploaded.body.contains("stored_filename"));
  CHECK(!uploaded.body.contains("data/downloads"));
  CHECK(!uploaded.body.contains(dbPath.toUtf8()));

  QString firstStoredFilename;
  {
    QSqlQuery stored(verification.Conn());
    stored.prepare(QStringLiteral("SELECT stored_filename,original_filename "
                                  "FROM bloodborne_web_download WHERE id=?"));
    stored.addBindValue(downloadId);
    CHECK(stored.exec());
    CHECK(stored.next());
    firstStoredFilename = stored.value(0).toString();
    CHECK(
        QRegularExpression(QStringLiteral("^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{"
                                          "3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$"))
            .match(firstStoredFilename)
            .hasMatch());
    CHECK(QFileInfo::exists(temporary.filePath(
        QStringLiteral("data/downloads/") + firstStoredFilename)));
  }

  const HttpResult publicDownloads = Send(
      Url(server, QStringLiteral("/api/downloads")), QByteArrayLiteral("GET"));
  CHECK(publicDownloads.status == 200);
  CHECK(Data(publicDownloads).value(QStringLiteral("total")).toInt() == 1);
  CHECK(Data(publicDownloads)
            .value(QStringLiteral("downloads"))
            .toArray()
            .first()
            .toObject()
            .value(QStringLiteral("id"))
            .toVariant()
            .toLongLong() == downloadId);
  CHECK(!publicDownloads.body.contains("stored_filename"));
  CHECK(!publicDownloads.body.contains(firstStoredFilename.toUtf8()));
  const QString downloadApiPath =
      QStringLiteral("/api/downloads/%1").arg(downloadId);
  CHECK(Send(Url(server, downloadApiPath), QByteArrayLiteral("GET")).status ==
        200);
  CHECK(Send(Url(server, QStringLiteral("/api/admin/downloads")),
             QByteArrayLiteral("GET"), {}, {}, normalLogin.cookie)
            .status == 403);
  CHECK(Send(Url(server, QStringLiteral("/api/admin/downloads")),
             QByteArrayLiteral("GET"), {}, {}, login.cookie)
            .status == 200);

  const QString filePath = QStringLiteral("/downloads/file/%1").arg(downloadId);
  const HttpResult downloaded =
      Send(Url(server, filePath), QByteArrayLiteral("GET"));
  CHECK(downloaded.status == 200);
  CHECK(downloaded.body == packageBytes);
  CHECK(HasHeader(downloaded, QByteArrayLiteral("Content-Type"),
                  QByteArrayLiteral("application/octet-stream")));
  CHECK(HasHeader(downloaded, QByteArrayLiteral("Content-Disposition"),
                  QByteArrayLiteral("attachment;")));
  CHECK(HasHeader(downloaded, QByteArrayLiteral("Content-Disposition"),
                  QByteArrayLiteral("server_script_.zip")));
  const HttpResult afterDownload =
      Send(Url(server, downloadApiPath), QByteArrayLiteral("GET"));
  CHECK(Data(afterDownload).value(QStringLiteral("downloadCount")).toInt() ==
        1);

  const HttpResult disabledDownload = JsonMethod(
      server, QStringLiteral("/api/admin/downloads/%1").arg(downloadId),
      QByteArrayLiteral("PUT"), {{QStringLiteral("isActive"), false}},
      login.cookie, csrf);
  CHECK(disabledDownload.status == 200);
  CHECK(!Data(disabledDownload).value(QStringLiteral("isActive")).toBool());
  CHECK(Data(Send(Url(server, QStringLiteral("/api/downloads")),
                  QByteArrayLiteral("GET")))
            .value(QStringLiteral("total"))
            .toInt() == 0);
  CHECK(Send(Url(server, downloadApiPath), QByteArrayLiteral("GET")).status ==
        404);
  CHECK(Send(Url(server, filePath), QByteArrayLiteral("GET")).status == 404);

  const HttpResult enabledDownload = JsonMethod(
      server, QStringLiteral("/api/admin/downloads/%1").arg(downloadId),
      QByteArrayLiteral("PUT"),
      {{QStringLiteral("displayName"),
        QStringLiteral("Windows Server Updated")},
       {QStringLiteral("version"), QStringLiteral("2.0.0")},
       {QStringLiteral("category"), QStringLiteral("Server")},
       {QStringLiteral("description"), QStringLiteral("Updated package")},
       {QStringLiteral("isActive"), true}},
      login.cookie, csrf);
  CHECK(enabledDownload.status == 200);
  CHECK(Data(enabledDownload).value(QStringLiteral("displayName")).toString() ==
        QStringLiteral("Windows Server Updated"));
  CHECK(Data(enabledDownload).value(QStringLiteral("isActive")).toBool());

  const QByteArray replacementBytes =
      QByteArrayLiteral("PK\x03\x04replacement shadNet package");
  const QByteArray replaceBoundary =
      QByteArrayLiteral("----HunterRequiemReplacementBoundary");
  const HttpResult replaced = Send(
      Url(server,
          QStringLiteral("/api/admin/downloads/%1/replace").arg(downloadId)),
      QByteArrayLiteral("POST"),
      MultipartBody(replaceBoundary, {}, QByteArrayLiteral("shadnet-win64.zip"),
                    replacementBytes),
      QByteArrayLiteral("multipart/form-data; boundary=") + replaceBoundary,
      login.cookie, csrf);
  CHECK(replaced.status == 200);
  CHECK(Data(replaced).value(QStringLiteral("originalFilename")).toString() ==
        QStringLiteral("shadnet-win64.zip"));
  CHECK(Data(replaced).value(QStringLiteral("sha256")).toString() ==
        QString::fromLatin1(QCryptographicHash::hash(replacementBytes,
                                                     QCryptographicHash::Sha256)
                                .toHex()));
  CHECK(!QFileInfo::exists(temporary.filePath(
      QStringLiteral("data/downloads/") + firstStoredFilename)));
  CHECK(Send(Url(server, filePath), QByteArrayLiteral("GET")).body ==
        replacementBytes);

  QString replacementStoredFilename;
  {
    QSqlQuery stored(verification.Conn());
    stored.prepare(QStringLiteral(
        "SELECT stored_filename FROM bloodborne_web_download WHERE id=?"));
    stored.addBindValue(downloadId);
    CHECK(stored.exec());
    CHECK(stored.next());
    replacementStoredFilename = stored.value(0).toString();
    CHECK(replacementStoredFilename != firstStoredFilename);
    CHECK(QFileInfo::exists(temporary.filePath(
        QStringLiteral("data/downloads/") + replacementStoredFilename)));
  }

  server.Stop();
  CHECK(server.Start(&config, dbPath, &shared));
  CHECK(server.IsListening());
  CHECK(Send(Url(server, filePath), QByteArrayLiteral("GET")).body ==
        replacementBytes);
  CHECK(QFileInfo::exists(temporary.filePath(QStringLiteral("data/downloads/") +
                                             replacementStoredFilename)));

  const HttpResult deleted = Send(
      Url(server, QStringLiteral("/api/admin/downloads/%1").arg(downloadId)),
      QByteArrayLiteral("DELETE"), {}, {}, login.cookie, csrf);
  CHECK(deleted.status == 200);
  CHECK(Send(Url(server, downloadApiPath), QByteArrayLiteral("GET")).status ==
        404);
  CHECK(!QFileInfo::exists(temporary.filePath(
      QStringLiteral("data/downloads/") + replacementStoredFilename)));
  CHECK(QDir(temporary.filePath(QStringLiteral("data/downloads")))
            .entryList(QDir::Files | QDir::NoDotAndDotDot)
            .isEmpty());
  {
    QSqlQuery count(verification.Conn());
    CHECK(count.exec(
        QStringLiteral("SELECT COUNT(*) FROM bloodborne_web_download")));
    CHECK(count.next());
    CHECK(count.value(0).toInt() == 0);
  }

  const auto seedChalice =
      [&](const QString &glyph, qint64 creator, int shareLevel, int ritualLevel,
          int holyGrailTypeId, int subFeatureFlag) -> bool {
    QSqlQuery insert(verification.Conn());
    insert.prepare(QStringLiteral(
        "INSERT INTO bloodborne_chalice(discernment_word,create_user_id,"
        "create_chara_id,create_date,last_play_date,fixed_or_general,form_data,"
        "form_data_version,holy_grail_type_id,ritual_level,share_level,status,"
        "sub_feature_flag,turnout_level,unlock_flag_list,wish_material_list) "
        "VALUES(?,?,'9223372036854776000','2026-08-28T08:30:31',"
        "'2026-08-28T08:30:31',1,'SENSITIVE_FORM_DATA',0,?,?,?,1,?,0,"
        "'[{\"UnlockFlag\":0}]','[]')"));
    insert.addBindValue(glyph);
    insert.addBindValue(creator);
    insert.addBindValue(holyGrailTypeId);
    insert.addBindValue(ritualLevel);
    insert.addBindValue(shareLevel);
    insert.addBindValue(subFeatureFlag);
    return insert.exec();
  };
  CHECK(seedChalice(QStringLiteral("n2vskrmr"), account->userId, 2, 5, 11, 19));
  CHECK(seedChalice(QStringLiteral("8abcde23"), -1, 2, 3, 1, 0));
  CHECK(seedChalice(QStringLiteral("9hidden2"), account->userId, 0, 1, 0, 0));

  const HttpResult chalicePage =
      Send(Url(server, QStringLiteral("/chalice")), QByteArrayLiteral("GET"));
  CHECK(chalicePage.status == 200);
  CHECK(chalicePage.body.contains("/chalice"));
  CHECK(Send(Url(server, QStringLiteral("/chalice/n2vskrmr")),
             QByteArrayLiteral("GET"))
            .status == 200);

  const HttpResult chalices = Send(Url(server, QStringLiteral("/api/chalices")),
                                   QByteArrayLiteral("GET"));
  CHECK(chalices.status == 200);
  CHECK(Data(chalices).value(QStringLiteral("total")).toInt() == 2);
  CHECK(Data(chalices).value(QStringLiteral("storedTotal")).toInt() == 3);
  const QJsonArray chaliceList =
      Data(chalices).value(QStringLiteral("chalices")).toArray();
  CHECK(chaliceList.size() == 2);
  QJsonObject localChalice;
  QJsonObject importedChalice;
  for (const QJsonValue &value : chaliceList) {
    const QJsonObject item = value.toObject();
    if (item.value(QStringLiteral("glyph")).toString() ==
        QStringLiteral("n2vskrmr"))
      localChalice = item;
    if (item.value(QStringLiteral("glyph")).toString() ==
        QStringLiteral("8abcde23"))
      importedChalice = item;
  }
  CHECK(!localChalice.isEmpty());
  CHECK(localChalice.value(QStringLiteral("creator"))
            .toObject()
            .value(QStringLiteral("kind"))
            .toString() == QStringLiteral("local"));
  CHECK(localChalice.value(QStringLiteral("creator"))
            .toObject()
            .value(QStringLiteral("username"))
            .toString() == QStringLiteral("Izuku"));
  CHECK(localChalice.value(QStringLiteral("creator"))
            .toObject()
            .value(QStringLiteral("avatarUrl"))
            .toString()
            .isEmpty());
  CHECK(localChalice.value(QStringLiteral("creator"))
            .toObject()
            .value(QStringLiteral("profileUrl"))
            .toString() == QStringLiteral("/player/Izuku"));
  CHECK(!importedChalice.isEmpty());
  CHECK(importedChalice.value(QStringLiteral("creator"))
            .toObject()
            .value(QStringLiteral("kind"))
            .toString() == QStringLiteral("imported"));
  CHECK(!importedChalice.value(QStringLiteral("creator"))
             .toObject()
             .contains(QStringLiteral("username")));

  const HttpResult glyphFilter =
      Send(Url(server, QStringLiteral("/api/chalices?glyph=n2vskrmr")),
           QByteArrayLiteral("GET"));
  CHECK(glyphFilter.status == 200);
  CHECK(Data(glyphFilter).value(QStringLiteral("total")).toInt() == 1);
  const HttpResult filteredChalices = Send(
      Url(server, QStringLiteral("/api/chalices?depth=5&type=11&rites=19")),
      QByteArrayLiteral("GET"));
  CHECK(Data(filteredChalices).value(QStringLiteral("total")).toInt() == 1);

  const HttpResult chaliceDetail =
      Send(Url(server, QStringLiteral("/api/chalices/n2vskrmr")),
           QByteArrayLiteral("GET"));
  CHECK(chaliceDetail.status == 200);
  CHECK(Data(chaliceDetail).value(QStringLiteral("glyph")).toString() ==
        QStringLiteral("n2vskrmr"));
  CHECK(Data(chaliceDetail).value(QStringLiteral("ritualLevel")).toInt() == 5);
  CHECK(Data(chaliceDetail).value(QStringLiteral("formDataBytes")).toInt() ==
        QByteArrayLiteral("SENSITIVE_FORM_DATA").size());
  CHECK(!chaliceDetail.body.contains("SENSITIVE_FORM_DATA"));
  CHECK(!chaliceDetail.body.contains("SessionId"));
  CHECK(!chaliceDetail.body.contains("Authorization"));
  CHECK(!chaliceDetail.body.contains("izuku@example.test"));
  CHECK(!Data(chaliceDetail).contains(QStringLiteral("createCharaId")));
  CHECK(Data(Send(Url(server, QStringLiteral("/api/chalices?glyph=3n7q")),
                  QByteArrayLiteral("GET")))
            .value(QStringLiteral("total"))
            .toInt(-1) == 0);
  CHECK(Send(Url(server, QStringLiteral("/api/chalices/3n7q")),
             QByteArrayLiteral("GET"))
            .status == 404);
  CHECK(Send(Url(server, QStringLiteral("/api/chalices/9hidden2")),
             QByteArrayLiteral("GET"))
            .status == 404);
  CHECK(Data(Send(Url(server, QStringLiteral("/api/chalices?glyph=9hidden2")),
                  QByteArrayLiteral("GET")))
            .value(QStringLiteral("total"))
            .toInt(-1) == 0);
  const HttpResult map =
      Send(Url(server, QStringLiteral("/api/chalices/n2vskrmr/map")),
           QByteArrayLiteral("GET"));
  CHECK(map.status == 200);
  CHECK(Data(map).value(QStringLiteral("status")).toString() ==
        QStringLiteral("not_decoded"));
  CHECK(Data(map).value(QStringLiteral("layout")).isNull());

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
  const QJsonObject chaliceWithAvatar =
      Data(Send(Url(server, QStringLiteral("/api/chalices/n2vskrmr")),
                QByteArrayLiteral("GET")));
  CHECK(chaliceWithAvatar.value(QStringLiteral("creator"))
            .toObject()
            .value(QStringLiteral("avatarUrl"))
            .toString() == avatarUrl);

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

  const HttpResult anonymousChatWrite =
      Json(server, QStringLiteral("/api/chat/messages"),
           {{QStringLiteral("message"), QStringLiteral("anonymous")}});
  CHECK(anonymousChatWrite.status == 401);
  const HttpResult chatCsrfRejected =
      Json(server, QStringLiteral("/api/chat/messages"),
           {{QStringLiteral("message"), QStringLiteral("forged csrf")}},
           login.cookie, QByteArrayLiteral("incorrect"));
  CHECK(chatCsrfRejected.status == 403);
  const HttpResult invalidChatJson =
      Send(Url(server, QStringLiteral("/api/chat/messages")),
           QByteArrayLiteral("POST"), QByteArrayLiteral("not-json"),
           QByteArrayLiteral("application/json"), login.cookie, csrf);
  CHECK(invalidChatJson.status == 400);
  const HttpResult emptyChat =
      Json(server, QStringLiteral("/api/chat/messages"),
           {{QStringLiteral("message"), QStringLiteral("  \n\t  ")}},
           login.cookie, csrf);
  CHECK(emptyChat.status == 400);
  const HttpResult longChat =
      Json(server, QStringLiteral("/api/chat/messages"),
           {{QStringLiteral("message"), QString(401, QLatin1Char('x'))}},
           login.cookie, csrf);
  CHECK(longChat.status == 400);

  const QString unicodeMessage =
      QStringLiteral("¡Buenas noches, cazadores! Ñandú 😀");
  const HttpResult chatCreated =
      Json(server, QStringLiteral("/api/chat/messages"),
           {{QStringLiteral("message"), unicodeMessage},
            {QStringLiteral("account_id"), 9999},
            {QStringLiteral("username"), QStringLiteral("Mika")},
            {QStringLiteral("avatar"), QStringLiteral("/avatars/forged.png")}},
           login.cookie, csrf);
  CHECK(chatCreated.status == 201);
  const QJsonObject firstChat = Data(chatCreated);
  CHECK(firstChat.value(QStringLiteral("username")).toString() ==
        QStringLiteral("Izuku"));
  CHECK(firstChat.value(QStringLiteral("avatarUrl")).toString() == avatarUrl);
  CHECK(firstChat.value(QStringLiteral("message")).toString() ==
        unicodeMessage);
  CHECK(firstChat.value(QStringLiteral("online")).toBool());
  CHECK(!firstChat.contains(QStringLiteral("account_id")));
  CHECK(
      QDateTime::fromString(
          firstChat.value(QStringLiteral("createdAt")).toString(), Qt::ISODate)
          .isValid());

  const HttpResult chatRateLimited =
      Json(server, QStringLiteral("/api/chat/messages"),
           {{QStringLiteral("message"), QStringLiteral("too fast")}},
           login.cookie, csrf);
  CHECK(chatRateLimited.status == 429);
  QThread::msleep(1050);
  const QString xssMessage =
      QStringLiteral("<script>alert('old blood')</script><b>literal</b>");
  const HttpResult xssChat =
      Json(server, QStringLiteral("/api/chat/messages"),
           {{QStringLiteral("message"), xssMessage}}, login.cookie, csrf);
  CHECK(xssChat.status == 201);
  CHECK(Data(xssChat).value(QStringLiteral("message")).toString() ==
        xssMessage);

  QSqlQuery injectChat(verification.Conn());
  injectChat.prepare(QStringLiteral(
      "INSERT INTO bloodborne_web_chat_message(account_id,message,created_at) "
      "VALUES(?,?,?)"));
  for (int index = 0; index < 4; ++index) {
    injectChat.bindValue(0, static_cast<qlonglong>(account->userId));
    injectChat.bindValue(1, QStringLiteral("history-%1").arg(index));
    injectChat.bindValue(2, QDateTime::currentSecsSinceEpoch() + index);
    CHECK(injectChat.exec());
  }

  const HttpResult recentChat =
      Send(Url(server, QStringLiteral("/api/chat/messages")),
           QByteArrayLiteral("GET"));
  CHECK(recentChat.status == 200);
  const QJsonArray recentMessages =
      Data(recentChat).value(QStringLiteral("messages")).toArray();
  CHECK(recentMessages.size() == 3);
  qint64 previousId = 0;
  QSet<qint64> recentIds;
  for (const QJsonValue &value : recentMessages) {
    const qint64 id = value.toObject().value(QStringLiteral("id")).toInteger();
    CHECK(id > previousId);
    CHECK(!recentIds.contains(id));
    recentIds.insert(id);
    previousId = id;
  }
  const qint64 cursor =
      recentMessages.at(0).toObject().value(QStringLiteral("id")).toInteger();
  const HttpResult incremental = Send(
      Url(server, QStringLiteral("/api/chat/messages?after=%1").arg(cursor)),
      QByteArrayLiteral("GET"));
  const QJsonArray incrementalMessages =
      Data(incremental).value(QStringLiteral("messages")).toArray();
  CHECK(incrementalMessages.size() == 2);
  for (const QJsonValue &value : incrementalMessages)
    CHECK(value.toObject().value(QStringLiteral("id")).toInteger() > cursor);
  const HttpResult afterZero =
      Send(Url(server, QStringLiteral("/api/chat/messages?after=0")),
           QByteArrayLiteral("GET"));
  CHECK(Data(afterZero).value(QStringLiteral("messages")).toArray().size() ==
        3);
  const HttpResult afterUnknown =
      Send(Url(server, QStringLiteral("/api/chat/messages?after=999999999")),
           QByteArrayLiteral("GET"));
  CHECK(
      Data(afterUnknown).value(QStringLiteral("messages")).toArray().isEmpty());
  CHECK(Send(Url(server, QStringLiteral("/api/chat/messages?after=invalid")),
             QByteArrayLiteral("GET"))
            .status == 400);

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

  // Seed unrelated persistent data so chat reset scope is verified against the
  // real community and Bloodborne tables.
  QSqlQuery preserved(verification.Conn());
  CHECK(preserved.exec(
      QStringLiteral(
          "INSERT OR IGNORE INTO bloodborne_player_stats(user_id) VALUES(%1)")
          .arg(account->userId)));
  CHECK(preserved.exec(
      QStringLiteral(
          "INSERT INTO "
          "bloodborne_blood_message(owner_user_id,owner_chara_id,area_id,"
          "area_region_id,channel_id,blood_data,blood_data_version,chara_data,"
          "chara_data_version,prev_blood_mess_id,created_at) "
          "VALUES(%1,1,10,20,30,'AA==',1,0,1,0,1)")
          .arg(account->userId)));
  CHECK(preserved.exec(
      QStringLiteral(
          "INSERT INTO "
          "bloodborne_tomb_message(owner_user_id,owner_chara_id,area_id,"
          "area_region_id,channel_id,tomb_data,tomb_data_version,death_vision_"
          "data,"
          "death_vision_data_version,created_at) "
          "VALUES(%1,1,10,20,30,'AA==',1,'AA==',1,1)")
          .arg(account->userId)));
  CHECK(preserved.exec(
      QStringLiteral(
          "INSERT INTO "
          "bloodborne_wandering_ghost(owner_user_id,owner_chara_id,area_id,"
          "area_region_id,channel_id,matching_level,reject_ignore,wandering_"
          "ghost_data,"
          "wandering_ghost_data_version,created_at,expires_at) "
          "VALUES(%1,1,10,20,30,100,0,'AA==',1,1,9999999999)")
          .arg(account->userId)));
  const auto tableCount = [&](const QString &table) -> qint64 {
    QSqlQuery count(verification.Conn());
    if (!count.exec(QStringLiteral("SELECT COUNT(*) FROM ") + table) ||
        !count.next())
      return -1;
    return count.value(0).toLongLong();
  };
  const QStringList preservedTables = {
      QStringLiteral("account"),
      QStringLiteral("bloodborne_web_profile"),
      QStringLiteral("bloodborne_web_session"),
      QStringLiteral("bloodborne_player_session"),
      QStringLiteral("bloodborne_player_stats"),
      QStringLiteral("bloodborne_activity"),
      QStringLiteral("bloodborne_blood_message"),
      QStringLiteral("bloodborne_tomb_message"),
      QStringLiteral("bloodborne_wandering_ghost"),
      QStringLiteral("bloodborne_chalice"),
  };
  QHash<QString, qint64> countsBeforeReset;
  for (const QString &table : preservedTables) {
    countsBeforeReset.insert(table, tableCount(table));
    CHECK(countsBeforeReset.value(table) > 0);
  }

  // A restart before the configured 24-hour interval preserves chat history.
  server.Stop();
  QSqlQuery resetState(verification.Conn());
  resetState.prepare(QStringLiteral("UPDATE bloodborne_web_chat_state SET "
                                    "value=? WHERE key='last_chat_reset'"));
  resetState.addBindValue(QDateTime::currentSecsSinceEpoch() - 23 * 60 * 60);
  CHECK(resetState.exec());
  BloodborneWebsiteServer notDue;
  CHECK(notDue.Start(&config, dbPath, &shared));
  const HttpResult chatBeforeReset =
      Send(Url(notDue, QStringLiteral("/api/chat/messages")),
           QByteArrayLiteral("GET"));
  CHECK(!Data(chatBeforeReset)
             .value(QStringLiteral("messages"))
             .toArray()
             .isEmpty());
  notDue.Stop();

  // A restart after the interval performs the reset before serving requests.
  const qint64 resetStartedAt = QDateTime::currentSecsSinceEpoch();
  resetState.bindValue(0, resetStartedAt - 25 * 60 * 60);
  CHECK(resetState.exec());
  BloodborneWebsiteServer restarted;
  CHECK(restarted.Start(&config, dbPath, &shared));
  const HttpResult chatAfterReset =
      Send(Url(restarted, QStringLiteral("/api/chat/messages")),
           QByteArrayLiteral("GET"));
  CHECK(Data(chatAfterReset)
            .value(QStringLiteral("messages"))
            .toArray()
            .isEmpty());
  CHECK(tableCount(QStringLiteral("bloodborne_web_chat_message")) == 0);
  for (const QString &table : preservedTables)
    CHECK(tableCount(table) == countsBeforeReset.value(table));
  CHECK(QFileInfo::exists(QDir(temporary.filePath(QStringLiteral(
                                   "data/bloodborne-website/avatars")))
                              .filePath(QFileInfo(avatarUrl).fileName())));
  QSqlQuery currentReset(verification.Conn());
  CHECK(currentReset.exec(
      QStringLiteral("SELECT value FROM bloodborne_web_chat_state WHERE "
                     "key='last_chat_reset'")));
  CHECK(currentReset.next());
  CHECK(currentReset.value(0).toLongLong() >= resetStartedAt);

  // Web sessions are persisted as token hashes and remain valid across normal
  // website listener restarts and a chat-only reset.
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
      QStringLiteral("SELECT COUNT(*) FROM migration WHERE migration_id=7")));
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
            .body.contains("--background:"));
  embeddedOnlyServer.Stop();
  return 0;
}
