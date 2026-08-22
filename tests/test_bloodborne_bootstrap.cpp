// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include <cstdlib>
#include <iostream>

#include <QCoreApplication>
#include <QJsonArray>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "bloodborne_bootstrap.h"
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

} // namespace

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);

  const QString baseUrl = QStringLiteral("http://100.64.10.20:31315");
  const QByteArray ssInfo = Bloodborne::BuildServerStatusInfo(baseUrl + '/');
  CHECK(ssInfo.startsWith("<ss>0</ss>\n<gameurl2>\n"));
  CHECK(ssInfo.endsWith("</gameurl2>\n"));
  CHECK(ssInfo.count("<api_") == 37);
  CHECK(!ssInfo.contains("gameurl3"));
  CHECK(!ssInfo.contains("thehuntersdream.com"));
  CHECK(!ssInfo.contains("scej-network.jp"));

  for (const Bloodborne::BootstrapApi &api : Bloodborne::BootstrapApis()) {
    const QByteArray expected = QByteArray("<") + api.name + ">" +
                                baseUrl.toUtf8() + api.path + "</" + api.name +
                                ">";
    CHECK(ssInfo.contains(expected));
  }
  CHECK(ssInfo.contains("<api_SummonDataCreate>http://100.64.10.20:31315/"
                        "summon_messenger/create"));
  CHECK(ssInfo.contains(
      "<api_SummonDataGetList>http://100.64.10.20:31315/summon_messenger/get"));
  CHECK(ssInfo.contains("<api_SummonDataRemove>http://100.64.10.20:31315/"
                        "summon_messenger/delete"));
  CHECK(ssInfo.contains("<api_SummonDataSummon>http://100.64.10.20:31315/"
                        "summon_messenger/request"));

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
