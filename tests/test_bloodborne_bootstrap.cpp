// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include <cstdlib>
#include <initializer_list>
#include <iostream>

#include <QCoreApplication>
#include <QEventLoop>
#include <QHostAddress>
#include <QHttpServer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSqlQuery>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QTimer>

#include "bloodborne_bootstrap.h"
#include "bloodborne_ssinfo_reference.h"
#include "client_session.h"
#include "database.h"
#include "webapi_routes_bloodborne.h"
#include "webapi_routes_bloodborne_bootstrap.h"

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

bool InsertAccount(Database &db, const QString &npid, const QString &email,
                   const QString &token) {
  QSqlQuery query(db.Conn());
  query.prepare(QStringLiteral(
      "INSERT INTO account(username, hash, salt, avatar_url, email, "
      "email_check, token, admin, "
      "stat_agent, banned) VALUES(?, ?, ?, '', ?, ?, ?, 0, 0, 0)"));
  query.addBindValue(npid);
  query.addBindValue(QByteArray("hash"));
  query.addBindValue(QByteArray("salt"));
  query.addBindValue(email);
  query.addBindValue(email);
  query.addBindValue(token);
  return query.exec();
}

struct HttpResult {
  bool finished = false;
  int status = 0;
  qint64 contentLength = -1;
  QByteArray body;
};

HttpResult Send(const QUrl &url, const QByteArray &method,
                const QByteArray &body = {},
                const QByteArray &contentType = {}) {
  QNetworkAccessManager manager;
  QNetworkRequest request(url);
  if (!contentType.isEmpty())
    request.setRawHeader("Content-Type", contentType);
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
    result.contentLength =
        reply->header(QNetworkRequest::ContentLengthHeader).toLongLong();
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

HttpResult Post(const QTcpServer &server, const QString &path,
                const QJsonObject &body) {
  return Send(ServerUrl(server, path), "POST",
              QJsonDocument(body).toJson(QJsonDocument::Compact),
              "application/json");
}

QJsonObject Object(const HttpResult &result) {
  return QJsonDocument::fromJson(result.body).object();
}

QJsonObject SessionRequest(const QString &messageId, qint64 userId,
                           const QString &sessionId) {
  QJsonObject body;
  body.insert(QStringLiteral("MessageId"), messageId);
  body.insert(QStringLiteral("UserId"), userId);
  body.insert(QStringLiteral("SessionId"), sessionId);
  return body;
}

bool IsSuccessful(const HttpResult &result, const QString &messageId,
                  std::initializer_list<const char *> emptyLists = {}) {
  if (!result.finished || result.status != 200)
    return false;
  const QJsonObject body = Object(result);
  if (body.value(QStringLiteral("MessageId")).toString() != messageId ||
      body.value(QStringLiteral("ResKind")).toInt(-1) != 0) {
    return false;
  }
  for (const char *name : emptyLists) {
    if (!body.value(QLatin1String(name)).isArray() ||
        !body.value(QLatin1String(name)).toArray().isEmpty()) {
      return false;
    }
  }
  return true;
}

} // namespace

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);

  QString referenceError;
  QByteArray decodedReference;
  CHECK(Bloodborne::ValidateReferenceServerStatusInfo(&referenceError,
                                                      &decodedReference));
  CHECK(referenceError.isEmpty());
  CHECK(decodedReference.size() ==
        Bloodborne::ReferenceDecodedServerStatusInfoSize);

  const QString baseUrl = QStringLiteral("http://73.244.12.22:31315");
  QString localError;
  QByteArray decodedLocal;
  const QByteArray encodedLocal = Bloodborne::BuildServerStatusInfo(
      baseUrl + '/', &decodedLocal, &localError);
  CHECK(localError.isEmpty());
  CHECK(!encodedLocal.isEmpty());
  CHECK(!encodedLocal.startsWith("<ss>"));
  const auto roundTrip = QByteArray::fromBase64Encoding(
      encodedLocal, QByteArray::AbortOnBase64DecodingErrors);
  CHECK(static_cast<bool>(roundTrip));
  CHECK(roundTrip.decoded == decodedLocal);
  CHECK(decodedLocal.startsWith("<ss>0</ss>"));
  CHECK(decodedLocal.contains("<gameurl2>"));
  CHECK(decodedLocal.count("<api_") == 37);
  CHECK(!decodedLocal.contains("thehuntersdream.com:18671"));
  CHECK(!decodedLocal.contains("/basic_utils/"));
  CHECK(!decodedLocal.contains("/summon_messenger/"));
  for (const Bloodborne::BootstrapApi &api : Bloodborne::BootstrapApis()) {
    const QByteArray opening = QByteArray("<") + api.name + ">";
    const QByteArray closing = QByteArray("</") + api.name + ">";
    CHECK(decodedLocal.count(opening) == 1);
    CHECK(decodedLocal.contains(opening + baseUrl.toUtf8() + closing));
  }

  const QJsonObject loginBuilder =
      Bloodborne::BuildLoginResponse(42, 4, QStringLiteral("session-42"));
  CHECK(loginBuilder.value(QStringLiteral("MessageId")).toString() ==
        QStringLiteral("LoginResponse"));
  CHECK(loginBuilder.value(QStringLiteral("ResKind")).toInt(-1) == 0);
  CHECK(loginBuilder.value(QStringLiteral("SessionId")).toString() ==
        QStringLiteral("session-42"));

  QTemporaryDir directory;
  CHECK(directory.isValid());
  Database db(QStringLiteral("bloodborne_bootstrap_test"));
  CHECK(db.Open(directory.filePath(QStringLiteral("test.db"))));
  CHECK(InsertAccount(db, QStringLiteral("Izuku"),
                      QStringLiteral("izuku@example.test"),
                      QStringLiteral("token-one")));

  SharedState shared;
  shared.config = nullptr;
  SharedState::ClientEntry client;
  client.npid = QStringLiteral("Izuku");
  client.peerAddress = QHostAddress::LocalHost;
  shared.clients.insert(1, client);
  shared.npidToUserId.insert(QStringLiteral("Izuku"), 1);

  QHttpServer http;
  WebApiRoutes::RegisterBloodborneBootstrapRoutes(http, db, shared, baseUrl,
                                                  encodedLocal, false);
  WebApiRoutes::RegisterBloodborneRoutes(http, false);
  QTcpServer tcp;
  CHECK(tcp.listen(QHostAddress::LocalHost, 0));
  CHECK(http.bind(&tcp));

  const HttpResult ssInfo =
      Send(ServerUrl(tcp, QStringLiteral("/bb-eu/ss.info")), "GET");
  CHECK(ssInfo.finished);
  CHECK(ssInfo.status == 200);
  CHECK(ssInfo.contentLength == encodedLocal.size());
  CHECK(ssInfo.body == encodedLocal);

  QJsonObject loginRequest;
  loginRequest.insert(QStringLiteral("ApplicationVersion"), 9);
  loginRequest.insert(QStringLiteral("AuthorizationCode"),
                      QStringLiteral("test-code"));
  loginRequest.insert(QStringLiteral("IssuerId"), 100);
  loginRequest.insert(QStringLiteral("LanguageId"), 4);
  loginRequest.insert(QStringLiteral("MessageId"),
                      QStringLiteral("LoginRequest"));
  loginRequest.insert(QStringLiteral("NatType"), 0);
  loginRequest.insert(QStringLiteral("PlatformAccountId"),
                      QStringLiteral("Izuku"));
  loginRequest.insert(QStringLiteral("RegionId"), 2);
  const HttpResult loginResult =
      Post(tcp, QStringLiteral("/basic_utils/login"), loginRequest);
  CHECK(loginResult.finished);
  CHECK(loginResult.status == 200);
  const QJsonObject login = Object(loginResult);
  CHECK(login.value(QStringLiteral("MessageId")).toString() ==
        QStringLiteral("LoginResponse"));
  CHECK(login.value(QStringLiteral("ResKind")).toInt(-1) == 0);
  CHECK(login.value(QStringLiteral("UserId")).toVariant().toLongLong() == 1);
  const QString sessionId = login.value(QStringLiteral("SessionId")).toString();
  CHECK(!sessionId.isEmpty());

  QJsonObject notice =
      SessionRequest(QStringLiteral("NoticeNormalGetRequest"), 1, sessionId);
  notice.insert(QStringLiteral("Language"), 4);
  notice.insert(QStringLiteral("Region"), 2);
  CHECK(IsSuccessful(
      Post(tcp, QStringLiteral("/basic_utils/get_normal_notice"), notice),
      QStringLiteral("NoticeNormalGetResponse"), {"NoticeList"}));

  QJsonObject penalty = SessionRequest(
      QStringLiteral("UserPropertiesMoveCountCheckRequest"), 1, sessionId);
  penalty.insert(QStringLiteral("Count"), 0);
  CHECK(IsSuccessful(
      Post(tcp,
           QStringLiteral("/penalty/check_user_priority_move_count?user_id=1"),
           penalty),
      QStringLiteral("UserPropertiesMoveCountCheckResponse")));

  QJsonObject search =
      SessionRequest(QStringLiteral("BloodMessSearchAddRequest"), 1, sessionId);
  search.insert(QStringLiteral("BloodMessIdList"), QJsonArray{});
  search.insert(QStringLiteral("CharaId"), 1001);
  CHECK(IsSuccessful(
      Post(tcp, QStringLiteral("/blood_messenger/exist_messages"), search),
      QStringLiteral("BloodMessSearchAddResponse"),
      {"BloodMessEvaluationList", "LostBloodMessIdList"}));

  QJsonObject shell = SessionRequest(
      QStringLiteral("MessengerShellUploadRequest"), 1, sessionId);
  shell.insert(QStringLiteral("CharaId"), 1001);
  shell.insert(QStringLiteral("ShellData"), QStringLiteral("AQAAAAAAAAA="));
  shell.insert(QStringLiteral("ShellDataVersion"), 2);
  CHECK(
      IsSuccessful(Post(tcp, QStringLiteral("/messenger_shell/upload"), shell),
                   QStringLiteral("MessengerShellUploadResponse")));

  const QString checkTime = QStringLiteral("2026-08-22T18:05:14");
  QJsonObject emergency =
      SessionRequest(QStringLiteral("NoticeEmergencyGetRequest"), 1, sessionId);
  emergency.insert(QStringLiteral("CheckTime"), checkTime);
  emergency.insert(QStringLiteral("Language"), 4);
  emergency.insert(QStringLiteral("Region"), 2);
  const HttpResult emergencyResult =
      Post(tcp, QStringLiteral("/basic_utils/get_emergency_notice"), emergency);
  CHECK(IsSuccessful(emergencyResult,
                     QStringLiteral("NoticeEmergencyGetResponse"),
                     {"NoticeList"}));
  CHECK(Object(emergencyResult).value(QStringLiteral("CheckTime")).toString() ==
        checkTime);

  QJsonObject ghost =
      SessionRequest(QStringLiteral("WanderingGhostGetRequest"), 1, sessionId);
  ghost.insert(QStringLiteral("AreaList"), QJsonArray{});
  ghost.insert(QStringLiteral("GetMaxCount"), 10);
  ghost.insert(QStringLiteral("JoinedCharaIdList"), QJsonArray{});
  ghost.insert(QStringLiteral("MatchingLevel"), 40);
  ghost.insert(QStringLiteral("WanderingGhostDataVersion"), 2);
  CHECK(IsSuccessful(Post(tcp, QStringLiteral("/wandering_ghost/get"), ghost),
                     QStringLiteral("WanderingGhostGetResponse"),
                     {"WanderingGhostList"}));

  QJsonObject chair =
      SessionRequest(QStringLiteral("ChairMessGetListRequest"), 1, sessionId);
  chair.insert(QStringLiteral("CharaId"), 1001);
  chair.insert(QStringLiteral("WarpInfoList"), QJsonArray{});
  CHECK(IsSuccessful(Post(tcp, QStringLiteral("/chair_messenger/get"), chair),
                     QStringLiteral("ChairMessGetListResponse"),
                     {"ChairMessList"}));

  QJsonObject channel =
      SessionRequest(QStringLiteral("ChannelGetInfoRequest"), 1, sessionId);
  channel.insert(QStringLiteral("ChannelIdList"), QJsonArray{});
  CHECK(IsSuccessful(Post(tcp, QStringLiteral("/channel/get_info"), channel),
                     QStringLiteral("ChannelGetInfoResponse"),
                     {"ChannelInfoList", "LostChannelIdList"}));

  QJsonObject evaluation = SessionRequest(
      QStringLiteral("BloodMessGetEvaluateRequest"), 1, sessionId);
  evaluation.insert(QStringLiteral("BloodMessIdList"), QJsonArray{});
  CHECK(IsSuccessful(
      Post(tcp, QStringLiteral("/blood_messenger/evaluation"), evaluation),
      QStringLiteral("BloodMessGetEvaluateResponse"),
      {"BloodMessEvaluationList"}));

  QJsonObject bloodList =
      SessionRequest(QStringLiteral("BloodMessGetListRequest"), 1, sessionId);
  bloodList.insert(QStringLiteral("AreaInfoList"), QJsonArray{});
  bloodList.insert(QStringLiteral("BloodDataVersion"), 2);
  bloodList.insert(QStringLiteral("CharaDataVersion"), 2);
  bloodList.insert(QStringLiteral("GetMaxCount"), 100);
  bloodList.insert(QStringLiteral("MessShellInfoVersion"), 2);
  CHECK(IsSuccessful(
      Post(tcp, QStringLiteral("/blood_messenger/message_area"), bloodList),
      QStringLiteral("BloodMessGetListResponse"), {"BloodMessList"}));

  QJsonObject tombList =
      SessionRequest(QStringLiteral("TombMessGetListRequest"), 1, sessionId);
  tombList.insert(QStringLiteral("AreaInfoList"), QJsonArray{});
  tombList.insert(QStringLiteral("GetMaxCount"), 100);
  tombList.insert(QStringLiteral("MessShellInfoVersion"), 2);
  tombList.insert(QStringLiteral("TombDataVersion"), 2);
  CHECK(IsSuccessful(
      Post(tcp, QStringLiteral("/tomb_messenger/message_area"), tombList),
      QStringLiteral("TombMessGetListResponse"), {"TombMessList"}));

  QJsonObject invalidSession = penalty;
  invalidSession.insert(QStringLiteral("SessionId"),
                        QStringLiteral("not-a-session"));
  CHECK(
      Post(tcp,
           QStringLiteral("/penalty/check_user_priority_move_count?user_id=1"),
           invalidSession)
          .status == 401);
  QJsonObject malformedPenalty = penalty;
  malformedPenalty.remove(QStringLiteral("Count"));
  CHECK(
      Post(tcp,
           QStringLiteral("/penalty/check_user_priority_move_count?user_id=1"),
           malformedPenalty)
          .status == 400);

  QJsonObject summonCreate =
      SessionRequest(QStringLiteral("SummonDataCreateRequest"), 1, sessionId);
  summonCreate.insert(QStringLiteral("AreaId"), 1);
  summonCreate.insert(QStringLiteral("AreaRegionId"), 2);
  summonCreate.insert(QStringLiteral("SummonData"),
                      QStringLiteral("local-data"));
  summonCreate.insert(QStringLiteral("SummonDataVersion"), 3);
  summonCreate.insert(QStringLiteral("SummonType"), 0);
  summonCreate.insert(QStringLiteral("MatchingLevel"), 40);
  CHECK(IsSuccessful(
      Post(tcp, QStringLiteral("/summon_messenger/create"), summonCreate),
      QStringLiteral("SummonDataCreateResponse")));

  QJsonObject summonGet =
      SessionRequest(QStringLiteral("SummonDataGetListRequest"), 2,
                     QStringLiteral("searcher-session"));
  summonGet.insert(QStringLiteral("AreaId"), 1);
  summonGet.insert(QStringLiteral("AreaRegionId"), 2);
  summonGet.insert(QStringLiteral("MatchingLevel"), 40);
  const HttpResult summonGetResult =
      Post(tcp, QStringLiteral("/summon_messenger/get"), summonGet);
  CHECK(IsSuccessful(summonGetResult,
                     QStringLiteral("SummonDataGetListResponse")));
  CHECK(Object(summonGetResult)
            .value(QStringLiteral("SummonDataList"))
            .isArray());

  QJsonObject summonRequest =
      SessionRequest(QStringLiteral("SummonDataSummonRequest"), 2,
                     QStringLiteral("searcher-session"));
  summonRequest.insert(QStringLiteral("TargetUserId"), 1);
  CHECK(IsSuccessful(
      Post(tcp, QStringLiteral("/summon_messenger/request"), summonRequest),
      QStringLiteral("SummonDataSummonResponse")));

  QJsonObject summonDelete =
      SessionRequest(QStringLiteral("SummonDataRemoveRequest"), 1, sessionId);
  CHECK(IsSuccessful(
      Post(tcp, QStringLiteral("/summon_messenger/delete"), summonDelete),
      QStringLiteral("SummonDataRemoveResponse")));

  return 0;
}
