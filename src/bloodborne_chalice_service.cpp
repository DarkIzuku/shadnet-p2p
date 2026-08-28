// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include "bloodborne_chalice_service.h"

#include <algorithm>
#include <cmath>
#include <optional>

#include <QDateTime>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRandomGenerator>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>

#include "database.h"

namespace Bloodborne {
namespace {

constexpr qsizetype MaxListItems = 100;
constexpr qsizetype MaxFormDataCharacters = 2 * 1024 * 1024;
constexpr double Uint64LimitExclusive = 18446744073709551616.0; // 2^64

OnlineResult Success(const char* messageId) {
    OnlineResult result;
    result.response.insert(QStringLiteral("MessageId"), QLatin1String(messageId));
    result.response.insert(QStringLiteral("ResKind"), 0);
    return result;
}

OnlineResult Failure(OnlineError error, const QString& detail) {
    OnlineResult result;
    result.error = error;
    result.detail = detail;
    return result;
}

OnlineResult SqlFailure(const QSqlQuery& query, const QString& operation) {
    qWarning().noquote() << "Bloodborne Chalice database error:" << operation
                         << query.lastError().text();
    return Failure(OnlineError::Database, operation);
}

bool IsInteger(const QJsonValue& value, double minimum, double maximum) {
    if (!value.isDouble())
        return false;
    const double number = value.toDouble();
    return std::isfinite(number) && std::floor(number) == number && number >= minimum &&
           number <= maximum;
}

bool IsUnsigned64Integer(const QJsonValue& value) {
    return IsInteger(value, 0, std::nextafter(Uint64LimitExclusive, 0.0));
}

bool IsArray(const QJsonObject& object, const char* name, qsizetype maximum = MaxListItems) {
    const QJsonValue value = object.value(QLatin1String(name));
    return value.isArray() && value.toArray().size() <= maximum;
}

bool ValidNumberObjectList(const QJsonObject& request, const char* listName, const char* fieldName,
                           double minimum, double maximum, bool allowEmpty = true) {
    if (!IsArray(request, listName))
        return false;
    const QJsonArray list = request.value(QLatin1String(listName)).toArray();
    if (!allowEmpty && list.isEmpty())
        return false;
    for (const QJsonValue& value : list) {
        if (!value.isObject() ||
            !IsInteger(value.toObject().value(QLatin1String(fieldName)), minimum, maximum)) {
            return false;
        }
    }
    return true;
}

bool ValidOpaqueObjectList(const QJsonObject& request, const char* listName) {
    if (!IsArray(request, listName))
        return false;
    for (const QJsonValue& value : request.value(QLatin1String(listName)).toArray()) {
        if (!value.isObject())
            return false;
    }
    return true;
}

bool ValidNullableInteger(const QJsonObject& request, const char* name, double minimum,
                          double maximum) {
    const QJsonValue value = request.value(QLatin1String(name));
    return value.isNull() || IsInteger(value, minimum, maximum);
}

QString CompactArray(const QJsonArray& value) {
    return QString::fromUtf8(QJsonDocument(value).toJson(QJsonDocument::Compact));
}

std::optional<QJsonArray> StoredArray(const QString& json) {
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());
    if (!document.isArray())
        return std::nullopt;
    return document.array();
}

QString UnsignedJsonIntegerText(const QJsonValue& value) {
    QJsonObject wrapper;
    wrapper.insert(QStringLiteral("value"), value);
    const QByteArray json = QJsonDocument(wrapper).toJson(QJsonDocument::Compact);
    const qsizetype colon = json.indexOf(':');
    return QString::fromLatin1(json.mid(colon + 1, json.size() - colon - 2));
}

QString CurrentBloodborneDate() {
    return QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyy-MM-ddTHH:mm:ss"));
}

QString GenerateGlyph() {
    // All captured user-created glyphs contain eight lower-case characters from this
    // Bloodborne-compatible alphabet. Ambiguous 0, 1, l and o are intentionally absent.
    static const QString Alphabet = QStringLiteral("23456789abcdefghijkmnpqrstuvwxyz");
    QString glyph;
    glyph.reserve(8);
    QRandomGenerator* random = QRandomGenerator::system();
    for (int index = 0; index < 8; ++index)
        glyph.append(Alphabet.at(random->bounded(Alphabet.size())));
    return glyph;
}

QString ChannelColumns() {
    return QStringLiteral(
        "channel_id,create_chara_id,create_date,create_user_id,discernment_word,"
        "fixed_or_general,form_data,form_data_version,holy_grail_type_id,last_play_date,"
        "ritual_level,share_level,status,sub_feature_flag,turnout_level,unlock_flag_list,"
        "wish_material_list");
}

std::optional<QJsonObject> ChannelObject(const QSqlQuery& query) {
    const auto unlockFlags = StoredArray(query.value(15).toString());
    const auto wishMaterials = StoredArray(query.value(16).toString());
    if (!unlockFlags || !wishMaterials)
        return std::nullopt;

    QJsonObject channel;
    channel.insert(QStringLiteral("ChannelId"), query.value(0).toLongLong());
    channel.insert(QStringLiteral("CreateCharaId"), query.value(1).toString().toDouble());
    channel.insert(QStringLiteral("CreateDate"), query.value(2).toString());
    channel.insert(QStringLiteral("CreateUserId"), query.value(3).toLongLong());
    channel.insert(QStringLiteral("DiscernmentWord"), query.value(4).toString());
    channel.insert(QStringLiteral("FixedOrGeneral"), query.value(5).toInt());
    channel.insert(QStringLiteral("FormData"), query.value(6).toString());
    channel.insert(QStringLiteral("FormDataVersion"), query.value(7).toInt());
    channel.insert(QStringLiteral("HolyGrailTypeId"), query.value(8).toInt());
    channel.insert(QStringLiteral("LastPlayDate"), query.value(9).toString());
    channel.insert(QStringLiteral("RitualLevel"), query.value(10).toInt());
    channel.insert(QStringLiteral("ShareLevel"), query.value(11).toInt());
    channel.insert(QStringLiteral("Status"), query.value(12).toInt());
    channel.insert(QStringLiteral("SubFeatureFlag"), query.value(13).toVariant().toLongLong());
    channel.insert(QStringLiteral("TurnoutLevel"), query.value(14).toInt());
    channel.insert(QStringLiteral("UnlockFlagList"), *unlockFlags);
    channel.insert(QStringLiteral("WishMaterialList"), *wishMaterials);
    return channel;
}

bool ValidUpload(const QJsonObject& request) {
    const QJsonValue formData = request.value(QStringLiteral("FormData"));
    return IsUnsigned64Integer(request.value(QStringLiteral("CharaId"))) &&
           IsInteger(request.value(QStringLiteral("FixedOrGeneral")), 0, 2) &&
           formData.isString() && !formData.toString().isEmpty() &&
           formData.toString().size() <= MaxFormDataCharacters &&
           IsInteger(request.value(QStringLiteral("FormDataVersion")), 0, 0x7FFFFFFF) &&
           IsInteger(request.value(QStringLiteral("HolyGrailTypeId")), 0, 0x7FFFFFFF) &&
           IsInteger(request.value(QStringLiteral("RitualLevel")), 0, 0x7FFFFFFF) &&
           IsInteger(request.value(QStringLiteral("ShareLevel")), 0, 2) &&
           IsInteger(request.value(QStringLiteral("Status")), 0, 0x7FFFFFFF) &&
           IsInteger(request.value(QStringLiteral("SubFeatureFlag")), 0, 0xFFFFFFFFLL) &&
           ValidNumberObjectList(request, "UnlockFlagList", "UnlockFlag", 0, 0xFFFFFFFFLL) &&
           ValidOpaqueObjectList(request, "WishMaterialList");
}

bool ValidChannelIdList(const QJsonObject& request) {
    return ValidNumberObjectList(request, "ChannelIdList", "ChannelId", 1, 9007199254740991.0);
}

QList<qint64> ChannelIds(const QJsonObject& request) {
    QList<qint64> ids;
    QSet<qint64> seen;
    for (const QJsonValue& value : request.value(QStringLiteral("ChannelIdList")).toArray()) {
        const qint64 id = value.toObject().value(QStringLiteral("ChannelId")).toInteger();
        if (!seen.contains(id)) {
            ids.append(id);
            seen.insert(id);
        }
    }
    return ids;
}

QJsonObject LostChannel(qint64 channelId) {
    QJsonObject lost;
    lost.insert(QStringLiteral("LostChannelId"), channelId);
    return lost;
}

} // namespace

ChaliceService::ChaliceService(Database& db) : m_db(db) {}

OnlineResult ChaliceService::Upload(qint64 userId, const QJsonObject& request) {
    if (userId <= 0 || !ValidUpload(request))
        return Failure(OnlineError::InvalidRequest, QStringLiteral("Invalid ChannelUploadRequest"));

    const QString createDate = CurrentBloodborneDate();
    const QString unlockFlags =
        CompactArray(request.value(QStringLiteral("UnlockFlagList")).toArray());
    const QString wishMaterials =
        CompactArray(request.value(QStringLiteral("WishMaterialList")).toArray());
    QSqlQuery insert(m_db.Conn());
    insert.prepare(QStringLiteral(
        "INSERT INTO bloodborne_chalice(discernment_word,create_user_id,create_chara_id,"
        "create_date,last_play_date,fixed_or_general,form_data,form_data_version,"
        "holy_grail_type_id,ritual_level,share_level,status,sub_feature_flag,turnout_level,"
        "unlock_flag_list,wish_material_list) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));

    for (int attempt = 0; attempt < 32; ++attempt) {
        const QString glyph = GenerateGlyph();
        insert.bindValue(0, glyph);
        insert.bindValue(1, userId);
        insert.bindValue(2, UnsignedJsonIntegerText(request.value(QStringLiteral("CharaId"))));
        insert.bindValue(3, createDate);
        insert.bindValue(4, createDate);
        insert.bindValue(5, request.value(QStringLiteral("FixedOrGeneral")).toInt());
        insert.bindValue(6, request.value(QStringLiteral("FormData")).toString());
        insert.bindValue(7, request.value(QStringLiteral("FormDataVersion")).toInt());
        insert.bindValue(8, request.value(QStringLiteral("HolyGrailTypeId")).toInt());
        insert.bindValue(9, request.value(QStringLiteral("RitualLevel")).toInt());
        insert.bindValue(10, request.value(QStringLiteral("ShareLevel")).toInt());
        insert.bindValue(11, request.value(QStringLiteral("Status")).toInt());
        insert.bindValue(12, request.value(QStringLiteral("SubFeatureFlag")).toVariant());
        insert.bindValue(13, 0);
        insert.bindValue(14, unlockFlags);
        insert.bindValue(15, wishMaterials);
        if (insert.exec()) {
            const qint64 channelId = insert.lastInsertId().toLongLong();
            OnlineResult result = Success("ChannelUploadResponse");
            result.response.insert(QStringLiteral("ChannelId"), channelId);
            result.response.insert(QStringLiteral("DiscernmentWord"), glyph);
            qInfo().noquote()
                << "[BLOODBORNE CHALICE UPLOAD]"
                << "user_id=" + QString::number(userId)
                << "channel_id=" + QString::number(channelId) << "glyph=" + glyph
                << "share_level=" +
                       QString::number(request.value(QStringLiteral("ShareLevel")).toInt())
                << "form_data_bytes=" +
                       QString::number(
                           request.value(QStringLiteral("FormData")).toString().toUtf8().size());
            return result;
        }
        if (!insert.lastError().text().contains(QStringLiteral("discernment_word"),
                                                Qt::CaseInsensitive)) {
            return SqlFailure(insert, QStringLiteral("Could not store Chalice"));
        }
    }
    return Failure(OnlineError::Database, QStringLiteral("Could not allocate unique glyph"));
}

OnlineResult ChaliceService::Share(qint64 userId, const QJsonObject& request) {
    if (userId <= 0 ||
        !IsInteger(request.value(QStringLiteral("ChannelId")), 1, 9007199254740991.0) ||
        !IsUnsigned64Integer(request.value(QStringLiteral("CharaId"))) ||
        !IsInteger(request.value(QStringLiteral("ShareLevel")), 0, 2)) {
        return Failure(OnlineError::InvalidRequest, QStringLiteral("Invalid ChannelShareRequest"));
    }

    const qint64 channelId = request.value(QStringLiteral("ChannelId")).toInteger();
    const int shareLevel = request.value(QStringLiteral("ShareLevel")).toInt();
    QSqlQuery update(m_db.Conn());
    update.prepare(QStringLiteral(
        "UPDATE bloodborne_chalice SET share_level=? WHERE channel_id=? AND create_user_id=?"));
    update.addBindValue(shareLevel);
    update.addBindValue(channelId);
    update.addBindValue(userId);
    if (!update.exec())
        return SqlFailure(update, QStringLiteral("Could not update Chalice sharing"));

    qInfo().noquote() << "[BLOODBORNE CHALICE SHARE]"
                      << "user_id=" + QString::number(userId)
                      << "channel_id=" + QString::number(channelId)
                      << "share_level=" + QString::number(shareLevel);
    // The captured backend returns success for unknown IDs and does not create a record.
    return Success("ChannelShareResponse");
}

OnlineResult ChaliceService::WordSearch(qint64 userId, const QJsonObject& request) {
    if (userId <= 0 ||
        !IsInteger(request.value(QStringLiteral("FormDataVersion")), 0, 0x7FFFFFFF) ||
        !request.value(QStringLiteral("SearchWord")).isString() ||
        !ValidNumberObjectList(request, "UnlockedFlagList", "UnlockedFlag", 0, 0xFFFFFFFFLL)) {
        return Failure(OnlineError::InvalidRequest,
                       QStringLiteral("Invalid ChannelWordSearchRequest"));
    }

    const QString glyph = request.value(QStringLiteral("SearchWord")).toString();
    QSqlQuery query(m_db.Conn());
    query.prepare(QStringLiteral("SELECT ") + ChannelColumns() +
                  QStringLiteral(" FROM bloodborne_chalice WHERE discernment_word=? "
                                 "AND form_data_version=? AND share_level=2 AND status=1 LIMIT 1"));
    query.addBindValue(glyph);
    query.addBindValue(request.value(QStringLiteral("FormDataVersion")).toInt());
    if (!query.exec())
        return SqlFailure(query, QStringLiteral("Could not search Chalice glyph"));

    OnlineResult result = Success("ChannelWordSearchResponse");
    bool found = false;
    if (query.next()) {
        const auto channel = ChannelObject(query);
        if (!channel)
            return Failure(OnlineError::Database, QStringLiteral("Stored Chalice is invalid"));
        for (auto it = channel->constBegin(); it != channel->constEnd(); ++it)
            result.response.insert(it.key(), it.value());
        found = true;
    }
    qInfo().noquote() << "[BLOODBORNE CHALICE WORD SEARCH]"
                      << "user_id=" + QString::number(userId) << "glyph=" + glyph
                      << "found=" +
                             QString(found ? QStringLiteral("true") : QStringLiteral("false"));
    return result;
}

OnlineResult ChaliceService::Search(qint64 userId, const QJsonObject& request) {
    if (userId <= 0 ||
        !ValidNumberObjectList(request, "FixedOrGeneralList", "FixedOrGeneral", 0, 2, false) ||
        !IsInteger(request.value(QStringLiteral("FormDataVersion")), 0, 0x7FFFFFFF) ||
        !IsInteger(request.value(QStringLiteral("GetCount")), 1, MaxListItems) ||
        !ValidNullableInteger(request, "HolyGrailTypeId", 0, 0x7FFFFFFF) ||
        !ValidNullableInteger(request, "RitualLevel", 0, 0x7FFFFFFF) ||
        !IsInteger(request.value(QStringLiteral("Status")), 0, 0x7FFFFFFF) ||
        !ValidNullableInteger(request, "SubFeatureFlag", 0, 0xFFFFFFFFLL) ||
        !ValidNumberObjectList(request, "UnlockedFlagList", "UnlockedFlag", 0, 0xFFFFFFFFLL)) {
        return Failure(OnlineError::InvalidRequest, QStringLiteral("Invalid ChannelSearchRequest"));
    }

    QStringList fixedValues;
    for (const QJsonValue& value : request.value(QStringLiteral("FixedOrGeneralList")).toArray()) {
        const QString fixed =
            QString::number(value.toObject().value(QStringLiteral("FixedOrGeneral")).toInt());
        if (!fixedValues.contains(fixed))
            fixedValues.append(fixed);
    }

    QStringList fixedPlaceholders;
    for (qsizetype i = 0; i < fixedValues.size(); ++i)
        fixedPlaceholders.append(QStringLiteral("?"));

    QString sql = QStringLiteral("SELECT ") + ChannelColumns() +
                  QStringLiteral(" FROM bloodborne_chalice WHERE share_level=2 AND status=? "
                                 "AND form_data_version=? AND fixed_or_general IN (") +
                  fixedPlaceholders.join(QLatin1Char(',')) + QLatin1Char(')');
    const QJsonValue holyGrail = request.value(QStringLiteral("HolyGrailTypeId"));
    const QJsonValue ritual = request.value(QStringLiteral("RitualLevel"));
    const QJsonValue subFeature = request.value(QStringLiteral("SubFeatureFlag"));
    if (!holyGrail.isNull())
        sql += QStringLiteral(" AND holy_grail_type_id=?");
    if (!ritual.isNull())
        sql += QStringLiteral(" AND ritual_level=?");
    // Vanilla uses 4 for the observed "any root rites" search. Other numeric values are
    // compared exactly; null and 4 intentionally do not narrow the result set.
    if (!subFeature.isNull() && subFeature.toInt() != 4)
        sql += QStringLiteral(" AND sub_feature_flag=?");
    sql += QStringLiteral(" ORDER BY last_play_date DESC,channel_id DESC LIMIT ?");

    QSqlQuery query(m_db.Conn());
    query.prepare(sql);
    query.addBindValue(request.value(QStringLiteral("Status")).toInt());
    query.addBindValue(request.value(QStringLiteral("FormDataVersion")).toInt());
    for (const QString& fixed : fixedValues)
        query.addBindValue(fixed.toInt());
    if (!holyGrail.isNull())
        query.addBindValue(holyGrail.toInt());
    if (!ritual.isNull())
        query.addBindValue(ritual.toInt());
    if (!subFeature.isNull() && subFeature.toInt() != 4)
        query.addBindValue(subFeature.toVariant());
    query.addBindValue(request.value(QStringLiteral("GetCount")).toInt());
    if (!query.exec())
        return SqlFailure(query, QStringLiteral("Could not search Chalices"));

    QJsonArray channels;
    while (query.next()) {
        const auto channel = ChannelObject(query);
        if (!channel)
            return Failure(OnlineError::Database, QStringLiteral("Stored Chalice is invalid"));
        channels.append(*channel);
    }
    OnlineResult result = Success("ChannelSearchResponse");
    result.response.insert(QStringLiteral("ChannelList"), channels);
    qInfo().noquote() << "[BLOODBORNE CHALICE SEARCH]"
                      << "user_id=" + QString::number(userId)
                      << "results=" + QString::number(channels.size());
    return result;
}

OnlineResult ChaliceService::GetInfo(qint64 userId, const QJsonObject& request) {
    if (userId <= 0 || !ValidChannelIdList(request))
        return Failure(OnlineError::InvalidRequest,
                       QStringLiteral("Invalid ChannelGetInfoRequest"));

    QJsonArray found;
    QJsonArray lost;
    QSqlQuery query(m_db.Conn());
    query.prepare(QStringLiteral(
        "SELECT share_level,status,turnout_level FROM bloodborne_chalice WHERE channel_id=?"));
    for (qint64 channelId : ChannelIds(request)) {
        query.bindValue(0, channelId);
        if (!query.exec())
            return SqlFailure(query, QStringLiteral("Could not load Chalice info"));
        if (!query.next()) {
            lost.append(LostChannel(channelId));
            continue;
        }
        QJsonObject info;
        info.insert(QStringLiteral("ChannelId"), channelId);
        info.insert(QStringLiteral("ShareLevel"), query.value(0).toInt());
        info.insert(QStringLiteral("Status"), query.value(1).toInt());
        info.insert(QStringLiteral("TurnoutLevel"), query.value(2).toInt());
        found.append(info);
    }
    OnlineResult result = Success("ChannelGetInfoResponse");
    result.response.insert(QStringLiteral("ChannelInfoList"), found);
    result.response.insert(QStringLiteral("LostChannelIdList"), lost);
    return result;
}

OnlineResult ChaliceService::GetDetailsInfo(qint64 userId, const QJsonObject& request) {
    if (userId <= 0 || !ValidChannelIdList(request)) {
        return Failure(OnlineError::InvalidRequest,
                       QStringLiteral("Invalid ChannelGetDetailsInfoRequest"));
    }

    QJsonArray found;
    QJsonArray lost;
    QSqlQuery query(m_db.Conn());
    query.prepare(QStringLiteral("SELECT ") + ChannelColumns() +
                  QStringLiteral(" FROM bloodborne_chalice WHERE channel_id=?"));
    for (qint64 channelId : ChannelIds(request)) {
        query.bindValue(0, channelId);
        if (!query.exec())
            return SqlFailure(query, QStringLiteral("Could not load Chalice details"));
        if (!query.next()) {
            lost.append(LostChannel(channelId));
            continue;
        }
        const auto channel = ChannelObject(query);
        if (!channel)
            return Failure(OnlineError::Database, QStringLiteral("Stored Chalice is invalid"));
        found.append(*channel);
    }
    OnlineResult result = Success("ChannelGetDetailsInfoResponse");
    result.response.insert(QStringLiteral("ChannelList"), found);
    result.response.insert(QStringLiteral("LostChannelIdList"), lost);
    return result;
}

OnlineResult ChaliceService::RandomJoin(qint64 userId, const QJsonObject& request) {
    if (userId <= 0 || !IsUnsigned64Integer(request.value(QStringLiteral("CharaId"))) ||
        !IsInteger(request.value(QStringLiteral("FormDataVersion")), 0, 0x7FFFFFFF) ||
        !ValidNumberObjectList(request, "UnlockedFlagList", "UnlockedFlag", 0, 0xFFFFFFFFLL) ||
        !IsArray(request, "RandomJoinTargetList") ||
        request.value(QStringLiteral("RandomJoinTargetList")).toArray().isEmpty()) {
        return Failure(OnlineError::InvalidRequest,
                       QStringLiteral("Invalid ChannelRandomJoinRequest"));
    }

    QStringList targets;
    QList<QPair<int, int>> values;
    for (const QJsonValue& value :
         request.value(QStringLiteral("RandomJoinTargetList")).toArray()) {
        if (!value.isObject())
            return Failure(OnlineError::InvalidRequest,
                           QStringLiteral("Invalid ChannelRandomJoinRequest"));
        const QJsonObject target = value.toObject();
        if (!IsInteger(target.value(QStringLiteral("HolyGrailTypeId")), 0, 0x7FFFFFFF) ||
            !IsInteger(target.value(QStringLiteral("RitualLevel")), 0, 0x7FFFFFFF)) {
            return Failure(OnlineError::InvalidRequest,
                           QStringLiteral("Invalid ChannelRandomJoinRequest"));
        }
        targets.append(QStringLiteral("(holy_grail_type_id=? AND ritual_level=?)"));
        values.append({target.value(QStringLiteral("HolyGrailTypeId")).toInt(),
                       target.value(QStringLiteral("RitualLevel")).toInt()});
    }

    QSqlDatabase db = m_db.Conn();
    if (!db.transaction())
        return Failure(OnlineError::Database, QStringLiteral("Could not start random join"));
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT ") + ChannelColumns() +
                  QStringLiteral(" FROM bloodborne_chalice WHERE share_level=2 AND status=1 "
                                 "AND form_data_version=? AND (") +
                  targets.join(QStringLiteral(" OR ")) +
                  QStringLiteral(") ORDER BY random_join_count ASC,last_play_date ASC,"
                                 "channel_id ASC LIMIT 1"));
    query.addBindValue(request.value(QStringLiteral("FormDataVersion")).toInt());
    for (const auto& target : values) {
        query.addBindValue(target.first);
        query.addBindValue(target.second);
    }
    if (!query.exec()) {
        db.rollback();
        return SqlFailure(query, QStringLiteral("Could not find random Chalice"));
    }

    OnlineResult result = Success("ChannelRandomJoinResponse");
    qint64 channelId = 0;
    if (query.next()) {
        const auto channel = ChannelObject(query);
        if (!channel) {
            db.rollback();
            return Failure(OnlineError::Database, QStringLiteral("Stored Chalice is invalid"));
        }
        channelId = channel->value(QStringLiteral("ChannelId")).toInteger();
        for (auto it = channel->constBegin(); it != channel->constEnd(); ++it)
            result.response.insert(it.key(), it.value());
        query.finish();

        QSqlQuery update(db);
        update.prepare(QStringLiteral(
            "UPDATE bloodborne_chalice SET last_play_date=?,random_join_count=random_join_count+1 "
            "WHERE channel_id=?"));
        update.addBindValue(CurrentBloodborneDate());
        update.addBindValue(channelId);
        if (!update.exec()) {
            db.rollback();
            return SqlFailure(update, QStringLiteral("Could not update Chalice play date"));
        }
    }
    if (!db.commit())
        return Failure(OnlineError::Database, QStringLiteral("Could not finish random join"));

    qInfo().noquote() << "[BLOODBORNE CHALICE RANDOM JOIN]"
                      << "user_id=" + QString::number(userId)
                      << "channel_id=" +
                             (channelId > 0 ? QString::number(channelId) : QStringLiteral("0"));
    return result;
}

} // namespace Bloodborne
