// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include <iostream>

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QSqlDatabase>
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

  const QString idempotentPath =
      temporary.filePath(QStringLiteral("idempotent-chalices.db"));
  for (int pass = 0; pass < 2; ++pass) {
    Database seeded(QStringLiteral("bloodborne_fixed_idempotent_%1").arg(pass));
    CHECK(seeded.Open(idempotentPath));
    QSqlQuery count(seeded.Conn());
    CHECK(count.exec(QStringLiteral("SELECT COUNT(*) FROM bloodborne_chalice "
                                    "WHERE origin='vanilla_fixed'")));
    CHECK(count.next());
    CHECK(count.value(0).toInt() == 10);
    CHECK(
        count.exec(QStringLiteral("SELECT COUNT(*) FROM bloodborne_chalice")));
    CHECK(count.next());
    CHECK(count.value(0).toInt() == 10);
  }

  const QString legacyPath =
      temporary.filePath(QStringLiteral("legacy-v7-chalices.db"));
  {
    const QString connection = QStringLiteral("bloodborne_chalice_v7_seed");
    {
      QSqlDatabase legacy =
          QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
      legacy.setDatabaseName(legacyPath);
      CHECK(legacy.open());
      QSqlQuery create(legacy);
      CHECK(create.exec(QStringLiteral(
          "CREATE TABLE bloodborne_chalice("
          "channel_id INTEGER PRIMARY KEY AUTOINCREMENT,"
          "discernment_word TEXT NOT NULL COLLATE BINARY UNIQUE,"
          "create_user_id INTEGER NOT NULL,create_chara_id TEXT NOT NULL,"
          "create_date TEXT NOT NULL,last_play_date TEXT NOT NULL,"
          "fixed_or_general INTEGER NOT NULL,form_data TEXT NOT NULL,"
          "form_data_version INTEGER NOT NULL,holy_grail_type_id INTEGER NOT "
          "NULL,"
          "ritual_level INTEGER NOT NULL,share_level INTEGER NOT NULL,"
          "status INTEGER NOT NULL,sub_feature_flag INTEGER NOT NULL,"
          "turnout_level INTEGER NOT NULL DEFAULT 0,unlock_flag_list TEXT NOT "
          "NULL,"
          "wish_material_list TEXT NOT NULL,random_join_count INTEGER NOT NULL "
          "DEFAULT 0,"
          "map_data_json TEXT)")));
      CHECK(create.exec(QStringLiteral(
          "INSERT INTO "
          "bloodborne_chalice(channel_id,discernment_word,create_user_id,"
          "create_chara_id,create_date,last_play_date,fixed_or_general,form_"
          "data,"
          "form_data_version,holy_grail_type_id,ritual_level,share_level,"
          "status,"
          "sub_feature_flag,turnout_level,unlock_flag_list,wish_material_list) "
          "VALUES("
          "10,'23456789',77,'99','2026-08-28T00:00:00','2026-08-28T00:00:00',"
          "1,'LEGACY_FORM_DATA',0,0,1,2,1,0,0,'[]','[]')")));
      legacy.close();
    }
    QSqlDatabase::removeDatabase(connection);
  }
  {
    Database migrated(QStringLiteral("bloodborne_chalice_v7_migrated"));
    CHECK(migrated.Open(legacyPath));
    QSqlQuery preserved(migrated.Conn());
    CHECK(preserved.exec(QStringLiteral(
        "SELECT channel_id,form_data,origin FROM bloodborne_chalice "
        "WHERE discernment_word='23456789'")));
    CHECK(preserved.next());
    CHECK(preserved.value(0).toLongLong() > 10);
    CHECK(preserved.value(1).toString() == QStringLiteral("LEGACY_FORM_DATA"));
    CHECK(preserved.value(2).toString() == QStringLiteral("community"));
    CHECK(preserved.exec(
        QStringLiteral("SELECT discernment_word,origin FROM bloodborne_chalice "
                       "WHERE channel_id=10")));
    CHECK(preserved.next());
    CHECK(preserved.value(0).toString() == QStringLiteral("3n7q"));
    CHECK(preserved.value(1).toString() == QStringLiteral("vanilla_fixed"));
    CHECK(preserved.exec(QStringLiteral(
        "SELECT seq FROM sqlite_sequence WHERE name='bloodborne_chalice'")));
    CHECK(preserved.next());
    CHECK(preserved.value(0).toLongLong() > 10);
  }

  Database db(QStringLiteral("bloodborne_chalice_service_test"));
  CHECK(db.Open(temporary.filePath(QStringLiteral("chalices.db"))));
  QSqlQuery migration(db.Conn());
  CHECK(migration.exec(
      QStringLiteral("SELECT COUNT(*) FROM migration WHERE migration_id=7 AND "
                     "description='Bloodborne Chalice Dungeons'")));
  CHECK(migration.next());
  CHECK(migration.value(0).toInt() == 1);
  CHECK(migration.exec(
      QStringLiteral("SELECT COUNT(*) FROM migration WHERE migration_id=8 AND "
                     "description='Bloodborne fixed Chalice catalog'")));
  CHECK(migration.next());
  CHECK(migration.value(0).toInt() == 1);

  Bloodborne::ChaliceService service(db);
  for (const BloodborneTestFixtures::OfficialFixedChalice &expected :
       BloodborneTestFixtures::OfficialFixedChalices) {
    QJsonObject fixedSearch =
        BloodborneTestFixtures::OfficialFixedChannelSearchRequest();
    fixedSearch.insert(QStringLiteral("HolyGrailTypeId"),
                       expected.holyGrailTypeId);
    fixedSearch.insert(QStringLiteral("RitualLevel"), expected.ritualLevel);
    fixedSearch.insert(
        QStringLiteral("UnlockedFlagList"),
        QJsonArray{QJsonObject{{QStringLiteral("UnlockedFlag"), 0}},
                   QJsonObject{{QStringLiteral("UnlockedFlag"),
                                static_cast<qint64>(expected.unlockFlag)}},
                   QJsonObject{{QStringLiteral("UnlockedFlag"), 0}}});
    const Bloodborne::OnlineResult result = service.Search(1, fixedSearch);
    CHECK(result.IsSuccess());
    CHECK(result.response.value(QStringLiteral("MessageId")).toString() ==
          QStringLiteral("ChannelSearchResponse"));
    CHECK(result.response.value(QStringLiteral("ResKind")).toInt(-1) == 0);
    const QJsonArray channels =
        result.response.value(QStringLiteral("ChannelList")).toArray();
    CHECK(channels.size() == 1);
    const QJsonObject channel = channels.at(0).toObject();
    CHECK(channel.value(QStringLiteral("ChannelId")).toInteger() ==
          expected.channelId);
    CHECK(channel.value(QStringLiteral("DiscernmentWord")).toString() ==
          QLatin1String(expected.glyph));
    CHECK(channel.value(QStringLiteral("FixedOrGeneral")).toInt() == 2);
    CHECK(channel.value(QStringLiteral("HolyGrailTypeId")).toInt() ==
          expected.holyGrailTypeId);
    CHECK(channel.value(QStringLiteral("RitualLevel")).toInt() ==
          expected.ritualLevel);
    CHECK(channel.value(QStringLiteral("SubFeatureFlag")).toInt() == 256);
    CHECK(channel.value(QStringLiteral("FormData")).toString() ==
          QLatin1String(expected.formData));
    CHECK(QByteArray::fromBase64(
              channel.value(QStringLiteral("FormData")).toString().toLatin1())
              .size() == 140);
  }

  const QJsonObject depthOne =
      service
          .Search(1,
                  BloodborneTestFixtures::OfficialFixedChannelSearchRequest())
          .response.value(QStringLiteral("ChannelList"))
          .toArray()
          .at(0)
          .toObject();
  CHECK(depthOne.value(QStringLiteral("ChannelId")).toInteger() == 10);
  CHECK(depthOne.value(QStringLiteral("DiscernmentWord")).toString() ==
        QStringLiteral("3n7q"));

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
  CHECK(firstId > 10);
  CHECK(QRegularExpression(QStringLiteral("^[2-9a-km-np-z]{8}$"))
            .match(firstGlyph)
            .hasMatch());

  QSqlQuery stored(db.Conn());
  stored.prepare(QStringLiteral(
      "SELECT form_data,create_chara_id,create_user_id,share_level,origin FROM "
      "bloodborne_chalice "
      "WHERE channel_id=?"));
  stored.addBindValue(firstId);
  CHECK(stored.exec());
  CHECK(stored.next());
  CHECK(stored.value(0).toString() == exactFormData);
  CHECK(stored.value(1).toString() == QStringLiteral("9223372036854776000"));
  CHECK(stored.value(2).toLongLong() == 2880);
  CHECK(stored.value(3).toInt() == 0);
  CHECK(stored.value(4).toString() == QStringLiteral("community"));

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
  // Keep the captured nullable optionals while selecting the FixedOrGeneral
  // value of the synthetic uploaded record used by this test.
  nullableSearch.insert(
      QStringLiteral("FixedOrGeneralList"),
      QJsonArray{QJsonObject{{QStringLiteral("FixedOrGeneral"), 1}}});
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
