// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include <iostream>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QEventLoop>
#include <QHostAddress>
#include <QHttpServer>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSqlQuery>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QTimer>

#include "database.h"
#include "webapi_routes_activity.h"

namespace {

QMutex MessageMutex;
QStringList Messages;

bool Check(bool condition, const char *expression, int line) {
  if (!condition) {
    std::cerr << "check failed at line " << line << ": " << expression << '\n';
  }
  return condition;
}

#define CHECK(expression)                                                      \
  do {                                                                         \
    if (!Check((expression), #expression, __LINE__))                           \
      return 1;                                                                \
  } while (false)

void CaptureMessage(QtMsgType, const QMessageLogContext &,
                    const QString &message) {
  QMutexLocker lock(&MessageMutex);
  Messages.append(message);
}

void ClearMessages() {
  QMutexLocker lock(&MessageMutex);
  Messages.clear();
}

QString CapturedMessages() {
  QMutexLocker lock(&MessageMutex);
  return Messages.join(QLatin1Char('\n'));
}

bool InsertAccount(Database &db, const QString &npid, const QString &token) {
  QSqlQuery query(db.Conn());
  query.prepare(QStringLiteral(
      "INSERT INTO account(username, hash, salt, avatar_url, email, "
      "email_check, token, admin, stat_agent, banned) "
      "VALUES(?, ?, ?, '', ?, ?, ?, 0, 0, 0)"));
  query.addBindValue(npid);
  query.addBindValue(QByteArray("hash"));
  query.addBindValue(QByteArray("salt"));
  query.addBindValue(npid.toLower() + QStringLiteral("@example.test"));
  query.addBindValue(npid.toLower() + QStringLiteral("@example.test"));
  query.addBindValue(token);
  return query.exec();
}

struct HttpResult {
  bool finished = false;
  int status = 0;
  QByteArray body;
};

HttpResult Post(const QTcpServer &server, const QString &path,
                const QByteArray &body, const QByteArray &token = {},
                const QByteArray &contentType =
                    QByteArrayLiteral("application/json; charset=utf-8")) {
  QNetworkAccessManager manager;
  QNetworkRequest request(QUrl(QStringLiteral("http://127.0.0.1:%1%2")
                                   .arg(server.serverPort())
                                   .arg(path)));
  if (!contentType.isEmpty()) {
    request.setRawHeader(QByteArrayLiteral("Content-Type"), contentType);
  }
  if (!token.isEmpty()) {
    request.setRawHeader(QByteArrayLiteral("Authorization"),
                         QByteArrayLiteral("Bearer ") + token);
  }

  QNetworkReply *reply = manager.post(request, body);
  QEventLoop loop;
  QTimer timeout;
  timeout.setSingleShot(true);
  QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
  QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
  timeout.start(5'000);
  loop.exec();

  HttpResult result;
  result.finished = reply->isFinished();
  if (result.finished) {
    result.status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    result.body = reply->readAll();
  } else {
    reply->abort();
  }
  delete reply;
  return result;
}

} // namespace

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);
  const QtMessageHandler previousHandler =
      qInstallMessageHandler(CaptureMessage);

  QTemporaryDir temporary;
  CHECK(temporary.isValid());
  Database db(QStringLiteral("webapi_activity_feed_test"));
  CHECK(db.Open(temporary.filePath(QStringLiteral("test.db"))));
  CHECK(
      InsertAccount(db, QStringLiteral("Izuku"), QStringLiteral("token-one")));
  CHECK(InsertAccount(db, QStringLiteral("Zeight725"),
                      QStringLiteral("token-two")));

  QHttpServer quietHttp;
  WebApiRoutes::RegisterActivityFeedRoutes(quietHttp, db, false);
  QTcpServer quietTcp;
  CHECK(quietTcp.listen(QHostAddress::LocalHost, 0));
  CHECK(quietHttp.bind(&quietTcp));

  const QByteArray unknownValidJson =
      R"({"unknownEvent":{"kind":7,"members":["A","B"]}})";
  ClearMessages();
  HttpResult result = Post(quietTcp, QStringLiteral("/v1/users/Izuku/feed"),
                           unknownValidJson, QByteArrayLiteral("token-one"));
  CHECK(result.finished);
  CHECK(result.status == 200);
  CHECK(result.body.isEmpty());
  CHECK(
      !CapturedMessages().contains(QStringLiteral("[BLOODBORNE FEED TRACE]")));

  CHECK(Post(quietTcp, QStringLiteral("/v1/users/Izuku/feed"), unknownValidJson)
            .status == 401);
  CHECK(Post(quietTcp, QStringLiteral("/v1/users/Izuku/feed"), unknownValidJson,
             QByteArrayLiteral("invalid-token"))
            .status == 401);
  CHECK(Post(quietTcp, QStringLiteral("/v1/users/Unknown/feed"),
             unknownValidJson, QByteArrayLiteral("token-one"))
            .status == 403);
  CHECK(Post(quietTcp, QStringLiteral("/v1/users/Zeight725/feed"),
             unknownValidJson, QByteArrayLiteral("token-one"))
            .status == 403);
  CHECK(Post(quietTcp, QStringLiteral("/v1/users/me/feed"), unknownValidJson,
             QByteArrayLiteral("token-one"))
            .status == 200);
  CHECK(Post(quietTcp, QStringLiteral("/v1/users/Izuku/feed"), QByteArray{},
             QByteArrayLiteral("token-one"))
            .status == 400);
  CHECK(Post(quietTcp, QStringLiteral("/v1/users/Izuku/feed"),
             QByteArrayLiteral("not-json"), QByteArrayLiteral("token-one"))
            .status == 400);
  CHECK(Post(quietTcp, QStringLiteral("/v1/users/Izuku/feed"), unknownValidJson,
             QByteArrayLiteral("token-one"), QByteArrayLiteral("text/plain"))
            .status == 415);

  QHttpServer tracedHttp;
  WebApiRoutes::RegisterActivityFeedRoutes(tracedHttp, db, true);
  QTcpServer tracedTcp;
  CHECK(tracedTcp.listen(QHostAddress::LocalHost, 0));
  CHECK(tracedHttp.bind(&tracedTcp));

  const QByteArray sensitiveBody =
      R"({"event":"joined","count":2,"active":true,"SessionId":"session-secret","Authorization":"Bearer body-secret","nested":{"accessToken":"nested-secret","password":"password-secret"},"note":"Authorization: Bearer inline-secret"})";
  const QString expectedSha = QString::fromLatin1(
      QCryptographicHash::hash(sensitiveBody, QCryptographicHash::Sha256)
          .toHex());
  ClearMessages();
  result = Post(tracedTcp, QStringLiteral("/v1/users/Izuku/feed"),
                sensitiveBody, QByteArrayLiteral("token-one"));
  CHECK(result.status == 200);
  const QString trace = CapturedMessages();
  CHECK(trace.contains(QStringLiteral("[BLOODBORNE FEED TRACE]")));
  CHECK(trace.contains(QStringLiteral("npid=Izuku")));
  CHECK(trace.contains(
      QStringLiteral("body_bytes=%1").arg(sensitiveBody.size())));
  CHECK(trace.contains(QStringLiteral("sha256=") + expectedSha));
  CHECK(trace.contains(QStringLiteral("\"event\":\"string\"")));
  CHECK(trace.contains(QStringLiteral("\"count\":\"number\"")));
  CHECK(trace.contains(QStringLiteral("\"event\":\"joined\"")));
  CHECK(trace.contains(QStringLiteral("[REDACTED]")));
  CHECK(!trace.contains(QStringLiteral("token-one")));
  CHECK(!trace.contains(QStringLiteral("body-secret")));
  CHECK(!trace.contains(QStringLiteral("nested-secret")));
  CHECK(!trace.contains(QStringLiteral("password-secret")));
  CHECK(!trace.contains(QStringLiteral("inline-secret")));
  CHECK(!trace.contains(QStringLiteral("Bearer token-one")));

  qInstallMessageHandler(previousHandler);
  std::cout << "WebAPI activity feed test passed\n";
  return 0;
}
