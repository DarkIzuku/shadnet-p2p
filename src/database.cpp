// SPDX-FileCopyrightText: Copyright 2019-2026 rpcsn Project
// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include <algorithm>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QSqlError>
#include <QSqlRecord>
#include <QUuid>
#include <qcryptographichash.h>
#include "bloodborne_chalice_fixed_catalog.h"
#include "database.h"

QByteArray Database::GenerateSalt(int bytes) {
    QByteArray salt(bytes, Qt::Uninitialized);
    QRandomGenerator* generator = QRandomGenerator::global();

    for (int i = 0; i < bytes; ++i) {
        salt[i] = static_cast<char>(generator->bounded(256));
    }

    return salt;
}
QString Database::GenerateToken(int len) {
    const QString chars = QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                         "abcdefghijklmnopqrstuvwxyz"
                                         "0123456789");
    QString tok;
    tok.reserve(len);

    QRandomGenerator* generator = QRandomGenerator::global();

    for (int i = 0; i < len; ++i) {
        tok.append(chars.at(generator->bounded(chars.size())));
    }

    return tok;
}

QByteArray Database::HashPassword(const QString& password, const QByteArray& salt) {
    int iterations = 100000;
    if (password.isEmpty() || salt.isEmpty()) {
        return QByteArray();
    }

    QByteArray passwordUtf8 = password.toUtf8();
    QByteArray hash = passwordUtf8 + salt;

    // Apply multiple iterations of SHA-256
    for (int i = 0; i < iterations; ++i) {
        hash = QCryptographicHash::hash(hash, QCryptographicHash::Sha256);
    }

    return hash;
}

Database::Database(const QString& connectionName)
    : m_connName(connectionName.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces)
                                          : connectionName) {}

Database::~Database() {
    if (m_db.isOpen())
        m_db.close();
    m_db = QSqlDatabase{};                    // release the reference first
    QSqlDatabase::removeDatabase(m_connName); // now safe to remove
}

bool Database::IsOpen() const {
    return m_db.isOpen();
}

bool Database::Open(const QString& path) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    m_db = QSqlDatabase::addDatabase("QSQLITE", m_connName);
    m_db.setDatabaseName(path);
    if (!m_db.open()) {
        m_lastError = m_db.lastError().text();
        return false;
    }
    Exec("PRAGMA journal_mode=WAL");
    Exec("PRAGMA foreign_keys=ON");
    return Migrate();
}

bool Database::Exec(const QString& sql) {
    QSqlQuery q(m_db);
    if (!q.exec(sql)) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}
bool Database::Exec(QSqlQuery& q) {
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

bool Database::Migrate() {
    Exec("CREATE TABLE IF NOT EXISTS migration("
         "  migration_id UNSIGNED INTEGER PRIMARY KEY,"
         "  description  TEXT NOT NULL)");

    // Migration 1: core tables
    QStringList stmts1 = {
        "CREATE TABLE IF NOT EXISTS account("
        "  user_id     INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  username    TEXT NOT NULL,"
        "  hash        BLOB NOT NULL,"
        "  salt        BLOB NOT NULL,"
        "  avatar_url  TEXT NOT NULL,"
        "  email       TEXT NOT NULL,"
        "  email_check TEXT NOT NULL UNIQUE,"
        "  token       TEXT NOT NULL,"
        "  reset_token TEXT,"
        "  admin       BOOL NOT NULL,"
        "  stat_agent  BOOL NOT NULL,"
        "  banned      BOOL NOT NULL,"
        "  UNIQUE(username COLLATE NOCASE))",

        "CREATE TABLE IF NOT EXISTS account_timestamp("
        "  user_id          UNSIGNED BIGINT NOT NULL PRIMARY KEY,"
        "  creation         UNSIGNED INTEGER NOT NULL,"
        "  last_login       UNSIGNED INTEGER,"
        "  token_last_sent  UNSIGNED INTEGER,"
        "  reset_emit       UNSIGNED INTEGER)",

        // Friendship table.
        // user_id_1 < user_id_2 is enforced by CHECK so there is exactly one row
        // per pair regardless of who initiated. status_user_1 and status_user_2
        // each hold a bitmask of FriendStatus flags for their respective user.
        "CREATE TABLE IF NOT EXISTS friendship("
        "  user_id_1     INTEGER NOT NULL REFERENCES account(user_id) ON DELETE CASCADE,"
        "  user_id_2     INTEGER NOT NULL REFERENCES account(user_id) ON DELETE CASCADE,"
        "  status_user_1 INTEGER NOT NULL DEFAULT 0,"
        "  status_user_2 INTEGER NOT NULL DEFAULT 0,"
        "  PRIMARY KEY(user_id_1, user_id_2),"
        "  CHECK(user_id_1 < user_id_2))",

        "CREATE INDEX IF NOT EXISTS friendship_user1 ON friendship(user_id_1)",
        "CREATE INDEX IF NOT EXISTS friendship_user2 ON friendship(user_id_2)",

        // Score leaderboard configuration it has one row per (comId, boardId) pair.
        "CREATE TABLE IF NOT EXISTS score_table("
        "  communication_id  TEXT    NOT NULL,"
        "  board_id          INTEGER NOT NULL,"
        "  rank_limit        INTEGER NOT NULL DEFAULT 100,"
        "  update_mode       INTEGER NOT NULL DEFAULT 0,"
        "  sort_mode         INTEGER NOT NULL DEFAULT 0,"
        "  upload_num_limit  INTEGER NOT NULL DEFAULT 10,"
        "  upload_size_limit INTEGER NOT NULL DEFAULT 6000000,"
        "  PRIMARY KEY(communication_id, board_id))",

        // Score rows it has one per (comId, boardId, userId, characterId).
        "CREATE TABLE IF NOT EXISTS score("
        "  communication_id TEXT    NOT NULL,"
        "  board_id         INTEGER NOT NULL,"
        "  user_id          INTEGER NOT NULL,"
        "  character_id     INTEGER NOT NULL,"
        "  score            INTEGER NOT NULL,"
        "  comment          TEXT,"
        "  game_info        BLOB,"
        "  data_id          INTEGER,"
        "  timestamp        INTEGER NOT NULL,"
        "  PRIMARY KEY(communication_id, board_id, user_id, character_id))",
    };

    for (const QString& s : stmts1)
        Exec(s);

    QSqlQuery ins(m_db);
    ins.prepare("INSERT OR IGNORE INTO migration VALUES(1,'Initial setup')");
    Exec(ins);

    QStringList stmts2 = {
        "CREATE TABLE IF NOT EXISTS title_name("
        "  communication_id TEXT NOT NULL PRIMARY KEY,"
        "  title_name       TEXT NOT NULL)",
    };

    for (const QString& s : stmts2)
        Exec(s);

    QSqlQuery ins2(m_db);
    ins2.prepare("INSERT OR IGNORE INTO migration VALUES(2,'title_name mapping')");
    Exec(ins2);

    QStringList stmts3 = {
        "CREATE TABLE IF NOT EXISTS bloodborne_character("
        "  chara_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  user_id  INTEGER NOT NULL REFERENCES account(user_id) ON DELETE CASCADE,"
        "  slot     INTEGER NOT NULL,"
        "  UNIQUE(user_id, slot))",
    };

    for (const QString& s : stmts3)
        Exec(s);

    QSqlQuery ins3(m_db);
    ins3.prepare("INSERT OR IGNORE INTO migration VALUES(3,'Bloodborne character IDs')");
    Exec(ins3);

    const QStringList stmts4 = {
        "CREATE TABLE IF NOT EXISTS bloodborne_messenger_shell("
        "  user_id INTEGER PRIMARY KEY REFERENCES account(user_id) ON DELETE CASCADE,"
        "  chara_id REAL NOT NULL,"
        "  shell_data TEXT NOT NULL,"
        "  shell_data_version INTEGER NOT NULL,"
        "  updated_at INTEGER NOT NULL)",

        "CREATE TABLE IF NOT EXISTS bloodborne_blood_message("
        "  blood_mess_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  owner_user_id INTEGER NOT NULL REFERENCES account(user_id) ON DELETE CASCADE,"
        "  owner_chara_id REAL NOT NULL,"
        "  area_id INTEGER NOT NULL,"
        "  area_region_id INTEGER NOT NULL,"
        "  channel_id INTEGER NOT NULL,"
        "  blood_data TEXT NOT NULL,"
        "  blood_data_version INTEGER NOT NULL,"
        "  chara_data INTEGER NOT NULL,"
        "  chara_data_version INTEGER NOT NULL,"
        "  base_evaluate_plus INTEGER NOT NULL DEFAULT 0,"
        "  base_evaluate_minus INTEGER NOT NULL DEFAULT 0,"
        "  prev_blood_mess_id REAL NOT NULL,"
        "  created_at INTEGER NOT NULL)",
        "CREATE INDEX IF NOT EXISTS bloodborne_blood_message_area "
        "ON bloodborne_blood_message(area_id, area_region_id, channel_id, created_at DESC)",

        "CREATE TABLE IF NOT EXISTS bloodborne_blood_message_evaluation("
        "  blood_mess_id INTEGER NOT NULL REFERENCES bloodborne_blood_message(blood_mess_id) "
        "    ON DELETE CASCADE,"
        "  user_id INTEGER NOT NULL REFERENCES account(user_id) ON DELETE CASCADE,"
        "  evaluate_kind INTEGER NOT NULL CHECK(evaluate_kind IN (-1, 1)),"
        "  created_at INTEGER NOT NULL,"
        "  PRIMARY KEY(blood_mess_id, user_id))",

        "CREATE TABLE IF NOT EXISTS bloodborne_tomb_message("
        "  tomb_mess_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  owner_user_id INTEGER NOT NULL REFERENCES account(user_id) ON DELETE CASCADE,"
        "  owner_chara_id REAL NOT NULL,"
        "  area_id INTEGER NOT NULL,"
        "  area_region_id INTEGER NOT NULL,"
        "  channel_id INTEGER NOT NULL,"
        "  tomb_data TEXT NOT NULL,"
        "  tomb_data_version INTEGER NOT NULL,"
        "  death_vision_data TEXT NOT NULL,"
        "  death_vision_data_version INTEGER NOT NULL,"
        "  created_at INTEGER NOT NULL)",
        "CREATE INDEX IF NOT EXISTS bloodborne_tomb_message_area "
        "ON bloodborne_tomb_message(area_id, area_region_id, channel_id, created_at DESC)",

        "CREATE TABLE IF NOT EXISTS bloodborne_wandering_ghost("
        "  wandering_ghost_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  owner_user_id INTEGER NOT NULL REFERENCES account(user_id) ON DELETE CASCADE,"
        "  owner_chara_id REAL NOT NULL,"
        "  area_id INTEGER NOT NULL,"
        "  area_region_id INTEGER NOT NULL,"
        "  channel_id INTEGER NOT NULL,"
        "  matching_level INTEGER NOT NULL,"
        "  reject_ignore INTEGER NOT NULL,"
        "  wandering_ghost_data TEXT NOT NULL,"
        "  wandering_ghost_data_version INTEGER NOT NULL,"
        "  created_at INTEGER NOT NULL,"
        "  expires_at INTEGER NOT NULL)",
        "CREATE INDEX IF NOT EXISTS bloodborne_wandering_ghost_area "
        "ON bloodborne_wandering_ghost(area_id, area_region_id, channel_id, expires_at)",
    };

    for (const QString& statement : stmts4) {
        if (!Exec(statement))
            return false;
    }

    QSqlQuery ins4(m_db);
    ins4.prepare("INSERT OR IGNORE INTO migration VALUES(4,'Bloodborne asynchronous world data')");
    if (!Exec(ins4))
        return false;

    const QStringList stmts5 = {
        "CREATE TABLE IF NOT EXISTS bloodborne_player_session("
        "  player_session_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  user_id INTEGER NOT NULL REFERENCES account(user_id) ON DELETE CASCADE,"
        "  started_at INTEGER NOT NULL,"
        "  ended_at INTEGER,"
        "  duration_seconds INTEGER NOT NULL DEFAULT 0,"
        "  interrupted INTEGER NOT NULL DEFAULT 0,"
        "  public_visible INTEGER NOT NULL DEFAULT 1)",
        "CREATE INDEX IF NOT EXISTS bloodborne_player_session_user "
        "ON bloodborne_player_session(user_id, started_at DESC)",
        "CREATE INDEX IF NOT EXISTS bloodborne_player_session_open "
        "ON bloodborne_player_session(user_id) WHERE ended_at IS NULL",

        "CREATE TABLE IF NOT EXISTS bloodborne_player_stats("
        "  user_id INTEGER PRIMARY KEY REFERENCES account(user_id) ON DELETE CASCADE,"
        "  messages_created INTEGER NOT NULL DEFAULT 0,"
        "  bloodstains_created INTEGER NOT NULL DEFAULT 0,"
        "  ghosts_generated INTEGER NOT NULL DEFAULT 0,"
        "  summons_advertised INTEGER NOT NULL DEFAULT 0,"
        "  summon_claims INTEGER NOT NULL DEFAULT 0)",

        "CREATE TABLE IF NOT EXISTS bloodborne_web_session("
        "  token_hash BLOB PRIMARY KEY,"
        "  user_id INTEGER NOT NULL REFERENCES account(user_id) ON DELETE CASCADE,"
        "  created_at INTEGER NOT NULL,"
        "  last_seen_at INTEGER NOT NULL,"
        "  expires_at INTEGER NOT NULL)",
        "CREATE INDEX IF NOT EXISTS bloodborne_web_session_user "
        "ON bloodborne_web_session(user_id, expires_at)",

        "CREATE TABLE IF NOT EXISTS bloodborne_web_profile("
        "  user_id INTEGER PRIMARY KEY REFERENCES account(user_id) ON DELETE CASCADE,"
        "  avatar_file TEXT NOT NULL DEFAULT '',"
        "  updated_at INTEGER NOT NULL)",

        "CREATE TABLE IF NOT EXISTS bloodborne_activity("
        "  activity_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  user_id INTEGER REFERENCES account(user_id) ON DELETE SET NULL,"
        "  event_type TEXT NOT NULL,"
        "  created_at INTEGER NOT NULL)",
        "CREATE INDEX IF NOT EXISTS bloodborne_activity_recent "
        "ON bloodborne_activity(created_at DESC, activity_id DESC)",
    };

    for (const QString& statement : stmts5) {
        if (!Exec(statement))
            return false;
    }

    // Seed exact lifetime counters from data that predates this migration. INSERT OR IGNORE
    // makes the backfill idempotent and preserves all counters recorded after migration.
    if (!Exec("INSERT OR IGNORE INTO bloodborne_player_stats("
              "user_id,messages_created,bloodstains_created,ghosts_generated,"
              "summons_advertised,summon_claims) "
              "SELECT a.user_id,"
              "(SELECT COUNT(*) FROM bloodborne_blood_message b WHERE b.owner_user_id=a.user_id),"
              "(SELECT COUNT(*) FROM bloodborne_tomb_message t WHERE t.owner_user_id=a.user_id),"
              "(SELECT COUNT(*) FROM bloodborne_wandering_ghost g "
              " WHERE g.owner_user_id=a.user_id),0,0 FROM account a")) {
        return false;
    }

    QSqlQuery ins5(m_db);
    ins5.prepare("INSERT OR IGNORE INTO migration VALUES(5,'Bloodborne community website')");
    if (!Exec(ins5))
        return false;

    const QStringList stmts6 = {
        "CREATE TABLE IF NOT EXISTS bloodborne_web_chat_message("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  account_id INTEGER NOT NULL REFERENCES account(user_id) ON DELETE CASCADE,"
        "  message TEXT NOT NULL,"
        "  created_at INTEGER NOT NULL)",
        "CREATE INDEX IF NOT EXISTS bloodborne_web_chat_message_recent "
        "ON bloodborne_web_chat_message(created_at,id)",
        "CREATE TABLE IF NOT EXISTS bloodborne_web_chat_state("
        "  key TEXT PRIMARY KEY,"
        "  value INTEGER NOT NULL)",
        "INSERT OR IGNORE INTO bloodborne_web_chat_state(key,value) "
        "VALUES('last_chat_reset',CAST(strftime('%s','now') AS INTEGER))",
    };

    for (const QString& statement : stmts6) {
        if (!Exec(statement))
            return false;
    }

    QSqlQuery ins6(m_db);
    ins6.prepare("INSERT OR IGNORE INTO migration VALUES(6,'Bloodborne website global chat')");
    if (!Exec(ins6))
        return false;

    const QStringList stmts7 = {
        "CREATE TABLE IF NOT EXISTS bloodborne_chalice("
        "  channel_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  discernment_word TEXT NOT NULL COLLATE BINARY UNIQUE,"
        "  create_user_id INTEGER NOT NULL,"
        // Bloodborne sends unsigned JSON numbers that can exceed INT64_MAX. Store their
        // decimal representation instead of coercing them into a signed SQLite INTEGER.
        "  create_chara_id TEXT NOT NULL,"
        "  create_date TEXT NOT NULL,"
        "  last_play_date TEXT NOT NULL,"
        "  fixed_or_general INTEGER NOT NULL,"
        "  form_data TEXT NOT NULL,"
        "  form_data_version INTEGER NOT NULL,"
        "  holy_grail_type_id INTEGER NOT NULL,"
        "  ritual_level INTEGER NOT NULL,"
        "  share_level INTEGER NOT NULL CHECK(share_level IN (0,1,2)),"
        "  status INTEGER NOT NULL,"
        "  sub_feature_flag INTEGER NOT NULL,"
        "  turnout_level INTEGER NOT NULL DEFAULT 0,"
        "  unlock_flag_list TEXT NOT NULL,"
        "  wish_material_list TEXT NOT NULL,"
        "  random_join_count INTEGER NOT NULL DEFAULT 0,"
        "  origin TEXT NOT NULL DEFAULT 'community' "
        "    CHECK(origin IN ('vanilla_fixed','community')),"
        // Reserved for a future decoded SVG/Canvas layout. Raw maps are never stored as PNG.
        "  map_data_json TEXT)",
        "CREATE UNIQUE INDEX IF NOT EXISTS bloodborne_chalice_glyph "
        "ON bloodborne_chalice(discernment_word COLLATE BINARY)",
        "CREATE INDEX IF NOT EXISTS bloodborne_chalice_public_search "
        "ON bloodborne_chalice(share_level,status,form_data_version,fixed_or_general,"
        "holy_grail_type_id,ritual_level,sub_feature_flag,random_join_count,last_play_date)",
        "CREATE INDEX IF NOT EXISTS bloodborne_chalice_creator "
        "ON bloodborne_chalice(create_user_id,channel_id DESC)",
    };

    for (const QString& statement : stmts7) {
        if (!Exec(statement))
            return false;
    }

    QSqlQuery ins7(m_db);
    ins7.prepare("INSERT OR IGNORE INTO migration VALUES(7,'Bloodborne Chalice Dungeons')");
    if (!Exec(ins7))
        return false;

    // Migration 8: reserve the captured vanilla IDs, classify existing player rows as
    // community data, and seed the fixed ritual catalog. SQLite does not support a useful
    // ADD COLUMN IF NOT EXISTS on all versions shipped with Qt, so inspect the schema first.
    if (!m_db.transaction()) {
        m_lastError = m_db.lastError().text();
        return false;
    }
    const auto migration8Failure = [this](const QString& error) {
        m_lastError = error;
        m_db.rollback();
        return false;
    };

    bool hasOrigin = false;
    QSqlQuery columns(m_db);
    if (!columns.exec(QStringLiteral("PRAGMA table_info(bloodborne_chalice)")))
        return migration8Failure(columns.lastError().text());
    while (columns.next()) {
        if (columns.value(1).toString() == QStringLiteral("origin")) {
            hasOrigin = true;
            break;
        }
    }
    columns.finish();
    if (!hasOrigin && !Exec(QStringLiteral(
                          "ALTER TABLE bloodborne_chalice ADD COLUMN origin TEXT NOT NULL "
                          "DEFAULT 'community' CHECK(origin IN ('vanilla_fixed','community'))"))) {
        m_db.rollback();
        return false;
    }
    if (!Exec(QStringLiteral(
            "UPDATE bloodborne_chalice SET origin='community' WHERE origin IS NULL"))) {
        m_db.rollback();
        return false;
    }

    QSqlQuery existingFixed(m_db);
    if (!existingFixed.exec(QStringLiteral(
            "SELECT COUNT(*) FROM bloodborne_chalice WHERE origin='vanilla_fixed'")) ||
        !existingFixed.next()) {
        return migration8Failure(existingFixed.lastError().text());
    }
    const int existingFixedCount = existingFixed.value(0).toInt();

    QSqlQuery maximumId(m_db);
    if (!maximumId.exec(
            QStringLiteral("SELECT MAX(COALESCE(channel_id,0)) FROM bloodborne_chalice")) ||
        !maximumId.next()) {
        return migration8Failure(maximumId.lastError().text());
    }
    qint64 nextCommunityId = std::max<qint64>(maximumId.value(0).toLongLong(), 10);
    int remappedCommunityIds = 0;
    QSqlQuery collision(m_db);
    collision.prepare(
        QStringLiteral("SELECT origin FROM bloodborne_chalice WHERE channel_id=? LIMIT 1"));
    QSqlQuery remap(m_db);
    remap.prepare(QStringLiteral(
        "UPDATE bloodborne_chalice SET channel_id=? WHERE channel_id=? AND origin='community'"));
    for (const Bloodborne::CapturedFixedChalice& fixture : Bloodborne::CapturedFixedChalices) {
        collision.bindValue(0, fixture.channelId);
        if (!collision.exec())
            return migration8Failure(collision.lastError().text());
        if (collision.next() && collision.value(0).toString() != QStringLiteral("vanilla_fixed")) {
            remap.bindValue(0, ++nextCommunityId);
            remap.bindValue(1, fixture.channelId);
            if (!remap.exec() || remap.numRowsAffected() != 1)
                return migration8Failure(remap.lastError().text());
            ++remappedCommunityIds;
        }
        collision.finish();
    }

    QSqlQuery insertFixed(m_db);
    insertFixed.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO bloodborne_chalice("
        "channel_id,discernment_word,create_user_id,create_chara_id,create_date,"
        "last_play_date,fixed_or_general,form_data,form_data_version,holy_grail_type_id,"
        "ritual_level,share_level,status,sub_feature_flag,turnout_level,unlock_flag_list,"
        "wish_material_list,origin) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));
    QSqlQuery validateFixed(m_db);
    validateFixed.prepare(QStringLiteral(
        "SELECT discernment_word,create_user_id,create_chara_id,create_date,last_play_date,"
        "fixed_or_general,form_data,form_data_version,holy_grail_type_id,ritual_level,"
        "share_level,status,sub_feature_flag,turnout_level,unlock_flag_list,"
        "wish_material_list,origin FROM bloodborne_chalice WHERE channel_id=?"));
    for (const Bloodborne::CapturedFixedChalice& fixture : Bloodborne::CapturedFixedChalices) {
        const QString glyph = QString::fromLatin1(fixture.glyph);
        const QString date = QString::fromLatin1(fixture.date);
        const QString formData = QString::fromLatin1(fixture.formData);
        const QString unlockFlags =
            QStringLiteral("[{\"UnlockFlag\":0},{\"UnlockFlag\":%1},{\"UnlockFlag\":0}]")
                .arg(fixture.unlockFlag);
        insertFixed.bindValue(0, fixture.channelId);
        insertFixed.bindValue(1, glyph);
        insertFixed.bindValue(2, 0);
        insertFixed.bindValue(3, QStringLiteral("0"));
        insertFixed.bindValue(4, date);
        insertFixed.bindValue(5, date);
        insertFixed.bindValue(6, 2);
        insertFixed.bindValue(7, formData);
        insertFixed.bindValue(8, 0);
        insertFixed.bindValue(9, fixture.holyGrailTypeId);
        insertFixed.bindValue(10, fixture.ritualLevel);
        insertFixed.bindValue(11, 2);
        insertFixed.bindValue(12, 1);
        insertFixed.bindValue(13, 256);
        insertFixed.bindValue(14, 0);
        insertFixed.bindValue(15, unlockFlags);
        insertFixed.bindValue(16, QStringLiteral("[]"));
        insertFixed.bindValue(17, QStringLiteral("vanilla_fixed"));
        if (!insertFixed.exec())
            return migration8Failure(insertFixed.lastError().text());

        validateFixed.bindValue(0, fixture.channelId);
        if (!validateFixed.exec() || !validateFixed.next())
            return migration8Failure(validateFixed.lastError().text());
        const bool valid =
            validateFixed.value(0).toString() == glyph && validateFixed.value(1).toInt() == 0 &&
            validateFixed.value(2).toString() == QStringLiteral("0") &&
            validateFixed.value(3).toString() == date &&
            validateFixed.value(4).toString() == date && validateFixed.value(5).toInt() == 2 &&
            validateFixed.value(6).toString() == formData && validateFixed.value(7).toInt() == 0 &&
            validateFixed.value(8).toInt() == fixture.holyGrailTypeId &&
            validateFixed.value(9).toInt() == fixture.ritualLevel &&
            validateFixed.value(10).toInt() == 2 && validateFixed.value(11).toInt() == 1 &&
            validateFixed.value(12).toInt() == 256 && validateFixed.value(13).toInt() == 0 &&
            validateFixed.value(14).toString() == unlockFlags &&
            validateFixed.value(15).toString() == QStringLiteral("[]") &&
            validateFixed.value(16).toString() == QStringLiteral("vanilla_fixed");
        validateFixed.finish();
        if (!valid) {
            return migration8Failure(
                QStringLiteral("Captured fixed Chalice integrity check failed for ChannelId %1")
                    .arg(fixture.channelId));
        }
    }

    if (!Exec(QStringLiteral("CREATE INDEX IF NOT EXISTS bloodborne_chalice_origin "
                             "ON bloodborne_chalice(origin,share_level,status,channel_id)"))) {
        m_db.rollback();
        return false;
    }
    QSqlQuery sequence(m_db);
    if (!sequence.exec(QStringLiteral(
            "UPDATE sqlite_sequence SET seq=(SELECT MAX(channel_id) FROM bloodborne_chalice) "
            "WHERE name='bloodborne_chalice'"))) {
        return migration8Failure(sequence.lastError().text());
    }
    if (sequence.numRowsAffected() == 0) {
        if (!sequence.exec(
                QStringLiteral("INSERT INTO sqlite_sequence(name,seq) VALUES('bloodborne_chalice',"
                               "(SELECT MAX(channel_id) FROM bloodborne_chalice))"))) {
            return migration8Failure(sequence.lastError().text());
        }
    }

    QSqlQuery ins8(m_db);
    ins8.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO migration VALUES(8,'Bloodborne fixed Chalice catalog')"));
    if (!ins8.exec())
        return migration8Failure(ins8.lastError().text());
    if (!m_db.commit()) {
        m_lastError = m_db.lastError().text();
        return false;
    }
    const int insertedFixedCount =
        static_cast<int>(Bloodborne::CapturedFixedChalices.size()) - existingFixedCount;
    qInfo().noquote() << "[BLOODBORNE CHALICE FIXED BOOTSTRAP]"
                      << "inserted=" + QString::number(std::max(0, insertedFixedCount))
                      << "existing=" + QString::number(existingFixedCount)
                      << "remapped_community_ids=" + QString::number(remappedCommunityIds);

    qInfo() << "Database migrations complete";

    RunMaintenance();
    return true;
}

bool Database::RecordBloodborneWebsiteEvent(qint64 userId, BloodborneWebsiteEvent event,
                                            int amount) {
    if (userId <= 0 || amount <= 0 || !m_db.isOpen())
        return false;

    QString column;
    QString eventType;
    switch (event) {
    case BloodborneWebsiteEvent::MessageCreated:
        column = QStringLiteral("messages_created");
        eventType = QStringLiteral("message_created");
        break;
    case BloodborneWebsiteEvent::BloodstainCreated:
        column = QStringLiteral("bloodstains_created");
        eventType = QStringLiteral("bloodstain_created");
        break;
    case BloodborneWebsiteEvent::GhostCreated:
        column = QStringLiteral("ghosts_generated");
        eventType = QStringLiteral("ghost_created");
        break;
    case BloodborneWebsiteEvent::SummonAdvertised:
        column = QStringLiteral("summons_advertised");
        eventType = QStringLiteral("summon_advertised");
        break;
    case BloodborneWebsiteEvent::SummonClaimed:
        column = QStringLiteral("summon_claims");
        eventType = QStringLiteral("summon_claimed");
        break;
    }

    if (!m_db.transaction()) {
        qWarning() << "Bloodborne website metric transaction failed:" << m_db.lastError().text();
        return false;
    }

    QSqlQuery stats(m_db);
    stats.prepare(QStringLiteral("INSERT INTO bloodborne_player_stats(user_id,%1) VALUES(?,?) "
                                 "ON CONFLICT(user_id) DO UPDATE SET %1=%1+excluded.%1")
                      .arg(column));
    stats.addBindValue(userId);
    stats.addBindValue(amount);

    QSqlQuery activity(m_db);
    activity.prepare(QStringLiteral(
        "INSERT INTO bloodborne_activity(user_id,event_type,created_at) VALUES(?,?,?)"));
    activity.addBindValue(userId);
    activity.addBindValue(eventType);
    activity.addBindValue(QDateTime::currentSecsSinceEpoch());

    if (!stats.exec() || !activity.exec() || !m_db.commit()) {
        qWarning() << "Bloodborne website metric write failed:"
                   << (stats.lastError().isValid() ? stats.lastError().text()
                                                   : activity.lastError().text());
        m_db.rollback();
        return false;
    }
    return true;
}

QList<qint64> Database::GetOrCreateBloodborneCharaIds(qint64 userId, int count) {
    QList<qint64> result;
    if (userId <= 0 || count <= 0 || count > 16)
        return result;

    if (!m_db.transaction()) {
        m_lastError = m_db.lastError().text();
        return result;
    }

    for (int slot = 0; slot < count; ++slot) {
        QSqlQuery insert(m_db);
        insert.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO bloodborne_character(user_id, slot) VALUES(?, ?)"));
        insert.addBindValue(userId);
        insert.addBindValue(slot);
        if (!insert.exec()) {
            m_lastError = insert.lastError().text();
            m_db.rollback();
            return {};
        }

        QSqlQuery select(m_db);
        select.prepare(
            QStringLiteral("SELECT chara_id FROM bloodborne_character WHERE user_id=? AND slot=?"));
        select.addBindValue(userId);
        select.addBindValue(slot);
        if (!select.exec() || !select.next()) {
            m_lastError = select.lastError().text();
            m_db.rollback();
            return {};
        }
        result.append(select.value(0).toLongLong());
    }

    if (!m_db.commit()) {
        m_lastError = m_db.lastError().text();
        return {};
    }
    return result;
}

std::optional<DbError> Database::CreateAccount(const QString& npid, const QString& password,
                                               const QString& avatarUrl, const QString& email) {
    // Input validation
    if (npid.isEmpty()) {
        qWarning() << "createAccount: NPID is empty";
        return DbError::InvalidInput;
    }

    if (password.isEmpty()) {
        qWarning() << "createAccount: Password is empty";
        return DbError::InvalidInput;
    }

    // Check database connection
    if (!m_db.isOpen() || !m_db.isValid()) {
        qCritical() << "createAccount: Database connection is not valid";
        return DbError::Internal;
    }

    // Username collision check
    {
        QSqlQuery q(m_db);
        if (!q.prepare("SELECT COUNT(*) FROM account WHERE username=? COLLATE NOCASE")) {
            qCritical() << "createAccount: Failed to prepare username check query:"
                        << q.lastError().text();
            return DbError::Internal;
        }

        q.addBindValue(npid);

        if (!q.exec()) {
            qCritical() << "createAccount: Failed to execute username check:"
                        << q.lastError().text();
            return DbError::Internal;
        }

        if (!q.next()) {
            qCritical() << "createAccount: Failed to get username check result";
            return DbError::Internal;
        }

        if (q.value(0).toInt() > 0) {
            qWarning() << "createAccount: Username already exists:" << npid;
            return DbError::ExistingUsername;
        }
    }

    // Email collision check (if email is provided)
    if (!email.isEmpty()) {
        QString emailCheck = email.toLower().trimmed();

        // Basic email format validation
        if (!emailCheck.contains('@') || !emailCheck.contains('.')) {
            qWarning() << "createAccount: Invalid email format:" << email;
            return DbError::InvalidEmail;
        }

        QSqlQuery q(m_db);
        if (!q.prepare("SELECT COUNT(*) FROM account WHERE email_check=?")) {
            qCritical() << "createAccount: Failed to prepare email check query:"
                        << q.lastError().text();
            return DbError::Internal;
        }

        q.addBindValue(emailCheck);

        if (!q.exec()) {
            qCritical() << "createAccount: Failed to execute email check:" << q.lastError().text();
            return DbError::Internal;
        }

        if (!q.next()) {
            qCritical() << "createAccount: Failed to get email check result";
            return DbError::Internal;
        }

        if (q.value(0).toInt() > 0) {
            qWarning() << "createAccount: Email already exists:" << emailCheck;
            return DbError::ExistingEmail;
        }
    }

    // Generate cryptographic values
    QByteArray salt = GenerateSalt();
    if (salt.isEmpty()) {
        qCritical() << "createAccount: Failed to generate salt";
        return DbError::Internal;
    }

    QByteArray hash = HashPassword(password, salt);
    if (hash.isEmpty()) {
        qCritical() << "createAccount: Failed to generate password hash";
        return DbError::Internal;
    }

    QString token = GenerateToken();
    if (token.isEmpty()) {
        qCritical() << "createAccount: Failed to generate token";
        return DbError::Internal;
    }

    qint64 now = QDateTime::currentSecsSinceEpoch();

    // Store the new account ID for later use
    int64_t newId = -1;

    // Start transaction
    if (!m_db.transaction()) {
        qCritical() << "createAccount: Failed to start transaction:" << m_db.lastError().text();
        return DbError::Internal;
    }

    // Insert account
    {
        QSqlQuery q(m_db);
        if (!q.prepare("INSERT INTO account(username, hash, salt, avatar_url, "
                       "email, email_check, token, admin, stat_agent, banned) "
                       "VALUES(?, ?, ?, ?, ?, ?, ?, 0, 0, 0)")) {
            qCritical() << "createAccount: Failed to prepare insert query:" << q.lastError().text();
            m_db.rollback();
            return DbError::Internal;
        }

        q.addBindValue(npid);
        q.addBindValue(hash);
        q.addBindValue(salt);
        q.addBindValue(avatarUrl);
        q.addBindValue(email);
        q.addBindValue(email.isEmpty() ? "" : email.toLower().trimmed());
        q.addBindValue(token);

        if (!q.exec()) {
            qCritical() << "createAccount: Failed to insert account:" << q.lastError().text();
            m_db.rollback();

            // Check for specific SQL errors
            if (q.lastError().nativeErrorCode() == "19" || // SQLITE_CONSTRAINT
                q.lastError().text().contains("UNIQUE", Qt::CaseInsensitive)) {
                return DbError::ExistingUsername;
            }
            return DbError::Internal;
        }

        // Get the new account ID
        QVariant lastId = q.lastInsertId();
        if (!lastId.isValid() || lastId.isNull()) {
            qCritical() << "createAccount: Failed to get last insert ID";
            m_db.rollback();
            return DbError::Internal;
        }

        bool ok;
        newId = lastId.toLongLong(&ok);
        if (!ok || newId <= 0) {
            qCritical() << "createAccount: Invalid last insert ID:" << lastId;
            m_db.rollback();
            return DbError::Internal;
        }
    }

    // Insert timestamp (using a separate query with the newId we saved)
    {
        QSqlQuery q2(m_db);
        if (!q2.prepare("INSERT INTO account_timestamp(user_id, creation) VALUES(?, ?)")) {
            qCritical() << "createAccount: Failed to prepare timestamp query:"
                        << q2.lastError().text();
            m_db.rollback();
            return DbError::Internal;
        }

        q2.addBindValue(static_cast<qlonglong>(newId));
        q2.addBindValue(now);

        if (!q2.exec()) {
            qCritical() << "createAccount: Failed to insert timestamp:" << q2.lastError().text();
            m_db.rollback();
            return DbError::Internal;
        }
    }

    // Commit transaction
    if (!m_db.commit()) {
        qCritical() << "createAccount: Failed to commit transaction:" << m_db.lastError().text();
        m_db.rollback();
        return DbError::Internal;
    }

    qInfo() << "createAccount: Successfully created account:" << npid << "(ID:" << newId << ")";

    return std::nullopt; // success
}

std::optional<UserRecord> Database::CheckUser(const QString& npid, const QString& password,
                                              const QString& token, bool checkToken) {
    QSqlQuery q(m_db);
    q.prepare("SELECT user_id,username,hash,salt,avatar_url,email,email_check,"
              "token,admin,stat_agent,banned FROM account WHERE username=? COLLATE NOCASE");
    q.addBindValue(npid);
    if (!Exec(q) || !q.next())
        return std::nullopt; // Empty = no such user

    const QString canonicalUsername = q.value(1).toString();
    if (canonicalUsername != npid) {
        return std::nullopt;
    }

    UserRecord r;
    r.userId = q.value(0).toLongLong();
    r.username = canonicalUsername; // exact match
    r.hash = q.value(2).toByteArray();
    r.salt = q.value(3).toByteArray();
    r.avatarUrl = q.value(4).toString();
    r.email = q.value(5).toString();
    r.emailCheck = q.value(6).toString();
    r.token = q.value(7).toString();
    r.admin = q.value(8).toBool();
    r.statAgent = q.value(9).toBool();
    r.banned = q.value(10).toBool();

    QByteArray computed = HashPassword(password, r.salt);
    if (computed != r.hash)
        return std::nullopt; // WrongPass

    if (checkToken && r.token != token)
        return std::nullopt; // WrongToken

    return r;
}

std::optional<int64_t> Database::GetUserId(const QString& npid) {
    QSqlQuery q(m_db);
    q.prepare("SELECT user_id FROM account WHERE username=? COLLATE NOCASE");
    q.addBindValue(npid);
    if (!Exec(q) || !q.next())
        return std::nullopt;
    return q.value(0).toLongLong();
}

std::optional<QString> Database::GetUsername(int64_t userId) {
    QSqlQuery q(m_db);
    q.prepare("SELECT username FROM account WHERE user_id=?");
    q.addBindValue(static_cast<qlonglong>(userId));
    if (!Exec(q) || !q.next())
        return std::nullopt;
    return q.value(0).toString();
}

std::optional<QString> Database::GetAvatarUrl(int64_t userId) {
    QSqlQuery q(m_db);
    q.prepare("SELECT avatar_url FROM account WHERE user_id=?");
    q.addBindValue(static_cast<qlonglong>(userId));
    if (!Exec(q) || !q.next())
        return std::nullopt;
    return q.value(0).toString();
}

QList<QPair<int64_t, QString>> Database::GetUsernamesFromIds(const QSet<int64_t>& ids) {
    QList<QPair<int64_t, QString>> result;
    if (ids.isEmpty())
        return result;

    // Build IN clause
    QStringList placeholders;
    for (int i = 0; i < ids.size(); ++i)
        placeholders << "?";
    QSqlQuery q(m_db);
    q.prepare(QString("SELECT user_id,username FROM account WHERE user_id IN (%1)")
                  .arg(placeholders.join(',')));
    for (int64_t id : ids)
        q.addBindValue(static_cast<qlonglong>(id));
    if (!Exec(q))
        return result;
    while (q.next())
        result << qMakePair(q.value(0).toLongLong(), q.value(1).toString());
    return result;
}

bool Database::UpdateLoginTime(int64_t userId) {
    uint64_t now = static_cast<uint64_t>(QDateTime::currentSecsSinceEpoch());
    QSqlQuery q(m_db);
    q.prepare("UPDATE account_timestamp SET last_login=? WHERE user_id=?");
    q.addBindValue(static_cast<qint64>(now));
    q.addBindValue(static_cast<qlonglong>(userId));
    return Exec(q);
}

bool Database::BanUser(int64_t userId, bool ban) {
    QSqlQuery q(m_db);
    q.prepare("UPDATE account SET banned=? WHERE user_id=?");
    q.addBindValue(ban ? 1 : 0);
    q.addBindValue(static_cast<qlonglong>(userId));
    return Exec(q);
}

bool Database::DeleteUser(int64_t userId) {
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM account WHERE user_id=?");
    q.addBindValue(static_cast<qlonglong>(userId));
    return Exec(q);
}

bool Database::SetAdmin(int64_t userId, bool admin) {
    QSqlQuery q(m_db);
    q.prepare("UPDATE account SET admin=? WHERE user_id=?");
    q.addBindValue(admin ? 1 : 0);
    q.addBindValue(static_cast<qlonglong>(userId));
    return Exec(q);
}

int Database::TotalUsers() {
    QSqlQuery q(m_db);
    q.exec("SELECT COUNT(*) FROM account");
    return q.next() ? q.value(0).toInt() : 0;
}

void Database::CleanNeverUsedAccounts() {
    // Delete accounts that never logged in and are older than 30 days
    uint64_t cutoff = static_cast<uint64_t>(QDateTime::currentSecsSinceEpoch()) - 30 * 86400;
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM account WHERE user_id IN ("
              "  SELECT user_id FROM account_timestamp WHERE creation < ? AND last_login IS NULL)");
    q.addBindValue(static_cast<qint64>(cutoff));
    Exec(q);
}

void Database::RunMaintenance() {
    // Remove score rows whose communication_id is blank. An empty com id is
    // stored as 12 NUL bytes (GetNpCommId pads a missing id), so a plain '=' '''
    // check would miss them; strip NULs (and coalesce NULL) before comparing.
    const QString blank =
        QStringLiteral("replace(coalesce(communication_id, ''), char(0), '') = ''");
    for (const QString& table : {QStringLiteral("score"), QStringLiteral("score_table")}) {
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral("DELETE FROM %1 WHERE %2").arg(table, blank));
        if (Exec(q)) {
            const int n = q.numRowsAffected();
            if (n > 0) {
                qInfo() << "Maintenance: removed" << n << "empty-comId row(s) from" << table;
            }
        }
    }
}

// Title name mapping

bool Database::SetTitleName(const QString& comId, const QString& titleName) {
    if (comId.isEmpty() || titleName.isEmpty())
        return false;
    QSqlQuery q(m_db);
    q.prepare("INSERT OR IGNORE INTO title_name(communication_id, title_name) "
              "VALUES(?, ?)");
    q.addBindValue(comId);
    q.addBindValue(titleName);
    return Exec(q);
}

std::optional<QString> Database::GetTitleName(const QString& comId) {
    if (comId.isEmpty())
        return std::nullopt;
    QSqlQuery q(m_db);
    q.prepare("SELECT title_name FROM title_name WHERE communication_id=?");
    q.addBindValue(comId);
    if (!Exec(q) || !q.next())
        return std::nullopt;
    return q.value(0).toString();
}

QList<Database::GameTitleRow> Database::ListScoredGameTitles() {
    QList<GameTitleRow> out;
    QSqlQuery q(m_db);
    q.prepare("SELECT s.communication_id, COALESCE(tn.title_name, '') AS name "
              "FROM (SELECT DISTINCT communication_id FROM score) s "
              "LEFT JOIN title_name tn ON tn.communication_id = s.communication_id "
              "ORDER BY name ASC, s.communication_id ASC");
    if (!Exec(q))
        return out;
    while (q.next()) {
        GameTitleRow r;
        r.comId = q.value(0).toString();
        r.titleName = q.value(1).toString();
        out.append(r);
    }
    return out;
}

// Friendship DB methods

// The friendship table always stores rows with user_id_1 < user_id_2.
// This helper returns the canonical (lower, higher) order and a flag
// indicating whether the caller is user_id_2 (i.e. the IDs were swapped).
static std::tuple<int64_t, int64_t, bool> orderedIds(int64_t a, int64_t b) {
    if (a < b)
        return {a, b, false};
    return {b, a, true};
}

std::pair<Database::RelResult, Database::RelStatus> Database::GetRelStatus(int64_t callerId,
                                                                           int64_t otherId) {
    auto [id1, id2, swapped] = orderedIds(callerId, otherId);
    QSqlQuery q(m_db);
    q.prepare("SELECT status_user_1, status_user_2 FROM friendship "
              "WHERE user_id_1=? AND user_id_2=?");
    q.addBindValue(static_cast<qlonglong>(id1));
    q.addBindValue(static_cast<qlonglong>(id2));
    if (!Exec(q))
        return {RelResult::Error, {}};
    if (!q.next())
        return {RelResult::Empty, {}};

    RelStatus s;
    uint8_t s1 = static_cast<uint8_t>(q.value(0).toInt());
    uint8_t s2 = static_cast<uint8_t>(q.value(1).toInt());
    s.caller = swapped ? s2 : s1;
    s.other = swapped ? s1 : s2;
    return {RelResult::Ok, s};
}

bool Database::SetRelStatus(int64_t callerId, int64_t otherId, uint8_t statusCaller,
                            uint8_t statusOther) {
    auto [id1, id2, swapped] = orderedIds(callerId, otherId);
    uint8_t s1 = swapped ? statusOther : statusCaller;
    uint8_t s2 = swapped ? statusCaller : statusOther;

    QSqlQuery q(m_db);
    q.prepare("INSERT INTO friendship(user_id_1, user_id_2, status_user_1, status_user_2) "
              "VALUES(?,?,?,?) "
              "ON CONFLICT(user_id_1, user_id_2) DO UPDATE SET "
              "status_user_1=excluded.status_user_1, status_user_2=excluded.status_user_2");
    q.addBindValue(static_cast<qlonglong>(id1));
    q.addBindValue(static_cast<qlonglong>(id2));
    q.addBindValue(s1);
    q.addBindValue(s2);
    return Exec(q);
}

bool Database::DeleteRel(int64_t callerId, int64_t otherId) {
    auto [id1, id2, swapped] = orderedIds(callerId, otherId);
    Q_UNUSED(swapped);
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM friendship WHERE user_id_1=? AND user_id_2=?");
    q.addBindValue(static_cast<qlonglong>(id1));
    q.addBindValue(static_cast<qlonglong>(id2));
    return Exec(q);
}

UserRelationships Database::GetRelationships(int64_t userId) {
    UserRelationships result;

    QSqlQuery q(m_db);
    q.prepare("SELECT user_id_1, user_id_2, status_user_1, status_user_2 "
              "FROM friendship WHERE user_id_1=? OR user_id_2=?");
    q.addBindValue(static_cast<qlonglong>(userId));
    q.addBindValue(static_cast<qlonglong>(userId));
    if (!Exec(q))
        return result;

    constexpr uint8_t F = static_cast<uint8_t>(FriendStatus::Friend);
    constexpr uint8_t B = static_cast<uint8_t>(FriendStatus::Blocked);

    while (q.next()) {
        int64_t uid1 = q.value(0).toLongLong();
        int64_t uid2 = q.value(1).toLongLong();
        uint8_t su1 = static_cast<uint8_t>(q.value(2).toInt());
        uint8_t su2 = static_cast<uint8_t>(q.value(3).toInt());

        // Rotate so statusMe / statusOther are from our perspective.
        int64_t otherId;
        uint8_t statusMe, statusOther;
        if (uid1 == userId) {
            otherId = uid2;
            statusMe = su1;
            statusOther = su2;
        } else {
            otherId = uid1;
            statusMe = su2;
            statusOther = su1;
        }

        // Resolve the other user's npid.
        auto npidOpt = GetUsername(otherId);
        if (!npidOpt)
            continue;
        auto pair = qMakePair(otherId, *npidOpt);

        if ((statusMe & F) && (statusOther & F)) {
            result.friends.append(pair);
        } else if ((statusMe & F) && !(statusOther & F)) {
            result.friendRequestsSent.append(pair);
        } else if (!(statusMe & F) && (statusOther & F)) {
            result.friendRequestsReceived.append(pair);
        }
        // Blocked: we blocked them
        if (statusMe & B) {
            result.blocked.append(pair);
        }
    }

    return result;
}
