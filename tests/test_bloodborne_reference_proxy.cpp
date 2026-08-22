// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include <iostream>
#include <memory>

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QHostAddress>
#include <QHttpServer>
#include <QHttpServerRequest>
#include <QHttpServerResponder>
#include <QHttpServerResponse>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QTimer>

#include "bloodborne_reference_proxy.h"

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
  QByteArray contentType;
  QByteArray body;
};

HttpResult Send(const QUrl &url, const QByteArray &method,
                const QByteArray &body = {}, const QByteArray &contentType = {},
                const QByteArray &userAgent = {}) {
  QNetworkAccessManager manager;
  QNetworkRequest request{url};
  if (!contentType.isEmpty())
    request.setRawHeader("Content-Type", contentType);
  if (!userAgent.isEmpty())
    request.setRawHeader("User-Agent", userAgent);
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
    result.contentType = reply->rawHeader("Content-Type");
    result.body = reply->readAll();
  } else {
    reply->abort();
  }
  delete reply;
  return result;
}

QUrl ServerUrl(const QTcpServer &server, const QString &pathAndQuery) {
  return QUrl(QStringLiteral("http://127.0.0.1:%1%2")
                  .arg(server.serverPort())
                  .arg(pathAndQuery));
}

QByteArray ReadFile(const QString &path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly))
    return {};
  return file.readAll();
}

QStringList CapturedMessages;

void CaptureMessage(QtMsgType, const QMessageLogContext &,
                    const QString &message) {
  CapturedMessages.append(message);
}

struct ReceivedRequest {
  QByteArray method;
  QString path;
  QString query;
  QByteArray body;
  QByteArray contentType;
  QByteArray userAgent;
};

} // namespace

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);
  qputenv("SHADNET_BLOODBORNE_REFERENCE_TRACE", "1");

  QList<ReceivedRequest> upstreamRequests;
  QList<std::shared_ptr<QHttpServerResponder>> heldResponders;
  QHttpServer upstream;
  upstream.setMissingHandler(&upstream, [&](const QHttpServerRequest &request,
                                            QHttpServerResponder &responder) {
    upstreamRequests.append(
        {request.method() == QHttpServerRequest::Method::Post
             ? QByteArray("POST")
             : QByteArray("OTHER"),
         request.url().path(), request.url().query(QUrl::FullyEncoded),
         request.body(), request.value("Content-Type"),
         request.value("User-Agent")});

    if (request.url().path() == QStringLiteral("/penalty/timeout")) {
      heldResponders.append(
          std::make_shared<QHttpServerResponder>(std::move(responder)));
      return;
    }
    if (request.url().path() == QStringLiteral("/basic_utils/login")) {
      responder.sendResponse(QHttpServerResponse{
          "application/json",
          "{\"ResKind\":0,\"UserId\":2880,\"SessionId\":\"upstream\"}",
          QHttpServerResponse::StatusCode::Ok});
      return;
    }
    responder.sendResponse(QHttpServerResponse{
        "application/json", "{\"ResKind\":0,\"MoveCount\":7}",
        static_cast<QHttpServerResponse::StatusCode>(418)});
  });
  QTcpServer upstreamTcp;
  CHECK(upstreamTcp.listen(QHostAddress::LocalHost, 0));
  CHECK(upstream.bind(&upstreamTcp));

  QTemporaryDir temporary;
  CHECK(temporary.isValid());
  Bloodborne::ReferenceProxy::Options options;
  options.upstreamUrl = ServerUrl(upstreamTcp, QStringLiteral("/"));
  options.captureRoot =
      temporary.filePath(QStringLiteral("captures/bloodborne-reference"));
  options.transferTimeoutMs = 100;
  Bloodborne::ReferenceProxy proxy(options);
  QString proxyError;
  CHECK(proxy.Initialize(&proxyError));
  CHECK(proxyError.isEmpty());

  CHECK(Bloodborne::ReferenceProxy::IsReferenceBackendPath(
      QStringLiteral("/basic_utils/login")));
  CHECK(Bloodborne::ReferenceProxy::IsReferenceBackendPath(
      QStringLiteral("/penalty/check_user_priority_move_count")));
  CHECK(!Bloodborne::ReferenceProxy::IsReferenceBackendPath(
      QStringLiteral("/bb-eu/ss.info")));
  CHECK(!Bloodborne::ReferenceProxy::IsReferenceBackendPath(
      QStringLiteral("/summon_messenger/create")));
  CHECK(!Bloodborne::ReferenceProxy::IsReferenceBackendPath(
      QStringLiteral("/users/me")));

  QHttpServer local;
  int localSummonCalls = 0;
  local.route("/summon_messenger/create", QHttpServerRequest::Method::Post,
              [&](const QHttpServerRequest &) {
                ++localSummonCalls;
                return QHttpServerResponse{"application/json",
                                           "{\"local\":true}",
                                           QHttpServerResponse::StatusCode::Ok};
              });
  local.setMissingHandler(&local, [&](const QHttpServerRequest &request,
                                      QHttpServerResponder &responder) {
    if (Bloodborne::ReferenceProxy::IsReferenceBackendPath(
            request.url().path())) {
      proxy.Forward(request, std::move(responder));
      return;
    }
    responder.sendResponse(QHttpServerResponse{
        "application/json", "{\"error\":\"local not found\"}",
        QHttpServerResponse::StatusCode::NotFound});
  });
  QTcpServer localTcp;
  CHECK(localTcp.listen(QHostAddress::LocalHost, 0));
  CHECK(local.bind(&localTcp));

  const QtMessageHandler previousHandler =
      qInstallMessageHandler(CaptureMessage);
  const QByteArray loginBody = "{\"MessageId\":\"LoginRequest\","
                               "\"AuthorizationCode\":\"secret-auth-code\","
                               "\"PlatformAccountId\":\"Hunter\"}";
  const HttpResult login =
      Send(ServerUrl(localTcp, QStringLiteral("/basic_utils/login")), "POST",
           loginBody, "application/json", "Bloodborne-Test-Agent");
  CHECK(login.finished);
  CHECK(login.status == 200);
  CHECK(login.body ==
        "{\"ResKind\":0,\"UserId\":2880,\"SessionId\":\"upstream\"}");
  CHECK(upstreamRequests.size() == 1);
  CHECK(upstreamRequests[0].path == QStringLiteral("/basic_utils/login"));
  CHECK(upstreamRequests[0].body == loginBody);
  CHECK(upstreamRequests[0].contentType == "application/json");
  CHECK(upstreamRequests[0].userAgent == "Bloodborne-Test-Agent");

  const QByteArray penaltyBody =
      "{\"MessageId\":\"UserPropertiesMoveCountCheckRequest\",\"MoveCount\":7}";
  const HttpResult penalty = Send(
      ServerUrl(
          localTcp,
          QStringLiteral(
              "/penalty/check_user_priority_move_count?user_id=2880&slot=2")),
      "POST", penaltyBody, "application/json");
  CHECK(penalty.finished);
  CHECK(penalty.status == 418);
  CHECK(penalty.body == "{\"ResKind\":0,\"MoveCount\":7}");
  CHECK(upstreamRequests.size() == 2);
  CHECK(upstreamRequests[1].path ==
        QStringLiteral("/penalty/check_user_priority_move_count"));
  CHECK(upstreamRequests[1].query == QStringLiteral("user_id=2880&slot=2"));
  CHECK(upstreamRequests[1].body == penaltyBody);

  const int upstreamCallsBeforeSummon = upstreamRequests.size();
  const HttpResult summon =
      Send(ServerUrl(localTcp, QStringLiteral("/summon_messenger/create")),
           "POST", "{}", "application/json");
  CHECK(summon.finished);
  CHECK(summon.status == 200);
  CHECK(summon.body == "{\"local\":true}");
  CHECK(localSummonCalls == 1);
  CHECK(upstreamRequests.size() == upstreamCallsBeforeSummon);

  const HttpResult timeout =
      Send(ServerUrl(localTcp, QStringLiteral("/penalty/timeout")), "POST",
           "{}", "application/json");
  CHECK(timeout.finished);
  CHECK(timeout.status == 504);
  CHECK(timeout.body.contains("Bloodborne reference upstream request failed"));
  CHECK(upstreamRequests.size() == 3);

  qInstallMessageHandler(previousHandler);
  const QString capturedLog = CapturedMessages.join(QLatin1Char('\n'));
  CHECK(capturedLog.contains(QStringLiteral("[BLOODBORNE REFERENCE REQUEST]")));
  CHECK(
      capturedLog.contains(QStringLiteral("[BLOODBORNE REFERENCE RESPONSE]")));
  CHECK(capturedLog.contains(QStringLiteral("<redacted>")));
  CHECK(!capturedLog.contains(QStringLiteral("secret-auth-code")));

  const QDir captureDirectory(proxy.CaptureDirectory());
  CHECK(captureDirectory.exists());
  const QByteArray capturedLogin =
      ReadFile(captureDirectory.filePath(QStringLiteral("0001-request.json")));
  CHECK(capturedLogin.contains("AuthorizationCode"));
  CHECK(capturedLogin.contains("<redacted>"));
  CHECK(!capturedLogin.contains("secret-auth-code"));
  const QByteArray manifest =
      ReadFile(captureDirectory.filePath(QStringLiteral("manifest.jsonl")));
  CHECK(manifest.count('\n') == 3);
  CHECK(manifest.contains("check_user_priority_move_count"));
  CHECK(manifest.contains("user_id=2880&slot=2"));
  CHECK(ReadFile(captureDirectory.filePath(QStringLiteral("summary.txt")))
            .startsWith("Bloodborne reference capture summary"));
  CHECK(ReadFile(captureDirectory.filePath(QStringLiteral("summary.json")))
            .contains("check_user_priority_move_count"));

  QHttpServer disabled;
  int localLoginCalls = 0;
  disabled.route("/basic_utils/login", QHttpServerRequest::Method::Post,
                 [&](const QHttpServerRequest &) {
                   ++localLoginCalls;
                   return QHttpServerResponse{
                       "application/json", "{\"local_login\":true}",
                       QHttpServerResponse::StatusCode::Ok};
                 });
  QTcpServer disabledTcp;
  CHECK(disabledTcp.listen(QHostAddress::LocalHost, 0));
  CHECK(disabled.bind(&disabledTcp));
  const int upstreamCallsBeforeDisabled = upstreamRequests.size();
  const HttpResult disabledLogin =
      Send(ServerUrl(disabledTcp, QStringLiteral("/basic_utils/login")), "POST",
           loginBody, "application/json");
  CHECK(disabledLogin.status == 200);
  CHECK(disabledLogin.body == "{\"local_login\":true}");
  CHECK(localLoginCalls == 1);
  CHECK(upstreamRequests.size() == upstreamCallsBeforeDisabled);

  heldResponders.clear();
  qunsetenv("SHADNET_BLOODBORNE_REFERENCE_TRACE");
  return 0;
}
