// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include "bloodborne_online_service.h"

#include <algorithm>
#include <cmath>
#include <optional>

#include <QByteArray>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QJsonArray>
#include <QJsonValue>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include "database.h"

namespace Bloodborne {
namespace {

constexpr qsizetype MaxListItems = 100;
constexpr qsizetype MaxEncodedBlobBytes = 2 * 1024 * 1024;
constexpr qsizetype MaxDecodedBlobBytes = 1024 * 1024;
constexpr qint64 MaxJsonSafeInteger = 9007199254740991LL;

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
    qWarning().noquote() << "Bloodborne online database error:" << operation
                         << query.lastError().text();
    return Failure(OnlineError::Database, operation);
}

bool IsNumber(const QJsonObject& object, const char* name) {
    const QJsonValue value = object.value(QLatin1String(name));
    return value.isDouble() && std::isfinite(value.toDouble());
}

bool IsInteger(const QJsonObject& object, const char* name, qint64 minimum, qint64 maximum) {
    const QJsonValue value = object.value(QLatin1String(name));
    if (!value.isDouble())
        return false;
    const double number = value.toDouble();
    return std::isfinite(number) && std::floor(number) == number && number >= minimum &&
           number <= maximum;
}

bool IsArray(const QJsonObject& object, const char* name, qsizetype maximum = MaxListItems) {
    const QJsonValue value = object.value(QLatin1String(name));
    return value.isArray() && value.toArray().size() <= maximum;
}

bool IsString(const QJsonObject& object, const char* name) {
    return object.value(QLatin1String(name)).isString();
}

bool IsBoolean(const QJsonObject& object, const char* name) {
    return object.value(QLatin1String(name)).isBool();
}

std::optional<qint64> SafeId(const QJsonValue& value) {
    if (!value.isDouble())
        return std::nullopt;
    const double number = value.toDouble();
    if (!std::isfinite(number) || std::floor(number) != number || number <= 0 ||
        number > static_cast<double>(MaxJsonSafeInteger)) {
        return std::nullopt;
    }
    return static_cast<qint64>(number);
}

bool ValidBase64(const QJsonObject& object, const char* name, QByteArray* decoded = nullptr) {
    const QJsonValue value = object.value(QLatin1String(name));
    if (!value.isString())
        return false;
    const QByteArray encoded = value.toString().toLatin1();
    if (encoded.isEmpty() || encoded.size() > MaxEncodedBlobBytes)
        return false;
    const auto result =
        QByteArray::fromBase64Encoding(encoded, QByteArray::AbortOnBase64DecodingErrors);
    if (!result || result.decoded.isEmpty() || result.decoded.size() > MaxDecodedBlobBytes ||
        result.decoded.toBase64() != encoded) {
        return false;
    }
    if (decoded != nullptr)
        *decoded = result.decoded;
    return true;
}

QString Hash(const QByteArray& bytes) {
    return QString::fromLatin1(
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex().toUpper());
}

QString NumberKey(double number) {
    return QString::number(number, 'g', 17);
}

bool ValidArea(const QJsonObject& area) {
    return IsInteger(area, "AreaId", 0, 0xFFFFFFFFLL) &&
           IsInteger(area, "AreaRegionId", -1, 0x7FFFFFFFLL) &&
           IsInteger(area, "ChannelId", 0, 0x7FFFFFFFLL) &&
           IsInteger(area, "GetCount", 0, MaxListItems);
}

QJsonObject BloodMessageItem(const QSqlQuery& query) {
    QJsonObject item;
    item.insert(QStringLiteral("BloodData"), query.value(QStringLiteral("blood_data")).toString());
    item.insert(QStringLiteral("BloodDataVersion"),
                query.value(QStringLiteral("blood_data_version")).toInt());
    item.insert(QStringLiteral("BloodMessId"),
                query.value(QStringLiteral("blood_mess_id")).toLongLong());
    item.insert(QStringLiteral("EvaluateMinus"),
                query.value(QStringLiteral("evaluate_minus")).toLongLong());
    item.insert(QStringLiteral("EvaluatePlus"),
                query.value(QStringLiteral("evaluate_plus")).toLongLong());
    item.insert(QStringLiteral("MessShellInfo"),
                query.value(QStringLiteral("shell_data")).toString());
    item.insert(QStringLiteral("MessShellInfoVersion"),
                query.value(QStringLiteral("shell_data_version")).toInt());
    return item;
}

QString BloodMessageSelect() {
    return QStringLiteral(
        "SELECT b.blood_mess_id, b.blood_data, b.blood_data_version, "
        "b.base_evaluate_minus + (SELECT COUNT(*) FROM "
        "bloodborne_blood_message_evaluation e WHERE e.blood_mess_id=b.blood_mess_id AND "
        "e.evaluate_kind=-1) AS evaluate_minus, "
        "b.base_evaluate_plus + (SELECT COUNT(*) FROM "
        "bloodborne_blood_message_evaluation e WHERE e.blood_mess_id=b.blood_mess_id AND "
        "e.evaluate_kind=1) AS evaluate_plus, "
        "s.shell_data, s.shell_data_version "
        "FROM bloodborne_blood_message b JOIN bloodborne_messenger_shell s "
        "ON s.user_id=b.owner_user_id ");
}

bool PurgeExpiredGhosts(QSqlDatabase db) {
    QSqlQuery query(db);
    query.prepare(QStringLiteral("DELETE FROM bloodborne_wandering_ghost WHERE expires_at<=?"));
    query.addBindValue(QDateTime::currentSecsSinceEpoch());
    if (!query.exec()) {
        qWarning() << "Bloodborne ghost expiry failed:" << query.lastError().text();
        return false;
    }
    return true;
}

} // namespace

OnlineService::OnlineService(Database& db, int ghostLifetimeSeconds)
    : m_db(db), m_ghostLifetimeSeconds(std::clamp(ghostLifetimeSeconds, 60, 604800)) {}

OnlineResult OnlineService::UploadMessengerShell(qint64 userId, const QJsonObject& request) {
    QByteArray decoded;
    if (!IsNumber(request, "CharaId") || !IsInteger(request, "ShellDataVersion", 0, 1000) ||
        !ValidBase64(request, "ShellData", &decoded)) {
        return Failure(OnlineError::InvalidRequest,
                       QStringLiteral("invalid MessengerShellUploadRequest"));
    }

    QSqlQuery query(m_db.Conn());
    query.prepare(QStringLiteral(
        "INSERT INTO bloodborne_messenger_shell(user_id, chara_id, shell_data, "
        "shell_data_version, updated_at) VALUES(?,?,?,?,?) "
        "ON CONFLICT(user_id) DO UPDATE SET chara_id=excluded.chara_id, "
        "shell_data=excluded.shell_data, shell_data_version=excluded.shell_data_version, "
        "updated_at=excluded.updated_at"));
    query.addBindValue(userId);
    query.addBindValue(request.value(QStringLiteral("CharaId")).toDouble());
    query.addBindValue(request.value(QStringLiteral("ShellData")).toString());
    query.addBindValue(request.value(QStringLiteral("ShellDataVersion")).toInt());
    query.addBindValue(QDateTime::currentSecsSinceEpoch());
    if (!query.exec())
        return SqlFailure(query, QStringLiteral("store messenger shell"));

    return Success("MessengerShellUploadResponse");
}

OnlineResult OnlineService::CreateBloodMessages(qint64 userId, const QJsonObject& request) {
    if (!IsNumber(request, "CharaId") || !IsArray(request, "BloodMessList") ||
        request.value(QStringLiteral("BloodMessList")).toArray().isEmpty()) {
        return Failure(OnlineError::InvalidRequest,
                       QStringLiteral("invalid BloodMessCreateRequest"));
    }

    const QJsonArray messages = request.value(QStringLiteral("BloodMessList")).toArray();
    for (const QJsonValue& value : messages) {
        if (!value.isObject())
            return Failure(OnlineError::InvalidRequest,
                           QStringLiteral("BloodMessList item is not an object"));
        const QJsonObject item = value.toObject();
        if (!IsInteger(item, "AreaId", 0, 0xFFFFFFFFLL) ||
            !IsInteger(item, "AreaRegionId", 0, 0x7FFFFFFFLL) ||
            !IsInteger(item, "ChannelId", 0, 0x7FFFFFFFLL) ||
            !IsInteger(item, "BloodDataVersion", 0, 1000) ||
            !IsInteger(item, "CharaDataVersion", 0, 1000) ||
            !IsInteger(item, "EvaluateMinus", 0, 0x7FFFFFFFLL) ||
            !IsInteger(item, "EvaluatePlus", 0, 0x7FFFFFFFLL) ||
            !IsNumber(item, "PrevBloodMessId") || !IsBoolean(item, "CharaData") ||
            !ValidBase64(item, "BloodData")) {
            return Failure(OnlineError::InvalidRequest,
                           QStringLiteral("invalid BloodMessList item"));
        }
    }

    QSqlDatabase db = m_db.Conn();
    if (!db.transaction())
        return Failure(OnlineError::Database, QStringLiteral("begin BloodMess create"));

    QJsonArray ids;
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    for (const QJsonValue& value : messages) {
        const QJsonObject item = value.toObject();
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "INSERT INTO bloodborne_blood_message(owner_user_id, owner_chara_id, area_id, "
            "area_region_id, channel_id, blood_data, blood_data_version, chara_data, "
            "chara_data_version, base_evaluate_plus, base_evaluate_minus, prev_blood_mess_id, "
            "created_at) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?)"));
        query.addBindValue(userId);
        query.addBindValue(request.value(QStringLiteral("CharaId")).toDouble());
        query.addBindValue(item.value(QStringLiteral("AreaId")).toVariant());
        query.addBindValue(item.value(QStringLiteral("AreaRegionId")).toVariant());
        query.addBindValue(item.value(QStringLiteral("ChannelId")).toVariant());
        query.addBindValue(item.value(QStringLiteral("BloodData")).toString());
        query.addBindValue(item.value(QStringLiteral("BloodDataVersion")).toInt());
        query.addBindValue(item.value(QStringLiteral("CharaData")).toBool() ? 1 : 0);
        query.addBindValue(item.value(QStringLiteral("CharaDataVersion")).toInt());
        query.addBindValue(item.value(QStringLiteral("EvaluatePlus")).toVariant());
        query.addBindValue(item.value(QStringLiteral("EvaluateMinus")).toVariant());
        query.addBindValue(item.value(QStringLiteral("PrevBloodMessId")).toDouble());
        query.addBindValue(now);
        if (!query.exec()) {
            db.rollback();
            return SqlFailure(query, QStringLiteral("create blood message"));
        }
        const qint64 id = query.lastInsertId().toLongLong();
        QJsonObject created;
        created.insert(QStringLiteral("BloodMessId"), id);
        created.insert(QStringLiteral("PrevBloodMessId"),
                       item.value(QStringLiteral("PrevBloodMessId")));
        ids.append(created);

        QByteArray decoded;
        ValidBase64(item, "BloodData", &decoded);
        qInfo().noquote() << "[BLOODBORNE MESSAGE CREATE]"
                          << "user_id=" + QString::number(userId) << "id=" + QString::number(id)
                          << "area=" + QString::number(item.value(QStringLiteral("AreaId")).toInt())
                          << "region=" +
                                 QString::number(item.value(QStringLiteral("AreaRegionId")).toInt())
                          << "blob_bytes=" + QString::number(decoded.size())
                          << "sha256=" + Hash(decoded);
    }

    if (!db.commit())
        return Failure(OnlineError::Database, QStringLiteral("commit BloodMess create"));

    OnlineResult result = Success("BloodMessCreateResponse");
    result.response.insert(QStringLiteral("BloodMessIdList"), ids);
    return result;
}

OnlineResult OnlineService::GetBloodMessages(qint64 userId, const QJsonObject& request) {
    if (!IsArray(request, "AreaInfoList") || !IsInteger(request, "BloodDataVersion", 0, 1000) ||
        !IsInteger(request, "CharaDataVersion", 0, 1000) ||
        !IsInteger(request, "GetMaxCount", 0, MaxListItems) ||
        !IsInteger(request, "MessShellInfoVersion", 0, 1000)) {
        return Failure(OnlineError::InvalidRequest,
                       QStringLiteral("invalid BloodMessGetListRequest"));
    }

    const QJsonArray areas = request.value(QStringLiteral("AreaInfoList")).toArray();
    for (const QJsonValue& value : areas) {
        if (!value.isObject() || !ValidArea(value.toObject()))
            return Failure(OnlineError::InvalidRequest, QStringLiteral("invalid AreaInfoList"));
    }

    const int maximum = request.value(QStringLiteral("GetMaxCount")).toInt();
    QJsonArray messages;
    QSet<qint64> seen;
    for (const QJsonValue& value : areas) {
        if (messages.size() >= maximum)
            break;
        const QJsonObject area = value.toObject();
        const int areaLimit = std::min(area.value(QStringLiteral("GetCount")).toInt(),
                                       maximum - static_cast<int>(messages.size()));
        if (areaLimit <= 0)
            continue;

        QSqlQuery query(m_db.Conn());
        query.prepare(
            BloodMessageSelect() +
            QStringLiteral("WHERE b.owner_user_id<>? AND b.area_id=? AND b.area_region_id=? "
                           "AND b.channel_id=? AND b.blood_data_version=? "
                           "AND s.shell_data_version=? ORDER BY b.created_at DESC, "
                           "b.blood_mess_id DESC LIMIT ?"));
        query.addBindValue(userId);
        query.addBindValue(area.value(QStringLiteral("AreaId")).toVariant());
        query.addBindValue(area.value(QStringLiteral("AreaRegionId")).toVariant());
        query.addBindValue(area.value(QStringLiteral("ChannelId")).toVariant());
        query.addBindValue(request.value(QStringLiteral("BloodDataVersion")).toInt());
        query.addBindValue(request.value(QStringLiteral("MessShellInfoVersion")).toInt());
        query.addBindValue(areaLimit);
        if (!query.exec())
            return SqlFailure(query, QStringLiteral("get blood messages"));
        while (query.next() && messages.size() < maximum) {
            const qint64 id = query.value(QStringLiteral("blood_mess_id")).toLongLong();
            if (seen.contains(id))
                continue;
            seen.insert(id);
            messages.append(BloodMessageItem(query));
        }
    }

    qInfo().noquote() << "[BLOODBORNE MESSAGE GET]"
                      << "user_id=" + QString::number(userId)
                      << "count=" + QString::number(messages.size());
    OnlineResult result = Success("BloodMessGetListResponse");
    result.response.insert(QStringLiteral("BloodMessList"), messages);
    return result;
}

OnlineResult OnlineService::EvaluateBloodMessage(qint64 userId, const QJsonObject& request) {
    const auto id = SafeId(request.value(QStringLiteral("BloodMessId")));
    if (!id || !IsInteger(request, "EvaluateKind", -1, 1) ||
        request.value(QStringLiteral("EvaluateKind")).toInt() == 0) {
        return Failure(OnlineError::InvalidRequest,
                       QStringLiteral("invalid BloodMessEvaluateRequest"));
    }

    QSqlDatabase db = m_db.Conn();
    QSqlQuery owner(db);
    owner.prepare(
        QStringLiteral("SELECT owner_user_id FROM bloodborne_blood_message WHERE blood_mess_id=?"));
    owner.addBindValue(*id);
    if (!owner.exec())
        return SqlFailure(owner, QStringLiteral("find blood message owner"));
    if (!owner.next())
        return Failure(OnlineError::NotFound, QStringLiteral("blood message does not exist"));
    if (owner.value(0).toLongLong() == userId)
        return Failure(OnlineError::Forbidden, QStringLiteral("cannot evaluate own blood message"));

    QSqlQuery evaluation(db);
    evaluation.prepare(
        QStringLiteral("INSERT INTO bloodborne_blood_message_evaluation(blood_mess_id, user_id, "
                       "evaluate_kind, created_at) VALUES(?,?,?,?) "
                       "ON CONFLICT(blood_mess_id,user_id) DO UPDATE SET "
                       "evaluate_kind=excluded.evaluate_kind, created_at=excluded.created_at"));
    evaluation.addBindValue(*id);
    evaluation.addBindValue(userId);
    evaluation.addBindValue(request.value(QStringLiteral("EvaluateKind")).toInt());
    evaluation.addBindValue(QDateTime::currentSecsSinceEpoch());
    if (!evaluation.exec())
        return SqlFailure(evaluation, QStringLiteral("evaluate blood message"));

    QSqlQuery selected(db);
    selected.prepare(BloodMessageSelect() + QStringLiteral("WHERE b.blood_mess_id=?"));
    selected.addBindValue(*id);
    if (!selected.exec())
        return SqlFailure(selected, QStringLiteral("read evaluated blood message"));
    if (!selected.next())
        return Failure(OnlineError::NotFound, QStringLiteral("blood message shell is unavailable"));

    QJsonArray messages;
    messages.append(BloodMessageItem(selected));
    qInfo().noquote() << "[BLOODBORNE MESSAGE EVALUATE]"
                      << "user_id=" + QString::number(userId) << "id=" + QString::number(*id)
                      << "kind=" +
                             QString::number(request.value(QStringLiteral("EvaluateKind")).toInt());
    OnlineResult result = Success("BloodMessEvaluateResponse");
    result.response.insert(QStringLiteral("BloodMessList"), messages);
    return result;
}

OnlineResult OnlineService::GetBloodEvaluations(qint64 userId, const QJsonObject& request) {
    if (!IsArray(request, "BloodMessIdList"))
        return Failure(OnlineError::InvalidRequest,
                       QStringLiteral("invalid BloodMessGetEvaluateRequest"));

    QJsonArray evaluations;
    for (const QJsonValue& value : request.value(QStringLiteral("BloodMessIdList")).toArray()) {
        const auto id = SafeId(value);
        if (!id)
            return Failure(OnlineError::InvalidRequest, QStringLiteral("invalid BloodMessIdList"));
        QSqlQuery query(m_db.Conn());
        query.prepare(QStringLiteral(
            "SELECT b.blood_mess_id, b.base_evaluate_minus + "
            "(SELECT COUNT(*) FROM bloodborne_blood_message_evaluation e WHERE "
            "e.blood_mess_id=b.blood_mess_id AND e.evaluate_kind=-1) AS evaluate_minus, "
            "b.base_evaluate_plus + (SELECT COUNT(*) FROM "
            "bloodborne_blood_message_evaluation e WHERE "
            "e.blood_mess_id=b.blood_mess_id AND e.evaluate_kind=1) AS evaluate_plus "
            "FROM bloodborne_blood_message b WHERE b.blood_mess_id=?"));
        query.addBindValue(*id);
        if (!query.exec())
            return SqlFailure(query, QStringLiteral("get blood message evaluation"));
        if (!query.next())
            continue;
        QJsonObject item;
        item.insert(QStringLiteral("BloodMessId"), query.value(0).toLongLong());
        item.insert(QStringLiteral("EvaluateMinus"), query.value(1).toLongLong());
        item.insert(QStringLiteral("EvaluatePlus"), query.value(2).toLongLong());
        evaluations.append(item);
    }

    qInfo().noquote() << "[BLOODBORNE MESSAGE EVALUATION GET]"
                      << "user_id=" + QString::number(userId)
                      << "count=" + QString::number(evaluations.size());
    OnlineResult result = Success("BloodMessGetEvaluateResponse");
    result.response.insert(QStringLiteral("BloodMessEvaluationList"), evaluations);
    return result;
}

OnlineResult OnlineService::SearchBloodMessages(qint64 userId, const QJsonObject& request) {
    if (!IsArray(request, "BloodMessIdList") || !IsNumber(request, "CharaId"))
        return Failure(OnlineError::InvalidRequest,
                       QStringLiteral("invalid BloodMessSearchAddRequest"));

    QJsonArray evaluations;
    QJsonArray lost;
    for (const QJsonValue& value : request.value(QStringLiteral("BloodMessIdList")).toArray()) {
        const auto id = SafeId(value);
        if (!id)
            return Failure(OnlineError::InvalidRequest, QStringLiteral("invalid BloodMessIdList"));
        QSqlQuery query(m_db.Conn());
        query.prepare(
            QStringLiteral("SELECT b.blood_mess_id, b.base_evaluate_minus + "
                           "(SELECT COUNT(*) FROM bloodborne_blood_message_evaluation e WHERE "
                           "e.blood_mess_id=b.blood_mess_id AND e.evaluate_kind=-1), "
                           "b.base_evaluate_plus + (SELECT COUNT(*) FROM "
                           "bloodborne_blood_message_evaluation e WHERE "
                           "e.blood_mess_id=b.blood_mess_id AND e.evaluate_kind=1) "
                           "FROM bloodborne_blood_message b WHERE b.blood_mess_id=?"));
        query.addBindValue(*id);
        if (!query.exec())
            return SqlFailure(query, QStringLiteral("search blood message"));
        if (!query.next()) {
            lost.append(*id);
            continue;
        }
        QJsonObject item;
        item.insert(QStringLiteral("BloodMessId"), query.value(0).toLongLong());
        item.insert(QStringLiteral("EvaluateMinus"), query.value(1).toLongLong());
        item.insert(QStringLiteral("EvaluatePlus"), query.value(2).toLongLong());
        evaluations.append(item);
    }

    OnlineResult result = Success("BloodMessSearchAddResponse");
    result.response.insert(QStringLiteral("BloodMessEvaluationList"), evaluations);
    result.response.insert(QStringLiteral("LostBloodMessIdList"), lost);
    return result;
}

OnlineResult OnlineService::DeleteBloodMessage(qint64 userId, const QJsonObject& request) {
    const auto id = SafeId(request.value(QStringLiteral("BloodMessId")));
    if (!id)
        return Failure(OnlineError::InvalidRequest,
                       QStringLiteral("invalid BloodMessRemoveRequest"));
    QSqlQuery query(m_db.Conn());
    query.prepare(QStringLiteral(
        "DELETE FROM bloodborne_blood_message WHERE blood_mess_id=? AND owner_user_id=?"));
    query.addBindValue(*id);
    query.addBindValue(userId);
    if (!query.exec())
        return SqlFailure(query, QStringLiteral("delete blood message"));
    if (query.numRowsAffected() != 1)
        return Failure(OnlineError::NotFound, QStringLiteral("owned blood message does not exist"));
    qInfo().noquote() << "[BLOODBORNE MESSAGE DELETE]"
                      << "user_id=" + QString::number(userId) << "id=" + QString::number(*id);
    return Success("BloodMessRemoveResponse");
}

OnlineResult OnlineService::CreateTombMessage(qint64 userId, const QJsonObject& request) {
    QByteArray tombData;
    QByteArray deathVisionData;
    if (!IsInteger(request, "AreaId", 0, 0xFFFFFFFFLL) ||
        !IsInteger(request, "AreaRegionId", 0, 0x7FFFFFFFLL) ||
        !IsInteger(request, "ChannelId", 0, 0x7FFFFFFFLL) || !IsNumber(request, "CharaId") ||
        !IsInteger(request, "TombDataVersion", 0, 1000) ||
        !IsInteger(request, "DeathVisionDataVersion", 0, 1000) ||
        !ValidBase64(request, "TombData", &tombData) ||
        !ValidBase64(request, "DeathVisionData", &deathVisionData)) {
        return Failure(OnlineError::InvalidRequest,
                       QStringLiteral("invalid TombMessCreateRequest"));
    }

    QSqlQuery query(m_db.Conn());
    query.prepare(QStringLiteral(
        "INSERT INTO bloodborne_tomb_message(owner_user_id, owner_chara_id, area_id, "
        "area_region_id, channel_id, tomb_data, tomb_data_version, death_vision_data, "
        "death_vision_data_version, created_at) VALUES(?,?,?,?,?,?,?,?,?,?)"));
    query.addBindValue(userId);
    query.addBindValue(request.value(QStringLiteral("CharaId")).toDouble());
    query.addBindValue(request.value(QStringLiteral("AreaId")).toVariant());
    query.addBindValue(request.value(QStringLiteral("AreaRegionId")).toVariant());
    query.addBindValue(request.value(QStringLiteral("ChannelId")).toVariant());
    query.addBindValue(request.value(QStringLiteral("TombData")).toString());
    query.addBindValue(request.value(QStringLiteral("TombDataVersion")).toInt());
    query.addBindValue(request.value(QStringLiteral("DeathVisionData")).toString());
    query.addBindValue(request.value(QStringLiteral("DeathVisionDataVersion")).toInt());
    query.addBindValue(QDateTime::currentSecsSinceEpoch());
    if (!query.exec())
        return SqlFailure(query, QStringLiteral("create tomb message"));

    const qint64 id = query.lastInsertId().toLongLong();
    qInfo().noquote() << "[BLOODBORNE TOMB CREATE]"
                      << "user_id=" + QString::number(userId) << "id=" + QString::number(id)
                      << "area=" + QString::number(request.value(QStringLiteral("AreaId")).toInt())
                      << "region=" +
                             QString::number(request.value(QStringLiteral("AreaRegionId")).toInt())
                      << "tomb_bytes=" + QString::number(tombData.size())
                      << "death_vision_bytes=" + QString::number(deathVisionData.size())
                      << "death_vision_sha256=" + Hash(deathVisionData);
    OnlineResult result = Success("TombMessCreateResponse");
    result.response.insert(QStringLiteral("TombMessId"), id);
    return result;
}

OnlineResult OnlineService::GetTombMessages(qint64 userId, const QJsonObject& request) {
    if (!IsArray(request, "AreaInfoList") || !IsInteger(request, "GetMaxCount", 0, MaxListItems) ||
        !IsInteger(request, "MessShellInfoVersion", 0, 1000) ||
        !IsInteger(request, "TombDataVersion", 0, 1000)) {
        return Failure(OnlineError::InvalidRequest,
                       QStringLiteral("invalid TombMessGetListRequest"));
    }
    const QJsonArray areas = request.value(QStringLiteral("AreaInfoList")).toArray();
    for (const QJsonValue& value : areas) {
        if (!value.isObject() || !ValidArea(value.toObject()))
            return Failure(OnlineError::InvalidRequest, QStringLiteral("invalid AreaInfoList"));
    }

    const int maximum = request.value(QStringLiteral("GetMaxCount")).toInt();
    QJsonArray messages;
    QSet<qint64> seen;
    for (const QJsonValue& value : areas) {
        if (messages.size() >= maximum)
            break;
        const QJsonObject area = value.toObject();
        const int areaLimit = std::min(area.value(QStringLiteral("GetCount")).toInt(),
                                       maximum - static_cast<int>(messages.size()));
        if (areaLimit <= 0)
            continue;
        QSqlQuery query(m_db.Conn());
        query.prepare(QStringLiteral(
            "SELECT t.tomb_mess_id, t.tomb_data, s.shell_data FROM "
            "bloodborne_tomb_message t JOIN bloodborne_messenger_shell s "
            "ON s.user_id=t.owner_user_id WHERE t.owner_user_id<>? AND t.area_id=? "
            "AND t.area_region_id=? AND t.channel_id=? AND t.tomb_data_version=? "
            "AND s.shell_data_version=? ORDER BY t.created_at DESC, t.tomb_mess_id DESC LIMIT ?"));
        query.addBindValue(userId);
        query.addBindValue(area.value(QStringLiteral("AreaId")).toVariant());
        query.addBindValue(area.value(QStringLiteral("AreaRegionId")).toVariant());
        query.addBindValue(area.value(QStringLiteral("ChannelId")).toVariant());
        query.addBindValue(request.value(QStringLiteral("TombDataVersion")).toInt());
        query.addBindValue(request.value(QStringLiteral("MessShellInfoVersion")).toInt());
        query.addBindValue(areaLimit);
        if (!query.exec())
            return SqlFailure(query, QStringLiteral("get tomb messages"));
        while (query.next() && messages.size() < maximum) {
            const qint64 id = query.value(0).toLongLong();
            if (seen.contains(id))
                continue;
            seen.insert(id);
            QJsonObject item;
            item.insert(QStringLiteral("MessShellInfo"), query.value(2).toString());
            item.insert(QStringLiteral("TombData"), query.value(1).toString());
            item.insert(QStringLiteral("TombMessId"), id);
            messages.append(item);
        }
    }

    qInfo().noquote() << "[BLOODBORNE TOMB GET]"
                      << "user_id=" + QString::number(userId)
                      << "count=" + QString::number(messages.size());
    OnlineResult result = Success("TombMessGetListResponse");
    result.response.insert(QStringLiteral("TombMessList"), messages);
    return result;
}

OnlineResult OnlineService::GetDeathVision(qint64 userId, const QJsonObject& request) {
    const auto id = SafeId(request.value(QStringLiteral("TombMessId")));
    if (!id || !IsInteger(request, "DeathVisionDataVersion", 0, 1000))
        return Failure(OnlineError::InvalidRequest,
                       QStringLiteral("invalid DeathVisionGetRequest"));
    QSqlQuery query(m_db.Conn());
    query.prepare(QStringLiteral(
        "SELECT death_vision_data, death_vision_data_version FROM bloodborne_tomb_message "
        "WHERE tomb_mess_id=? AND death_vision_data_version=?"));
    query.addBindValue(*id);
    query.addBindValue(request.value(QStringLiteral("DeathVisionDataVersion")).toInt());
    if (!query.exec())
        return SqlFailure(query, QStringLiteral("get death vision"));
    if (!query.next())
        return Failure(OnlineError::NotFound, QStringLiteral("death vision does not exist"));

    const QString encoded = query.value(0).toString();
    const QByteArray decoded = QByteArray::fromBase64(encoded.toLatin1());
    qInfo().noquote() << "[BLOODBORNE DEATH VISION GET]"
                      << "user_id=" + QString::number(userId) << "id=" + QString::number(*id)
                      << "blob_bytes=" + QString::number(decoded.size())
                      << "sha256=" + Hash(decoded);
    OnlineResult result = Success("DeathVisionGetResponse");
    result.response.insert(QStringLiteral("DeathVisionData"), encoded);
    result.response.insert(QStringLiteral("DeathVisionDataVersion"), query.value(1).toInt());
    return result;
}

OnlineResult OnlineService::CreateWanderingGhost(qint64 userId, const QJsonObject& request) {
    QByteArray decoded;
    if (!IsInteger(request, "AreaId", 0, 0xFFFFFFFFLL) ||
        !IsInteger(request, "AreaRegionId", 0, 0x7FFFFFFFLL) ||
        !IsInteger(request, "ChannelId", 0, 0x7FFFFFFFLL) || !IsNumber(request, "CharaId") ||
        !IsInteger(request, "MatchingLevel", -1, 9999) ||
        !IsInteger(request, "RejectIgnore", 0, 1) ||
        !IsInteger(request, "WanderingGhostDataVersion", 0, 1000) ||
        !ValidBase64(request, "WanderingGhostData", &decoded)) {
        return Failure(OnlineError::InvalidRequest,
                       QStringLiteral("invalid WanderingGhostCreateRequest"));
    }
    if (!PurgeExpiredGhosts(m_db.Conn()))
        return Failure(OnlineError::Database, QStringLiteral("purge wandering ghosts"));

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    QSqlQuery query(m_db.Conn());
    query.prepare(QStringLiteral(
        "INSERT INTO bloodborne_wandering_ghost(owner_user_id, owner_chara_id, area_id, "
        "area_region_id, channel_id, matching_level, reject_ignore, wandering_ghost_data, "
        "wandering_ghost_data_version, created_at, expires_at) VALUES(?,?,?,?,?,?,?,?,?,?,?)"));
    query.addBindValue(userId);
    query.addBindValue(request.value(QStringLiteral("CharaId")).toDouble());
    query.addBindValue(request.value(QStringLiteral("AreaId")).toVariant());
    query.addBindValue(request.value(QStringLiteral("AreaRegionId")).toVariant());
    query.addBindValue(request.value(QStringLiteral("ChannelId")).toVariant());
    query.addBindValue(request.value(QStringLiteral("MatchingLevel")).toInt());
    query.addBindValue(request.value(QStringLiteral("RejectIgnore")).toInt());
    query.addBindValue(request.value(QStringLiteral("WanderingGhostData")).toString());
    query.addBindValue(request.value(QStringLiteral("WanderingGhostDataVersion")).toInt());
    query.addBindValue(now);
    query.addBindValue(now + m_ghostLifetimeSeconds);
    if (!query.exec())
        return SqlFailure(query, QStringLiteral("create wandering ghost"));

    const qint64 id = query.lastInsertId().toLongLong();
    qInfo().noquote() << "[BLOODBORNE GHOST CREATE]"
                      << "user_id=" + QString::number(userId) << "id=" + QString::number(id)
                      << "area=" + QString::number(request.value(QStringLiteral("AreaId")).toInt())
                      << "region=" +
                             QString::number(request.value(QStringLiteral("AreaRegionId")).toInt())
                      << "blob_bytes=" + QString::number(decoded.size())
                      << "sha256=" + Hash(decoded);
    OnlineResult result = Success("WanderingGhostCreateResponse");
    result.response.insert(QStringLiteral("WanderingGhostId"), id);
    return result;
}

OnlineResult OnlineService::GetWanderingGhosts(qint64 userId, const QJsonObject& request) {
    if (!IsArray(request, "AreaList") || !IsArray(request, "JoinedCharaIdList") ||
        !IsInteger(request, "GetMaxCount", 0, MaxListItems) ||
        !IsInteger(request, "MatchingLevel", -1, 9999) ||
        !IsInteger(request, "WanderingGhostDataVersion", 0, 1000)) {
        return Failure(OnlineError::InvalidRequest,
                       QStringLiteral("invalid WanderingGhostGetRequest"));
    }
    const QJsonArray areas = request.value(QStringLiteral("AreaList")).toArray();
    for (const QJsonValue& value : areas) {
        if (!value.isObject() || !ValidArea(value.toObject()))
            return Failure(OnlineError::InvalidRequest, QStringLiteral("invalid AreaList"));
    }
    QSet<QString> joinedCharaIds;
    for (const QJsonValue& value : request.value(QStringLiteral("JoinedCharaIdList")).toArray()) {
        if (!value.isDouble() || !std::isfinite(value.toDouble()))
            return Failure(OnlineError::InvalidRequest,
                           QStringLiteral("invalid JoinedCharaIdList"));
        joinedCharaIds.insert(NumberKey(value.toDouble()));
    }
    if (!PurgeExpiredGhosts(m_db.Conn()))
        return Failure(OnlineError::Database, QStringLiteral("purge wandering ghosts"));

    const int maximum = request.value(QStringLiteral("GetMaxCount")).toInt();
    const int requestedLevel = request.value(QStringLiteral("MatchingLevel")).toInt();
    QJsonArray ghosts;
    QSet<qint64> seen;
    for (const QJsonValue& value : areas) {
        if (ghosts.size() >= maximum)
            break;
        const QJsonObject area = value.toObject();
        const int areaLimit = std::min(area.value(QStringLiteral("GetCount")).toInt(),
                                       maximum - static_cast<int>(ghosts.size()));
        if (areaLimit <= 0)
            continue;
        const bool wildcard = area.value(QStringLiteral("AreaId")).toDouble() == 4294967295.0 &&
                              area.value(QStringLiteral("AreaRegionId")).toInt() == -1;
        QString sql = QStringLiteral(
            "SELECT wandering_ghost_id, owner_chara_id, wandering_ghost_data, "
            "wandering_ghost_data_version FROM bloodborne_wandering_ghost "
            "WHERE owner_user_id<>? AND expires_at>? AND wandering_ghost_data_version=? ");
        if (!wildcard)
            sql += QStringLiteral("AND area_id=? AND area_region_id=? AND channel_id=? ");
        if (requestedLevel >= 0)
            sql += QStringLiteral("AND matching_level=? ");
        sql += QStringLiteral("ORDER BY created_at DESC, wandering_ghost_id DESC LIMIT ?");

        QSqlQuery query(m_db.Conn());
        query.prepare(sql);
        query.addBindValue(userId);
        query.addBindValue(QDateTime::currentSecsSinceEpoch());
        query.addBindValue(request.value(QStringLiteral("WanderingGhostDataVersion")).toInt());
        if (!wildcard) {
            query.addBindValue(area.value(QStringLiteral("AreaId")).toVariant());
            query.addBindValue(area.value(QStringLiteral("AreaRegionId")).toVariant());
            query.addBindValue(area.value(QStringLiteral("ChannelId")).toVariant());
        }
        if (requestedLevel >= 0)
            query.addBindValue(requestedLevel);
        query.addBindValue(areaLimit + static_cast<int>(joinedCharaIds.size()));
        if (!query.exec())
            return SqlFailure(query, QStringLiteral("get wandering ghosts"));
        int areaCount = 0;
        while (query.next() && ghosts.size() < maximum && areaCount < areaLimit) {
            const qint64 id = query.value(0).toLongLong();
            if (seen.contains(id) || joinedCharaIds.contains(NumberKey(query.value(1).toDouble())))
                continue;
            seen.insert(id);
            QJsonObject item;
            item.insert(QStringLiteral("WanderingGhostData"), query.value(2).toString());
            item.insert(QStringLiteral("WanderingGhostDataVersion"), query.value(3).toInt());
            item.insert(QStringLiteral("WanderingGhostId"), id);
            ghosts.append(item);
            ++areaCount;
        }
    }

    qInfo().noquote() << "[BLOODBORNE GHOST GET]"
                      << "user_id=" + QString::number(userId)
                      << "count=" + QString::number(ghosts.size());
    OnlineResult result = Success("WanderingGhostGetResponse");
    result.response.insert(QStringLiteral("WanderingGhostList"), ghosts);
    return result;
}

} // namespace Bloodborne
