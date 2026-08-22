// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include "bloodborne_bootstrap.h"

#include <QJsonArray>
#include <QJsonObject>

namespace Bloodborne {
namespace {

constexpr std::array<BootstrapApi, 37> ApiTable{{
    {"api_Login", "/basic_utils/login"},
    {"api_ServerTimeGet", "/basic_utils/get_datetime"},
    {"api_SyncCharaId", "/basic_utils/sync_chara_id"},
    {"api_NoticeNormalGet", "/basic_utils/get_normal_notice"},
    {"api_NoticeEmergencyGet", "/basic_utils/get_emergency_notice"},
    {"api_UserAgreementGet", "/basic_utils/get_user_agreement"},
    {"api_BloodMessCreate", "/blood_messenger/create"},
    {"api_BloodMessGetList", "/blood_messenger/message_area"},
    {"api_BloodMessEvaluate", "/blood_messenger/evaluate_message"},
    {"api_BloodMessGetEvaluate", "/blood_messenger/evaluation"},
    {"api_BloodMessRemove", "/blood_messenger/delete"},
    {"api_BloodMessSearchAdd", "/blood_messenger/exist_messages"},
    {"api_ChannelUpload", "/channel/upload"},
    {"api_ChannelShare", "/channel/share"},
    {"api_ChannelSearch", "/channel/search"},
    {"api_ChannelWordSearch", "/channel/word_search"},
    {"api_ChannelGetDetailsInfo", "/channel/get_details_info"},
    {"api_ChannelGetInfo", "/channel/get_info"},
    {"api_ChannelRandomJoin", "/channel/random_join"},
    {"api_ChannelAddMaterial", "/channel/add_material"},
    {"api_ChannelAddMaterialCompleteNotify", "/channel/notify_add_material_complete"},
    {"api_MessengerShellUpload", "/messenger_shell/upload"},
    {"api_MultiPlayNetError", "/penalty/notify_multi_play_error"},
    {"api_UserPropertiesMoveCount", "/basic_utils/notify_user_properties_move_count"},
    {"api_UserPropertiesMoveCountCheck", "/basic_utils/check_user_priority_move_count"},
    {"api_SummonDataCreate", "/summon_messenger/create"},
    {"api_SummonDataGetList", "/summon_messenger/get"},
    {"api_SummonDataRemove", "/summon_messenger/delete"},
    {"api_SummonDataSummon", "/summon_messenger/request"},
    {"api_ChairMessGetList", "/chair_messenger/get"},
    {"api_ChairMessRespawnPointNotice", "/chair_messenger/update"},
    {"api_TombMessCreate", "/tomb_messenger/create"},
    {"api_TombMessGetList", "/tomb_messenger/message_area"},
    {"api_DeathVisionGet", "/tomb_messenger/death_vision_get"},
    {"api_TombMessRemove", "/tomb_messenger/delete"},
    {"api_WanderingGhostCreate", "/wandering_ghost/create"},
    {"api_WanderingGhostGet", "/wandering_ghost/get"},
}};

} // namespace

const std::array<BootstrapApi, 37>& BootstrapApis() {
    return ApiTable;
}

QByteArray BuildServerStatusInfo(const QString& publicBaseUrl) {
    QString baseUrl = publicBaseUrl.trimmed();
    while (baseUrl.endsWith('/')) {
        baseUrl.chop(1);
    }

    QByteArray body("<ss>0</ss>\n<gameurl2>\n");
    for (const BootstrapApi& api : ApiTable) {
        body += '<';
        body += api.name;
        body += '>';
        body += baseUrl.toUtf8();
        body += api.path;
        body += "</";
        body += api.name;
        body += ">\n";
    }
    body += "</gameurl2>\n";
    return body;
}

QJsonObject BuildLoginResponse(qint64 userId, int languageId, const QString& sessionId) {
    QJsonObject body;
    body.insert(QStringLiteral("ResKind"), 0);
    body.insert(QStringLiteral("UserId"), userId);
    body.insert(QStringLiteral("UserStatus"), 0);
    body.insert(QStringLiteral("LanguageId"), languageId);
    body.insert(QStringLiteral("SessionId"), sessionId);
    body.insert(QStringLiteral("ServerVersion"), 6);
    return body;
}

QJsonObject BuildServerTimeResponse() {
    QJsonObject body;
    body.insert(QStringLiteral("ResKind"), 0);
    return body;
}

QJsonObject BuildNoticeNormalResponse() {
    QJsonObject body;
    body.insert(QStringLiteral("ResKind"), 0);
    body.insert(QStringLiteral("NoticeList"), QJsonArray{});
    return body;
}

QJsonObject BuildNoticeEmergencyResponse(const QString& checkTime) {
    QJsonObject body;
    body.insert(QStringLiteral("ResKind"), 0);
    body.insert(QStringLiteral("CheckTime"), checkTime);
    body.insert(QStringLiteral("NoticeList"), QJsonArray{});
    return body;
}

QJsonObject BuildSyncCharaIdResponse(const QList<qint64>& charaIds) {
    QJsonArray ids;
    for (const qint64 charaId : charaIds) {
        QJsonObject item;
        item.insert(QStringLiteral("PublishCharaId"), charaId);
        ids.append(item);
    }

    QJsonObject body;
    body.insert(QStringLiteral("ResKind"), 0);
    body.insert(QStringLiteral("PublishCharacterIdList"), ids);
    return body;
}

} // namespace Bloodborne
