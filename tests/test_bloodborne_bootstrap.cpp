// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include <cstdlib>
#include <iostream>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QEventLoop>
#include <QHostAddress>
#include <QHttpServer>
#include <QHttpServerResponse>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSqlQuery>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QTimer>

#include "bloodborne_bootstrap.h"
#include "bloodborne_ssinfo_reference.h"
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

struct HttpGetResult {
  bool finished = false;
  int status = 0;
  qint64 contentLength = -1;
  QByteArray body;
};

HttpGetResult Get(const QUrl &url) {
  QNetworkAccessManager manager;
  QNetworkReply *reply = manager.get(QNetworkRequest(url));
  QEventLoop loop;
  QTimer timeout;
  timeout.setSingleShot(true);
  QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
  QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
  timeout.start(5000);
  loop.exec();

  HttpGetResult result;
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
  CHECK(decodedReference.startsWith("<ss>0</ss>"));
  CHECK(decodedReference.contains("<gameurl2>"));
  CHECK(decodedReference.count("<api_") == 37);
  CHECK(QCryptographicHash::hash(decodedReference, QCryptographicHash::Sha256)
            .toHex()
            .toUpper() == Bloodborne::ReferenceDecodedServerStatusInfoSha256);

  QHttpServer http;
  http.route("/bb-eu/ss.info", QHttpServerRequest::Method::Get,
             [](const QHttpServerRequest &) {
               return QHttpServerResponse{
                   Bloodborne::ServerStatusInfoContentType,
                   Bloodborne::ReferenceServerStatusInfo(),
                   QHttpServerResponse::StatusCode::Ok};
             });
  QTcpServer tcp;
  CHECK(tcp.listen(QHostAddress::LocalHost, 0));
  CHECK(http.bind(&tcp));

  const HttpGetResult referenceResponse =
      Get(QUrl(QStringLiteral("http://127.0.0.1:%1/bb-eu/ss.info")
                   .arg(tcp.serverPort())));
  CHECK(referenceResponse.finished);
  CHECK(referenceResponse.status == 200);
  CHECK(referenceResponse.contentLength ==
        Bloodborne::ReferenceServerStatusInfoSize);
  CHECK(referenceResponse.body.size() ==
        Bloodborne::ReferenceServerStatusInfoSize);
  CHECK(referenceResponse.body.startsWith("PHNzPjA8L3"));
  CHECK(QCryptographicHash::hash(referenceResponse.body,
                                 QCryptographicHash::Sha256)
            .toHex()
            .toUpper() == Bloodborne::ReferenceServerStatusInfoSha256);

  const QString baseUrl = QStringLiteral("http://73.244.12.22:31315");
  const QByteArray ssInfo = Bloodborne::BuildServerStatusInfo(baseUrl + '/');
  CHECK(ssInfo.startsWith("<ss>0</ss>"));
  CHECK(ssInfo.left(10) == QByteArray::fromHex("3c73733e303c2f73733e"));
  CHECK(ssInfo.front() == '<');
  CHECK(ssInfo.contains("<gameurl2>"));
  CHECK(ssInfo.count("<api_") == 37);
  CHECK(!ssInfo.contains("gameurl3"));
  CHECK(!ssInfo.contains("thehuntersdream.com"));
  CHECK(!ssInfo.contains("scej-network.jp"));
  CHECK(QByteArray(Bloodborne::ServerStatusInfoContentType)
            .startsWith("text/plain"));

  for (const Bloodborne::BootstrapApi &api : Bloodborne::BootstrapApis()) {
    const QByteArray opening = QByteArray("<") + api.name + ">";
    const QByteArray closing = QByteArray("</") + api.name + ">";
    const QByteArray expected = opening + baseUrl.toUtf8() + closing;
    CHECK(ssInfo.contains(expected));
    CHECK(ssInfo.count(opening) == 1);

    const qsizetype valueBegin = ssInfo.indexOf(opening) + opening.size();
    const qsizetype valueEnd = ssInfo.indexOf(closing, valueBegin);
    CHECK(valueEnd >= valueBegin);
    CHECK(ssInfo.mid(valueBegin, valueEnd - valueBegin) == baseUrl.toUtf8());
  }
  CHECK(ssInfo.contains("<api_Login>http://73.244.12.22:31315</api_Login>"));
  CHECK(ssInfo.contains("<api_SummonDataCreate>http://73.244.12.22:31315</"
                        "api_SummonDataCreate>"));
  CHECK(!ssInfo.contains("/basic_utils/"));
  CHECK(!ssInfo.contains("/blood_messenger/"));
  CHECK(!ssInfo.contains("/summon_messenger/"));
  CHECK(!ssInfo.contains("/channel/"));
  CHECK(!ssInfo.contains("/tomb_messenger/"));
  CHECK(!ssInfo.contains("/wandering_ghost/"));

  CHECK(ssInfo.contains("<ReloadServerStatusInfoInterval2>300</"
                        "ReloadServerStatusInfoInterval2>"));
  CHECK(ssInfo.contains(
      "<SummonDataCreateInterval2>30</SummonDataCreateInterval2>"));
  CHECK(ssInfo.contains(
      "<SummonDataGetListInterval2>35</SummonDataGetListInterval2>"));
  CHECK(ssInfo.contains("<SummonDataGetListGetMaxCount2>20</"
                        "SummonDataGetListGetMaxCount2>"));
  CHECK(ssInfo.contains("<SummonDataCoopMatchingLevelLowerAbs2>-20</"
                        "SummonDataCoopMatchingLevelLowerAbs2>"));
  CHECK(ssInfo.contains("<NoticeEmergencyGetInterval2>60</"
                        "NoticeEmergencyGetInterval2>"));
  CHECK(ssInfo.contains("<PlayLog2>1</PlayLog2>"));
  CHECK(ssInfo.endsWith(
      "<Playlog_Guest_StartMultiPlay2>1</Playlog_Guest_StartMultiPlay2>\n"));

  const QJsonObject login =
      Bloodborne::BuildLoginResponse(42, 4, QStringLiteral("session-42"));
  CHECK(login.value(QStringLiteral("ResKind")).toInt(-1) == 0);
  CHECK(login.value(QStringLiteral("UserId")).toVariant().toLongLong() == 42);
  CHECK(login.value(QStringLiteral("LanguageId")).toInt() == 4);
  CHECK(login.value(QStringLiteral("SessionId")).toString() ==
        QStringLiteral("session-42"));
  CHECK(login.value(QStringLiteral("ServerVersion")).toInt() == 6);

  const QJsonObject serverTime = Bloodborne::BuildServerTimeResponse();
  CHECK(serverTime.size() == 1);
  CHECK(serverTime.value(QStringLiteral("ResKind")).toInt(-1) == 0);

  const QJsonObject normalNotice = Bloodborne::BuildNoticeNormalResponse();
  CHECK(normalNotice.value(QStringLiteral("ResKind")).toInt(-1) == 0);
  CHECK(normalNotice.value(QStringLiteral("NoticeList")).toArray().isEmpty());

  const QJsonObject emergencyNotice = Bloodborne::BuildNoticeEmergencyResponse(
      QStringLiteral("2026-08-22T12:34:56"));
  CHECK(emergencyNotice.value(QStringLiteral("ResKind")).toInt(-1) == 0);
  CHECK(emergencyNotice.value(QStringLiteral("CheckTime")).toString() ==
        QStringLiteral("2026-08-22T12:34:56"));
  CHECK(
      emergencyNotice.value(QStringLiteral("NoticeList")).toArray().isEmpty());

  const QJsonObject sync = Bloodborne::BuildSyncCharaIdResponse({1001, 1002});
  const QJsonArray publishIds =
      sync.value(QStringLiteral("PublishCharacterIdList")).toArray();
  CHECK(publishIds.size() == 2);
  CHECK(publishIds.at(0)
            .toObject()
            .value(QStringLiteral("PublishCharaId"))
            .toVariant()
            .toLongLong() == 1001);

  QTemporaryDir directory;
  CHECK(directory.isValid());
  Database db(QStringLiteral("bloodborne_bootstrap_test"));
  CHECK(db.Open(directory.filePath(QStringLiteral("test.db"))));
  CHECK(InsertAccount(db, QStringLiteral("HunterOne"),
                      QStringLiteral("one@example.test"),
                      QStringLiteral("token-one")));
  CHECK(InsertAccount(db, QStringLiteral("HunterTwo"),
                      QStringLiteral("two@example.test"),
                      QStringLiteral("token-two")));

  const QList<qint64> first = db.GetOrCreateBloodborneCharaIds(1, 1);
  const QList<qint64> firstAgain = db.GetOrCreateBloodborneCharaIds(1, 1);
  const QList<qint64> second = db.GetOrCreateBloodborneCharaIds(2, 1);
  CHECK(first.size() == 1);
  CHECK(firstAgain == first);
  CHECK(second.size() == 1);
  CHECK(second.front() != first.front());

  return 0;
}
