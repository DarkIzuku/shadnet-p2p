// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include <iostream>

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "bloodborne_online_service.h"
#include "bloodborne_test_fixtures.h"
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

constexpr char ShellData[] = "AQAAAAAAAAA=";
constexpr char BloodData[] = "gJ/VAKRLTABAQg8AQEIPAP////////////////////"
                             "+guw0ASO4AADDyAAAY9gAA///////E2+EAAB2cnJwA"
                             "AEl6dWt1AAAAAAAAAAAAAAC3h4uaAAAAwAAAARhqCQAA/////"
                             "5CtAwAAAAAAAAAAANy4TsPtfgXCOxdFQjwY"
                             "ND8RJzF1AAAAAAAAAMMAAAAAAABFQgAAgD8=";
constexpr char TombData[] = "SXp1a3UAAAAAAAAAAAAAALeHi5oCAADAAAABGI/"
                            "CTsOkcAXCmZk7QtsPSb4AAAAAAAApemQAAABYAwQA";
constexpr char DeathVisionData[] =
    "AIKloc4NAAAWAgAAeNrd1c9LG0EUB/"
    "DvzCYaGovak+C1eAhSigEvQiKaaCUl5NiLpDSxlIgoQVFKDSn0KOI"
    "pepAqVqQIFQ/Vg4KlofTQaxH/"
    "AQ89efAQYpDo281Adpf8mF0XAh14PHYD83lv2HnpRWUpFN2ors8X8XJ0A"
    "ks+luSvefX9vMgxkadETor8RuSEyBGR50S+o8VFhimrNSge4BvZfeOtsY8uEuW1MDI+"
    "9oXv6OxYHduJmjT"
    "by/C9Yv/ysU2+UcOeMj3rjZvU5U/7fTOsk30YRr4VfQt7yU7fPBMceWjfXzW7dt8y/eSV/"
    "YB576Z2m94u"
    "Md3Vs2QD2aBVmzewrZieGns3td3472yPtB0vH4dUu99geylcFF1iDlu5Q1J2O0OO7GQIJdmZan"
    "7W1+gy1T"
    "i7+anuHVQ6GFbJfqrZxr5fUfj9/"
    "qC6NxN7cwfnOafDWSH7ZNRZu5jeCsjaf0dRsHvmtWp0SdSonnmW7N9h"
    "3DabqU7MccNce8SwTPbBGDrs9v0Q+"
    "4Pat2ZP8xS3O1OtzxZ1njthR2Nvg3pTzoZmn2l23PCdP6H4M2mvf0"
    "VytiySPdjAtmPK/Y/"
    "RN0X2VdgZGxbtNNl7LbJnyH7msA1J+53ad8hZ22PBPg615sz1K7t1jt3ISwyPdGpx"
    "Z1rbp4/x4gp4fg30FGD4/Z9S2WO7OOBW+8bH7NBGLhegXX9UAxj2GskJvMcCpinqrXum";
constexpr char GhostData[] = "SXp1a3UAAAAAAAAAAAAAALeHi5oBAADAAAABGMRuRMOeKuXBF"
                             "fCBQigAW94AAAAAzg0AAMEBAAB42pNmgAB"
                             "mIBZkQICF1+P/8fkyRGkx/mJEEmZIgdJJUDoZSntD6QIw2WD/"
                             "Hwg4gCwQzYBGM+EQB7mBiZW6djOj0YTsX"
                             "gO0O82XQUGLMYUpgQlhdwmUDoDSqTj4xLkR025mLkaG+9fT/"
                             "931Y1Ah1m5S7MLnb2ZgYEneKP6XE8CgpMV"
                             "ox2jLSE+7GRnCbtT/iw8ChTmq3aTYgZ7WOIixGxjf02/0/"
                             "5sbwiCnxWjFaEklu5mJspuR4dmN2f9iwqlrN"
                             "wOR/ja/OfufSBQozKlnt6D804PE2N0MtPtIDIPMQPh7C9DuO/"
                             "EMbJTYjV6m5DIUE2X3RqDdS5IZvtE/fzM"
                             "yLALaXZMKsnsO0wwku9HtLKFBubYYaHdLKsMXLcbFTIuw2E2M"
                             "2QY83+1Ijm82ZLtR65IAMuoUWHyXMuSh2"
                             "I0t/"
                             "zMh7H6ixVhPRj2GajbMbiMGCyLqMerazUBiHUoLuxkG0O6B9D"
                             "fHCAlzZjLba7TIY8wjNMxHSh4jN60"
                             "hg4b5VxmWePswODjxg/F/NLBgNy+DxztgvfWJgUHiGwOK/"
                             "AuggSC7FvwwYgWFA0Njg/WMadPsgKbuh2GQn"
                             "Q7cqFYyEAQAl+SgKg==";

bool InsertAccount(Database &db, qint64 userId, const QString &name) {
  QSqlQuery query(db.Conn());
  query.prepare(QStringLiteral("INSERT INTO account(user_id, username, hash, "
                               "salt, avatar_url, email, email_check, "
                               "token, admin, stat_agent, banned) "
                               "VALUES(?,?,X'01',X'02','',?,?,?,0,0,0)"));
  query.addBindValue(userId);
  query.addBindValue(name);
  query.addBindValue(name + QStringLiteral("@example.test"));
  query.addBindValue(name + QStringLiteral("@example.test"));
  query.addBindValue(name + QStringLiteral("-token"));
  return query.exec();
}

QJsonObject ShellRequest(double charaId) {
  QJsonObject request;
  request.insert(QStringLiteral("CharaId"), charaId);
  request.insert(QStringLiteral("MessageId"),
                 QStringLiteral("MessengerShellUploadRequest"));
  request.insert(QStringLiteral("SessionId"), QStringLiteral("test-session"));
  request.insert(QStringLiteral("ShellData"), QString::fromLatin1(ShellData));
  request.insert(QStringLiteral("ShellDataVersion"), 2);
  return request;
}

QJsonObject Area(qint64 areaId, qint64 regionId, int count = 30) {
  QJsonObject area;
  area.insert(QStringLiteral("AreaId"), areaId);
  area.insert(QStringLiteral("AreaRegionId"), regionId);
  area.insert(QStringLiteral("ChannelId"), 0);
  area.insert(QStringLiteral("GetCount"), count);
  return area;
}

QJsonObject BloodCreateRequest() {
  QJsonObject item;
  item.insert(QStringLiteral("AreaId"), 402718720);
  item.insert(QStringLiteral("AreaRegionId"), 241040);
  item.insert(QStringLiteral("BloodData"), QString::fromLatin1(BloodData));
  item.insert(QStringLiteral("BloodDataVersion"), 5);
  item.insert(QStringLiteral("ChannelId"), 0);
  item.insert(QStringLiteral("CharaData"), true);
  item.insert(QStringLiteral("CharaDataVersion"), 1);
  item.insert(QStringLiteral("EvaluateMinus"), 0);
  item.insert(QStringLiteral("EvaluatePlus"), 0);
  item.insert(QStringLiteral("PrevBloodMessId"), 1234);

  QJsonObject request;
  request.insert(QStringLiteral("BloodMessList"), QJsonArray{item});
  request.insert(QStringLiteral("CharaId"), 101);
  request.insert(QStringLiteral("MessageId"),
                 QStringLiteral("BloodMessCreateRequest"));
  return request;
}

QJsonObject BloodGetRequest(qint64 areaId, qint64 regionId) {
  QJsonObject request;
  request.insert(QStringLiteral("AreaInfoList"),
                 QJsonArray{Area(areaId, regionId)});
  request.insert(QStringLiteral("BloodDataVersion"), 5);
  request.insert(QStringLiteral("CharaDataVersion"), 1);
  request.insert(QStringLiteral("GetMaxCount"), 100);
  request.insert(QStringLiteral("MessShellInfoVersion"), 2);
  request.insert(QStringLiteral("MessageId"),
                 QStringLiteral("BloodMessGetListRequest"));
  return request;
}

QJsonObject TombCreateRequest() {
  QJsonObject request;
  request.insert(QStringLiteral("AreaId"), 402718720);
  request.insert(QStringLiteral("AreaRegionId"), 241040);
  request.insert(QStringLiteral("ChannelId"), 0);
  request.insert(QStringLiteral("CharaId"), 101);
  request.insert(QStringLiteral("DeathVisionData"),
                 QString::fromLatin1(DeathVisionData));
  request.insert(QStringLiteral("DeathVisionDataVersion"), 4);
  request.insert(QStringLiteral("MessageId"),
                 QStringLiteral("TombMessCreateRequest"));
  request.insert(QStringLiteral("TombData"), QString::fromLatin1(TombData));
  request.insert(QStringLiteral("TombDataVersion"), 4);
  return request;
}

QJsonObject TombGetRequest(qint64 areaId, qint64 regionId) {
  QJsonObject request;
  request.insert(QStringLiteral("AreaInfoList"),
                 QJsonArray{Area(areaId, regionId)});
  request.insert(QStringLiteral("GetMaxCount"), 100);
  request.insert(QStringLiteral("MessShellInfoVersion"), 2);
  request.insert(QStringLiteral("MessageId"),
                 QStringLiteral("TombMessGetListRequest"));
  request.insert(QStringLiteral("TombDataVersion"), 4);
  return request;
}

QJsonObject GhostCreateRequest() {
  QJsonObject request;
  request.insert(QStringLiteral("AreaId"), 402718720);
  request.insert(QStringLiteral("AreaRegionId"), 241040);
  request.insert(QStringLiteral("ChannelId"), 0);
  request.insert(QStringLiteral("CharaId"), 101);
  request.insert(QStringLiteral("MatchingLevel"), 17);
  request.insert(QStringLiteral("MessageId"),
                 QStringLiteral("WanderingGhostCreateRequest"));
  request.insert(QStringLiteral("RejectIgnore"), 0);
  request.insert(QStringLiteral("WanderingGhostData"),
                 QString::fromLatin1(GhostData));
  request.insert(QStringLiteral("WanderingGhostDataVersion"), 5);
  return request;
}

QJsonObject GhostGetRequest(qint64 areaId, qint64 regionId) {
  QJsonObject request;
  request.insert(QStringLiteral("AreaList"),
                 QJsonArray{Area(areaId, regionId, 5)});
  request.insert(QStringLiteral("GetMaxCount"), 10);
  request.insert(QStringLiteral("JoinedCharaIdList"), QJsonArray{});
  request.insert(QStringLiteral("MatchingLevel"), 17);
  request.insert(QStringLiteral("MessageId"),
                 QStringLiteral("WanderingGhostGetRequest"));
  request.insert(QStringLiteral("WanderingGhostDataVersion"), 5);
  return request;
}

bool HasExactKeys(const QJsonObject &object,
                  std::initializer_list<const char *> names) {
  QSet<QString> expected;
  for (const char *name : names)
    expected.insert(QLatin1String(name));
  const QStringList keys = object.keys();
  return QSet<QString>(keys.begin(), keys.end()) == expected;
}

} // namespace

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);
  QTemporaryDir directory;
  CHECK(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("online.db"));

  qint64 bloodMessageId = 0;
  qint64 tombMessageId = 0;
  qint64 ghostId = 0;

  {
    Database db(QStringLiteral("bloodborne_online_create"));
    CHECK(db.Open(path));
    CHECK(InsertAccount(db, 1, QStringLiteral("player-one")));
    CHECK(InsertAccount(db, 2, QStringLiteral("player-two")));
    CHECK(InsertAccount(db, 3, QStringLiteral("player-three")));
    Bloodborne::OnlineService service(db, 3600);

    CHECK(service.UploadMessengerShell(1, ShellRequest(101)).IsSuccess());
    CHECK(service.UploadMessengerShell(2, ShellRequest(202)).IsSuccess());
    CHECK(service.UploadMessengerShell(3, ShellRequest(303)).IsSuccess());

    const Bloodborne::OnlineResult blood =
        service.CreateBloodMessages(1, BloodCreateRequest());
    CHECK(blood.IsSuccess());
    CHECK(HasExactKeys(blood.response,
                       {"BloodMessIdList", "MessageId", "ResKind"}));
    CHECK(blood.response.value(QStringLiteral("MessageId")).toString() ==
          QStringLiteral("BloodMessCreateResponse"));
    const QJsonObject createdBlood =
        blood.response.value(QStringLiteral("BloodMessIdList"))
            .toArray()
            .at(0)
            .toObject();
    bloodMessageId = createdBlood.value(QStringLiteral("BloodMessId"))
                         .toVariant()
                         .toLongLong();
    CHECK(bloodMessageId > 0);
    CHECK(createdBlood.value(QStringLiteral("PrevBloodMessId")).toInt() ==
          1234);

    QJsonObject evaluation;
    evaluation.insert(QStringLiteral("BloodMessId"), bloodMessageId);
    evaluation.insert(QStringLiteral("EvaluateKind"), 1);
    const Bloodborne::OnlineResult firstEvaluation =
        service.EvaluateBloodMessage(2, evaluation);
    CHECK(firstEvaluation.IsSuccess());
    CHECK(HasExactKeys(firstEvaluation.response,
                       {"BloodMessList", "MessageId", "ResKind"}));
    CHECK(firstEvaluation.response.value(QStringLiteral("MessageId"))
              .toString() == QStringLiteral("BloodMessEvaluateResponse"));
    CHECK(firstEvaluation.response.value(QStringLiteral("BloodMessList"))
              .toArray()
              .at(0)
              .toObject()
              .value(QStringLiteral("EvaluatePlus"))
              .toInt() == 1);
    const Bloodborne::OnlineResult duplicateEvaluation =
        service.EvaluateBloodMessage(2, evaluation);
    CHECK(duplicateEvaluation.IsSuccess());
    CHECK(duplicateEvaluation.response.value(QStringLiteral("BloodMessList"))
              .toArray()
              .at(0)
              .toObject()
              .value(QStringLiteral("EvaluatePlus"))
              .toInt() == 1);

    const Bloodborne::OnlineResult tomb =
        service.CreateTombMessage(1, TombCreateRequest());
    CHECK(tomb.IsSuccess());
    CHECK(HasExactKeys(tomb.response, {"MessageId", "ResKind", "TombMessId"}));
    tombMessageId = tomb.response.value(QStringLiteral("TombMessId"))
                        .toVariant()
                        .toLongLong();
    CHECK(tombMessageId > 0);

    const Bloodborne::OnlineResult ghost =
        service.CreateWanderingGhost(1, GhostCreateRequest());
    CHECK(ghost.IsSuccess());
    CHECK(HasExactKeys(ghost.response,
                       {"MessageId", "ResKind", "WanderingGhostId"}));
    ghostId = ghost.response.value(QStringLiteral("WanderingGhostId"))
                  .toVariant()
                  .toLongLong();
    CHECK(ghostId > 0);

    QJsonObject invalidGhost = GhostCreateRequest();
    invalidGhost.insert(QStringLiteral("WanderingGhostData"),
                        QStringLiteral("not-base64"));
    CHECK(service.CreateWanderingGhost(1, invalidGhost).error ==
          Bloodborne::OnlineError::InvalidRequest);
  }

  {
    Database db(QStringLiteral("bloodborne_online_reopen"));
    CHECK(db.Open(path));
    Bloodborne::OnlineService service(db, 3600);

    CHECK(service.GetBloodMessages(1, BloodGetRequest(402718720, 241040))
              .response.value(QStringLiteral("BloodMessList"))
              .toArray()
              .isEmpty());
    const QJsonArray bloodMessages =
        service.GetBloodMessages(2, BloodGetRequest(402718720, 241040))
            .response.value(QStringLiteral("BloodMessList"))
            .toArray();
    CHECK(bloodMessages.size() == 1);
    const QJsonObject bloodItem = bloodMessages.at(0).toObject();
    CHECK(
        HasExactKeys(bloodItem, {"BloodData", "BloodDataVersion", "BloodMessId",
                                 "EvaluateMinus", "EvaluatePlus",
                                 "MessShellInfo", "MessShellInfoVersion"}));
    CHECK(bloodItem.value(QStringLiteral("BloodData")).toString() ==
          QString::fromLatin1(BloodData));
    CHECK(QByteArray::fromBase64(bloodItem.value(QStringLiteral("BloodData"))
                                     .toString()
                                     .toLatin1()) ==
          QByteArray::fromBase64(BloodData));
    CHECK(bloodItem.value(QStringLiteral("MessShellInfo")).toString() ==
          QString::fromLatin1(ShellData));
    CHECK(service.GetBloodMessages(3, BloodGetRequest(352321536, 210000))
              .response.value(QStringLiteral("BloodMessList"))
              .toArray()
              .isEmpty());

    QJsonObject evaluationRequest;
    evaluationRequest.insert(QStringLiteral("BloodMessIdList"),
                             QJsonArray{bloodMessageId});
    const QJsonArray evaluations =
        service.GetBloodEvaluations(1, evaluationRequest)
            .response.value(QStringLiteral("BloodMessEvaluationList"))
            .toArray();
    CHECK(evaluations.size() == 1);
    CHECK(HasExactKeys(evaluations.at(0).toObject(),
                       {"BloodMessId", "EvaluateMinus", "EvaluatePlus"}));
    CHECK(evaluations.at(0)
              .toObject()
              .value(QStringLiteral("EvaluatePlus"))
              .toInt() == 1);

    QJsonObject searchRequest;
    searchRequest.insert(QStringLiteral("BloodMessIdList"),
                         QJsonArray{bloodMessageId, 999999});
    searchRequest.insert(QStringLiteral("CharaId"), 101);
    const Bloodborne::OnlineResult search =
        service.SearchBloodMessages(1, searchRequest);
    CHECK(search.IsSuccess());
    CHECK(search.response.value(QStringLiteral("BloodMessEvaluationList"))
              .toArray()
              .size() == 1);
    CHECK(search.response.value(QStringLiteral("LostBloodMessIdList"))
              .toArray()
              .at(0)
              .toInt() == 999999);

    CHECK(service.GetTombMessages(1, TombGetRequest(402718720, 241040))
              .response.value(QStringLiteral("TombMessList"))
              .toArray()
              .isEmpty());
    const QJsonArray tombs =
        service.GetTombMessages(2, TombGetRequest(402718720, 241040))
            .response.value(QStringLiteral("TombMessList"))
            .toArray();
    CHECK(tombs.size() == 1);
    CHECK(HasExactKeys(tombs.at(0).toObject(),
                       {"MessShellInfo", "TombData", "TombMessId"}));
    CHECK(tombs.at(0).toObject().value(QStringLiteral("TombData")).toString() ==
          QString::fromLatin1(TombData));
    CHECK(service.GetTombMessages(3, TombGetRequest(352321536, 210000))
              .response.value(QStringLiteral("TombMessList"))
              .toArray()
              .isEmpty());

    QJsonObject deathVisionRequest;
    deathVisionRequest.insert(QStringLiteral("DeathVisionDataVersion"), 4);
    deathVisionRequest.insert(QStringLiteral("TombMessId"), tombMessageId);
    const Bloodborne::OnlineResult vision =
        service.GetDeathVision(2, deathVisionRequest);
    CHECK(vision.IsSuccess());
    CHECK(HasExactKeys(
        vision.response,
        {"DeathVisionData", "DeathVisionDataVersion", "MessageId", "ResKind"}));
    CHECK(vision.response.value(QStringLiteral("DeathVisionData")).toString() ==
          QString::fromLatin1(DeathVisionData));
    CHECK(QByteArray::fromBase64(
              vision.response.value(QStringLiteral("DeathVisionData"))
                  .toString()
                  .toLatin1()) == QByteArray::fromBase64(DeathVisionData));

    CHECK(service.GetWanderingGhosts(1, GhostGetRequest(402718720, 241040))
              .response.value(QStringLiteral("WanderingGhostList"))
              .toArray()
              .isEmpty());

    const QJsonObject officialGhostRequest =
        BloodborneTestFixtures::OfficialWanderingGhostGetRequest();
    CHECK(!officialGhostRequest.isEmpty());
    const Bloodborne::OnlineResult officialGhostResult =
        service.GetWanderingGhosts(2, officialGhostRequest);
    CHECK(officialGhostResult.IsSuccess());
    CHECK(officialGhostResult.response.value(QStringLiteral("MessageId"))
              .toString() == QStringLiteral("WanderingGhostGetResponse"));

    QJsonObject invalidAreaRequest = officialGhostRequest;
    QJsonObject invalidArea =
        invalidAreaRequest.value(QStringLiteral("AreaList"))
            .toArray()
            .at(0)
            .toObject();
    invalidArea.remove(QStringLiteral("ChannelId"));
    invalidAreaRequest.insert(QStringLiteral("AreaList"),
                              QJsonArray{invalidArea});
    const Bloodborne::OnlineResult invalidAreaResult =
        service.GetWanderingGhosts(2, invalidAreaRequest);
    CHECK(invalidAreaResult.error == Bloodborne::OnlineError::InvalidRequest);
    CHECK(invalidAreaResult.detail.contains(
        QStringLiteral("field=AreaList[0].ChannelId")));
    CHECK(invalidAreaResult.detail.contains(QStringLiteral("value=<missing>")));

    QJsonObject invalidJoinedRequest = officialGhostRequest;
    invalidJoinedRequest.insert(QStringLiteral("JoinedCharaIdList"),
                                QJsonArray{QStringLiteral("not-a-number")});
    const Bloodborne::OnlineResult invalidJoinedResult =
        service.GetWanderingGhosts(2, invalidJoinedRequest);
    CHECK(invalidJoinedResult.error == Bloodborne::OnlineError::InvalidRequest);
    CHECK(invalidJoinedResult.detail.contains(
        QStringLiteral("field=JoinedCharaIdList[0]")));

    QJsonObject invalidMaximumRequest = officialGhostRequest;
    invalidMaximumRequest.insert(QStringLiteral("GetMaxCount"), 101);
    const Bloodborne::OnlineResult invalidMaximumResult =
        service.GetWanderingGhosts(2, invalidMaximumRequest);
    CHECK(invalidMaximumResult.error ==
          Bloodborne::OnlineError::InvalidRequest);
    CHECK(invalidMaximumResult.detail.contains(
        QStringLiteral("field=GetMaxCount")));

    QJsonObject invalidVersionRequest = officialGhostRequest;
    invalidVersionRequest.remove(QStringLiteral("WanderingGhostDataVersion"));
    const Bloodborne::OnlineResult invalidVersionResult =
        service.GetWanderingGhosts(2, invalidVersionRequest);
    CHECK(invalidVersionResult.error ==
          Bloodborne::OnlineError::InvalidRequest);
    CHECK(invalidVersionResult.detail.contains(
        QStringLiteral("field=WanderingGhostDataVersion")));

    const QJsonArray ghosts =
        service.GetWanderingGhosts(2, GhostGetRequest(402718720, 241040))
            .response.value(QStringLiteral("WanderingGhostList"))
            .toArray();
    CHECK(ghosts.size() == 1);
    CHECK(HasExactKeys(ghosts.at(0).toObject(),
                       {"WanderingGhostData", "WanderingGhostDataVersion",
                        "WanderingGhostId"}));
    CHECK(ghosts.at(0)
              .toObject()
              .value(QStringLiteral("WanderingGhostId"))
              .toVariant()
              .toLongLong() == ghostId);
    CHECK(ghosts.at(0)
              .toObject()
              .value(QStringLiteral("WanderingGhostData"))
              .toString() == QString::fromLatin1(GhostData));

    QJsonObject wildcardGhostRequest = GhostGetRequest(4294967295LL, -1);
    wildcardGhostRequest.insert(QStringLiteral("MatchingLevel"), -1);
    CHECK(service.GetWanderingGhosts(2, wildcardGhostRequest)
              .response.value(QStringLiteral("WanderingGhostList"))
              .toArray()
              .size() == 1);
    wildcardGhostRequest.insert(QStringLiteral("JoinedCharaIdList"),
                                QJsonArray{101});
    CHECK(service.GetWanderingGhosts(2, wildcardGhostRequest)
              .response.value(QStringLiteral("WanderingGhostList"))
              .toArray()
              .isEmpty());
    CHECK(service.GetWanderingGhosts(3, GhostGetRequest(352321536, 210000))
              .response.value(QStringLiteral("WanderingGhostList"))
              .toArray()
              .isEmpty());

    QSqlQuery expireGhost(db.Conn());
    CHECK(expireGhost.exec(
        QStringLiteral("UPDATE bloodborne_wandering_ghost SET expires_at=0")));
    CHECK(service.GetWanderingGhosts(2, GhostGetRequest(402718720, 241040))
              .response.value(QStringLiteral("WanderingGhostList"))
              .toArray()
              .isEmpty());

    QJsonObject nonOwnerDelete;
    nonOwnerDelete.insert(QStringLiteral("BloodMessId"), bloodMessageId);
    CHECK(service.DeleteBloodMessage(2, nonOwnerDelete).error ==
          Bloodborne::OnlineError::NotFound);
    CHECK(service.DeleteBloodMessage(1, nonOwnerDelete).IsSuccess());
    CHECK(service.GetBloodMessages(2, BloodGetRequest(402718720, 241040))
              .response.value(QStringLiteral("BloodMessList"))
              .toArray()
              .isEmpty());
  }

  return 0;
}
