// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include <iostream>

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "bloodborne_chalice_service.h"
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

QJsonObject IdList(qint64 found, qint64 missing = 0) {
  QJsonArray list;
  list.append(QJsonObject{{QStringLiteral("ChannelId"), found}});
  if (missing > 0)
    list.append(QJsonObject{{QStringLiteral("ChannelId"), missing}});
  return QJsonObject{{QStringLiteral("ChannelIdList"), list}};
}

QJsonObject WordSearch(const QString &glyph) {
  return QJsonObject{
      {QStringLiteral("FormDataVersion"), 0},
      {QStringLiteral("SearchWord"), glyph},
      {QStringLiteral("UnlockedFlagList"),
       QJsonArray{QJsonObject{{QStringLiteral("UnlockedFlag"), 260}},
                  QJsonObject{{QStringLiteral("UnlockedFlag"), 278970752}},
                  QJsonObject{{QStringLiteral("UnlockedFlag"), 3491807617.0}}}},
  };
}

} // namespace

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);
  QTemporaryDir temporary;
  CHECK(temporary.isValid());

  Database db(QStringLiteral("bloodborne_chalice_service_test"));
  CHECK(db.Open(temporary.filePath(QStringLiteral("chalices.db"))));
  QSqlQuery migration(db.Conn());
  CHECK(migration.exec(
      QStringLiteral("SELECT COUNT(*) FROM migration WHERE migration_id=7 AND "
                     "description='Bloodborne Chalice Dungeons'")));
  CHECK(migration.next());
  CHECK(migration.value(0).toInt() == 1);

  Bloodborne::ChaliceService service(db);
  const QJsonObject capturedUpload =
      BloodborneTestFixtures::OfficialChannelUploadRequest();
  CHECK(!capturedUpload.isEmpty());
  const QString exactFormData =
      capturedUpload.value(QStringLiteral("FormData")).toString();
  const Bloodborne::OnlineResult first = service.Upload(2880, capturedUpload);
  CHECK(first.IsSuccess());
  CHECK(first.response.value(QStringLiteral("MessageId")).toString() ==
        QStringLiteral("ChannelUploadResponse"));
  CHECK(first.response.value(QStringLiteral("ResKind")).toInt(-1) == 0);
  const qint64 firstId =
      first.response.value(QStringLiteral("ChannelId")).toInteger();
  const QString firstGlyph =
      first.response.value(QStringLiteral("DiscernmentWord")).toString();
  CHECK(firstId > 0);
  CHECK(QRegularExpression(QStringLiteral("^[2-9a-km-np-z]{8}$"))
            .match(firstGlyph)
            .hasMatch());

  QSqlQuery stored(db.Conn());
  stored.prepare(QStringLiteral(
      "SELECT form_data,create_chara_id,create_user_id,share_level FROM "
      "bloodborne_chalice "
      "WHERE channel_id=?"));
  stored.addBindValue(firstId);
  CHECK(stored.exec());
  CHECK(stored.next());
  CHECK(stored.value(0).toString() == exactFormData);
  CHECK(stored.value(1).toString() == QStringLiteral("9223372036854776000"));
  CHECK(stored.value(2).toLongLong() == 2880);
  CHECK(stored.value(3).toInt() == 0);

  // A UNIQUE SQLite index backs the generator and independently created glyphs
  // differ.
  QSet<QString> glyphs{firstGlyph};
  for (int index = 0; index < 16; ++index) {
    const Bloodborne::OnlineResult uploaded =
        service.Upload(2880, capturedUpload);
    CHECK(uploaded.IsSuccess());
    const QString glyph =
        uploaded.response.value(QStringLiteral("DiscernmentWord")).toString();
    CHECK(!glyphs.contains(glyph));
    glyphs.insert(glyph);
  }
  QSqlQuery uniqueIndex(db.Conn());
  CHECK(uniqueIndex.exec(
      QStringLiteral("SELECT COUNT(*),COUNT(DISTINCT discernment_word) FROM "
                     "bloodborne_chalice")));
  CHECK(uniqueIndex.next());
  CHECK(uniqueIndex.value(0).toInt() == uniqueIndex.value(1).toInt());

  // The captured unshared glyph is intentionally invisible, even to its
  // creator.
  const Bloodborne::OnlineResult hidden =
      service.WordSearch(2880, WordSearch(firstGlyph));
  CHECK(hidden.IsSuccess());
  CHECK(!hidden.response.contains(QStringLiteral("ChannelId")));

  QJsonObject share{
      {QStringLiteral("ChannelId"), firstId},
      {QStringLiteral("CharaId"),
       capturedUpload.value(QStringLiteral("CharaId"))},
      {QStringLiteral("ShareLevel"), 2},
  };
  CHECK(service.Share(2880, share).IsSuccess());
  const Bloodborne::OnlineResult visible =
      service.WordSearch(777, WordSearch(firstGlyph));
  CHECK(visible.IsSuccess());
  CHECK(visible.response.value(QStringLiteral("ChannelId")).toInteger() ==
        firstId);
  CHECK(visible.response.value(QStringLiteral("FormData")).toString() ==
        exactFormData);
  CHECK(visible.response.value(QStringLiteral("ShareLevel")).toInt() == 2);

  // Sharing an unknown ID is a successful no-op, matching the captured
  // reference behavior.
  share.insert(QStringLiteral("ChannelId"), 9'999'999);
  CHECK(service.Share(2880, share).IsSuccess());
  QSqlQuery unknown(db.Conn());
  CHECK(unknown.exec(QStringLiteral(
      "SELECT COUNT(*) FROM bloodborne_chalice WHERE channel_id=9999999")));
  CHECK(unknown.next());
  CHECK(unknown.value(0).toInt() == 0);

  QJsonObject nullableSearch =
      BloodborneTestFixtures::OfficialNullableChannelSearchRequest();
  nullableSearch.insert(QStringLiteral("GetCount"), 1);
  const Bloodborne::OnlineResult oneResult =
      service.Search(2880, nullableSearch);
  CHECK(oneResult.IsSuccess());
  CHECK(oneResult.response.value(QStringLiteral("ChannelList"))
            .toArray()
            .size() == 1);

  QJsonObject filteredSearch = nullableSearch;
  filteredSearch.insert(
      QStringLiteral("FixedOrGeneralList"),
      QJsonArray{QJsonObject{{QStringLiteral("FixedOrGeneral"), 1}}});
  filteredSearch.insert(QStringLiteral("HolyGrailTypeId"), 0);
  filteredSearch.insert(QStringLiteral("RitualLevel"), 1);
  filteredSearch.insert(QStringLiteral("SubFeatureFlag"), 0);
  const Bloodborne::OnlineResult filtered =
      service.Search(2880, filteredSearch);
  CHECK(filtered.IsSuccess());
  CHECK(
      filtered.response.value(QStringLiteral("ChannelList")).toArray().size() ==
      1);
  filteredSearch.insert(QStringLiteral("RitualLevel"), 5);
  CHECK(service.Search(2880, filteredSearch)
            .response.value(QStringLiteral("ChannelList"))
            .toArray()
            .isEmpty());

  const Bloodborne::OnlineResult info =
      service.GetInfo(2880, IdList(firstId, 8'888'888));
  CHECK(info.IsSuccess());
  CHECK(
      info.response.value(QStringLiteral("ChannelInfoList")).toArray().size() ==
      1);
  CHECK(info.response.value(QStringLiteral("LostChannelIdList"))
            .toArray()
            .size() == 1);
  const QJsonObject infoItem =
      info.response.value(QStringLiteral("ChannelInfoList"))
          .toArray()
          .at(0)
          .toObject();
  CHECK(
      infoItem.keys() ==
      QStringList({QStringLiteral("ChannelId"), QStringLiteral("ShareLevel"),
                   QStringLiteral("Status"), QStringLiteral("TurnoutLevel")}));

  const Bloodborne::OnlineResult details =
      service.GetDetailsInfo(2880, IdList(firstId, 8'888'888));
  CHECK(details.IsSuccess());
  CHECK(
      details.response.value(QStringLiteral("ChannelList")).toArray().size() ==
      1);
  CHECK(details.response.value(QStringLiteral("LostChannelIdList"))
            .toArray()
            .size() == 1);
  CHECK(details.response.value(QStringLiteral("ChannelList"))
            .toArray()
            .at(0)
            .toObject()
            .value(QStringLiteral("CreateCharaId"))
            .toDouble() ==
        capturedUpload.value(QStringLiteral("CharaId")).toDouble());

  // Two compatible public Chalices rotate through Quick Search rather than
  // sticking to one.
  QJsonObject quickUpload = capturedUpload;
  quickUpload.insert(QStringLiteral("RitualLevel"), 5);
  quickUpload.insert(QStringLiteral("ShareLevel"), 2);
  const Bloodborne::OnlineResult quickOne = service.Upload(2880, quickUpload);
  const Bloodborne::OnlineResult quickTwo = service.Upload(2880, quickUpload);
  CHECK(quickOne.IsSuccess());
  CHECK(quickTwo.IsSuccess());
  const QJsonObject randomRequest =
      BloodborneTestFixtures::OfficialChannelRandomJoinRequest();
  const Bloodborne::OnlineResult joinedOne =
      service.RandomJoin(999, randomRequest);
  const Bloodborne::OnlineResult joinedTwo =
      service.RandomJoin(999, randomRequest);
  CHECK(joinedOne.IsSuccess());
  CHECK(joinedTwo.IsSuccess());
  CHECK(joinedOne.response.value(QStringLiteral("MessageId")).toString() ==
        QStringLiteral("ChannelRandomJoinResponse"));
  CHECK(joinedOne.response.value(QStringLiteral("ChannelId")).toInteger() > 0);
  CHECK(joinedOne.response.value(QStringLiteral("ChannelId")).toInteger() !=
        joinedTwo.response.value(QStringLiteral("ChannelId")).toInteger());

  return 0;
}
