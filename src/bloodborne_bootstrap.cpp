// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include "bloodborne_bootstrap.h"

#include <QJsonArray>
#include <QJsonObject>

#include "bloodborne_ssinfo_reference.h"

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
    {"api_UserPropertiesMoveCountCheck", "/penalty/check_user_priority_move_count"},
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

[[maybe_unused]] constexpr auto ServerStatusPrefix = R"ss(<ss>0</ss>
<msg2>
<lang3></lang3>
<lang4></lang4>
<lang5></lang5>
<lang6></lang6>
<lang7></lang7>
<lang8></lang8>
<lang9></lang9>
<lang13></lang13>
<lang14></lang14>
<lang15></lang15>
<lang16></lang16>
<lang17></lang17>
<lang19></lang19>
<lang20></lang20>
</msg2>

<BetaTestWebUrl2>
<lang3></lang3>
<lang4></lang4>
<lang5></lang5>
<lang6></lang6>
<lang7></lang7>
<lang8></lang8>
<lang9></lang9>
<lang13></lang13>
<lang14></lang14>
<lang15></lang15>
<lang16></lang16>
<lang17></lang17>
<lang19></lang19>
<lang20></lang20>
</BetaTestWebUrl2>

<gameurl2>
)ss";

[[maybe_unused]] constexpr auto ServerStatusSettings = R"ss(</gameurl2>

<ReloadServerStatusInfoInterval2>300</ReloadServerStatusInfoInterval2>
<SummonDataCreateInterval2>30</SummonDataCreateInterval2>
<SummonDataGetListInterval2>35</SummonDataGetListInterval2>
<SummonDataGetListGuardTimer2>60</SummonDataGetListGuardTimer2>
<SummonDataGetListNomalCoopWaitCount2>1</SummonDataGetListNomalCoopWaitCount2>
<SummonDataGetListCombinationInvasionWaitCount2>2</SummonDataGetListCombinationInvasionWaitCount2>
<SummonDataGetListNaturalEnemyWaitCountFixed2>4</SummonDataGetListNaturalEnemyWaitCountFixed2>
<SummonDataGetListAllAreaMethodWaitCount2>4</SummonDataGetListAllAreaMethodWaitCount2>
<SummonDataGetListGetCountPerSummonType2>5</SummonDataGetListGetCountPerSummonType2>
<SummonDataGetListGetMaxCount2>20</SummonDataGetListGetMaxCount2>
<SummonDataCreateNaturalEnemyPercentFixed2>100</SummonDataCreateNaturalEnemyPercentFixed2>
<SummonDataBloodMadHunterPercent2>5</SummonDataBloodMadHunterPercent2>
<SummonDataGetListCoopGuardTimerClampTime2>60</SummonDataGetListCoopGuardTimerClampTime2>
<SummonDataGetListInvationGuardTimerClampTime2>0</SummonDataGetListInvationGuardTimerClampTime2>
<SummonDataGetListCoopGuardTimerLimitationTimeRate2>100</SummonDataGetListCoopGuardTimerLimitationTimeRate2>
<SummonDataGetListCoopGuardTimerObserveTimeRate2>100</SummonDataGetListCoopGuardTimerObserveTimeRate2>
<SummonDataGetListInvationGuardTimerLimitationTimeRate2>100</SummonDataGetListInvationGuardTimerLimitationTimeRate2>
<SummonDataGetListInvationGuardTimerObserveTimeRate2>100</SummonDataGetListInvationGuardTimerObserveTimeRate2>
<SummonDataCoopMatchingLevelUpperAbs2>20</SummonDataCoopMatchingLevelUpperAbs2>
<SummonDataCoopMatchingLevelUpperRel2>20</SummonDataCoopMatchingLevelUpperRel2>
<SummonDataCoopMatchingLevelLowerAbs2>-20</SummonDataCoopMatchingLevelLowerAbs2>
<SummonDataCoopMatchingLevelLowerRel2>-20</SummonDataCoopMatchingLevelLowerRel2>
<TombMessGetListInterval2>120</TombMessGetListInterval2>
<TombMessGetListGetCountPerArea2>30</TombMessGetListGetCountPerArea2>
<TombMessGetListGetMaxCount2>100</TombMessGetListGetMaxCount2>
<TombMessGetEvaluateInterval2>600</TombMessGetEvaluateInterval2>
<BloodMessGetListInterval2>120</BloodMessGetListInterval2>
<BloodMessGetListGetCountPerArea2>30</BloodMessGetListGetCountPerArea2>
<BloodMessGetListGetMaxCount2>100</BloodMessGetListGetMaxCount2>
<BloodMessGetEvaluateInterval2>600</BloodMessGetEvaluateInterval2>
<WanderingGhostCreateInterval2>60</WanderingGhostCreateInterval2>
<WanderingGhostGetInterval2>120</WanderingGhostGetInterval2>
<WanderingGhostGetCountPerArea2>5</WanderingGhostGetCountPerArea2>
<WanderingGhostGetMaxCount2>10</WanderingGhostGetMaxCount2>
<ChairMessRespawnPointNoticeInterval2>600</ChairMessRespawnPointNoticeInterval2>
<ChairMessRespawnPointNoticeWaitTime2>300</ChairMessRespawnPointNoticeWaitTime2>
<ChairMessGetListInterval2>300</ChairMessGetListInterval2>
<ChannelGetInfoInterval2>600</ChannelGetInfoInterval2>
<NoticeEmergencyGetInterval2>60</NoticeEmergencyGetInterval2>
<MessengerShellUploadInterval2>600</MessengerShellUploadInterval2>

<PlayLog2>1</PlayLog2>
<Playlog_SeriousPlaylogInfo2>0</Playlog_SeriousPlaylogInfo2>
<Playlog_ChangeMapInfo2>0</Playlog_ChangeMapInfo2>
<Playlog_ChrDead2>0</Playlog_ChrDead2>
<Playlog_ChrFallDead2>0</Playlog_ChrFallDead2>
<Playlog_ChrActionMagic2>0</Playlog_ChrActionMagic2>
<Playlog_ChrChangeMagic2>0</Playlog_ChrChangeMagic2>
<Playlog_ChrFallDamage2>0</Playlog_ChrFallDamage2>
<Playlog_ChrAttackDamage2>0</Playlog_ChrAttackDamage2>
<Playlog_ChrAttack2>0</Playlog_ChrAttack2>
<Playlog_ChrAttackHitMap2>0</Playlog_ChrAttackHitMap2>
<Playlog_ChrThrow2>0</Playlog_ChrThrow2>
<Playlog_ChrGuard2>0</Playlog_ChrGuard2>
<Playlog_ChrUseItem2>0</Playlog_ChrUseItem2>
<Playlog_PickUpItem2>0</Playlog_PickUpItem2>
<Playlog_SetEquipItem2>0</Playlog_SetEquipItem2>
<Playlog_AddEquipItem2>0</Playlog_AddEquipItem2>
<Playlog_CommonInfo2>0</Playlog_CommonInfo2>
<Playlog_ChrRolling2>0</Playlog_ChrRolling2>
<Playlog_ChrBackStep2>0</Playlog_ChrBackStep2>
<Playlog_ChrDashJump2>0</Playlog_ChrDashJump2>
<Playlog_ChrLadderUp2>0</Playlog_ChrLadderUp2>
<Playlog_ChrLadderDown2>0</Playlog_ChrLadderDown2>
<Playlog_ChrTalk2>0</Playlog_ChrTalk2>
<Playlog_RegularLog2>0</Playlog_RegularLog2>
<Playlog_Enemy_RegularLog2>0</Playlog_Enemy_RegularLog2>
<Playlog_EventLog2>0</Playlog_EventLog2>
<Playlog_MeasureTime2>0</Playlog_MeasureTime2>
<Playlog_BreakObj2>0</Playlog_BreakObj2>
<Playlog_PutBloodMessageMessenger2>0</Playlog_PutBloodMessageMessenger2>
<Playlog_PutBloodStainMessenger2>0</Playlog_PutBloodStainMessenger2>
<Playlog_CheckBloodMessageMessenger2>0</Playlog_CheckBloodMessageMessenger2>
<Playlog_CheckBloodStainMessenger2>0</Playlog_CheckBloodStainMessenger2>
<Playlog_ChrChangeWeapon2>0</Playlog_ChrChangeWeapon2>
<Playlog_ChrSwitchWeapon2>0</Playlog_ChrSwitchWeapon2>
<Playlog_ChrDestroyPart2>0</Playlog_ChrDestroyPart2>
<Playlog_ObjActEvoke2>0</Playlog_ObjActEvoke2>
<Playlog_AIRecogEnemy2>0</Playlog_AIRecogEnemy2>
<Playlog_AIRecogSound2>0</Playlog_AIRecogSound2>
<Playlog_CreateNewProfileData2>0</Playlog_CreateNewProfileData2>
<Playlog_ChrHeal2>0</Playlog_ChrHeal2>
<Playlog_ChrGuardBreak2>0</Playlog_ChrGuardBreak2>
<Playlog_ChrBreakDown2>0</Playlog_ChrBreakDown2>
<Playlog_EvaluateBloodMessageMessenger2>0</Playlog_EvaluateBloodMessageMessenger2>
<Playlog_EvaluateBloodStainMessenger2>0</Playlog_EvaluateBloodStainMessenger2>
<Playlog_MatchingLog2>0</Playlog_MatchingLog2>
<Playlog_MatchingFailureLog2>0</Playlog_MatchingFailureLog2>
<Playlog_PcPrimaryParam2>0</Playlog_PcPrimaryParam2>
<Playlog_PcTempParam2>0</Playlog_PcTempParam2>
<Playlog_PcWeaponParam2>0</Playlog_PcWeaponParam2>
<Playlog_PcArmorParam2>0</Playlog_PcArmorParam2>
<Playlog_Host_SendGetListRequest2>1</Playlog_Host_SendGetListRequest2>
<Playlog_Host_ReceiveGetListResponse2>1</Playlog_Host_ReceiveGetListResponse2>
<Playlog_Host_GuestInBlockList2>1</Playlog_Host_GuestInBlockList2>
<Playlog_Host_SendSummonRequest2>1</Playlog_Host_SendSummonRequest2>
<Playlog_Host_ReceiveSummonResponse2>1</Playlog_Host_ReceiveSummonResponse2>
<Playlog_Host_SendCreateSessionRequest2>1</Playlog_Host_SendCreateSessionRequest2>
<Playlog_Host_ReceiveCreateSessionResponse2>1</Playlog_Host_ReceiveCreateSessionResponse2>
<Playlog_Host_SessionTimeout2>1</Playlog_Host_SessionTimeout2>
<Playlog_Host_SendP2PhandShakeRequest2>1</Playlog_Host_SendP2PhandShakeRequest2>
<Playlog_Host_RejectedP2PhandShake2>1</Playlog_Host_RejectedP2PhandShake2>
<Playlog_Host_NotComeInHostWroldTimeout2>1</Playlog_Host_NotComeInHostWroldTimeout2>
<Playlog_Host_StartMultiPlay2>1</Playlog_Host_StartMultiPlay2>
<Playlog_Guest_SendCreateRequest2>1</Playlog_Guest_SendCreateRequest2>
<Playlog_Guest_ReceiveCreateResponse2>1</Playlog_Guest_ReceiveCreateResponse2>
<Playlog_Guest_ReceiveHandShakeRequest2>1</Playlog_Guest_ReceiveHandShakeRequest2>
<Playlog_Guest_SendHandShakeResponse2>1</Playlog_Guest_SendHandShakeResponse2>
<Playlog_Guest_SendJoinSessionRequest2>1</Playlog_Guest_SendJoinSessionRequest2>
<Playlog_Guest_ReceiveJoinSessionResponse2>1</Playlog_Guest_ReceiveJoinSessionResponse2>
<Playlog_Guest_StartMultiPlay2>1</Playlog_Guest_StartMultiPlay2>
)ss";

} // namespace

const std::array<BootstrapApi, 37>& BootstrapApis() {
    return ApiTable;
}

QByteArray BuildServerStatusInfo(const QString& publicBaseUrl, QByteArray* decodedXml,
                                 QString* error) {
    QString baseUrl = publicBaseUrl.trimmed();
    while (baseUrl.endsWith('/')) {
        baseUrl.chop(1);
    }

    const auto fail = [error](const QString& message) {
        if (error != nullptr)
            *error = message;
        return QByteArray{};
    };
    if (baseUrl.isEmpty())
        return fail(QStringLiteral("BloodbornePublicBaseUrl is empty"));

    QByteArray xml;
    QString referenceError;
    if (!ValidateReferenceServerStatusInfo(&referenceError, &xml))
        return fail(QStringLiteral("reference template is invalid: %1").arg(referenceError));

    const QByteArray encodedBaseUrl = baseUrl.toUtf8();
    for (const BootstrapApi& api : ApiTable) {
        const QByteArray opening = QByteArray("<") + api.name + ">";
        const QByteArray closing = QByteArray("</") + api.name + ">";
        const qsizetype openingOffset = xml.indexOf(opening);
        if (openingOffset < 0 || xml.indexOf(opening, openingOffset + opening.size()) >= 0)
            return fail(QStringLiteral("template does not contain exactly one %1")
                            .arg(QString::fromLatin1(opening)));
        const qsizetype valueOffset = openingOffset + opening.size();
        const qsizetype closingOffset = xml.indexOf(closing, valueOffset);
        if (closingOffset < valueOffset)
            return fail(QStringLiteral("template is missing %1").arg(QString::fromLatin1(closing)));
        xml.replace(valueOffset, closingOffset - valueOffset, encodedBaseUrl);
    }

    if (!xml.startsWith("<ss>0</ss>") || !xml.contains("<gameurl2>") ||
        xml.count("<api_") != static_cast<qsizetype>(ApiTable.size())) {
        return fail(QStringLiteral("generated decoded XML failed structural validation"));
    }
    if (xml.contains("thehuntersdream.com:18671"))
        return fail(QStringLiteral("generated decoded XML still contains the reference upstream"));

    for (const BootstrapApi& api : ApiTable) {
        const QByteArray opening = QByteArray("<") + api.name + ">";
        const QByteArray closing = QByteArray("</") + api.name + ">";
        if (!xml.contains(opening + encodedBaseUrl + closing)) {
            return fail(QStringLiteral("generated value for %1 is invalid")
                            .arg(QString::fromLatin1(api.name)));
        }
    }

    const QByteArray encoded = xml.toBase64();
    const auto decodedCheck =
        QByteArray::fromBase64Encoding(encoded, QByteArray::AbortOnBase64DecodingErrors);
    if (!decodedCheck || decodedCheck.decoded != xml)
        return fail(QStringLiteral("generated HTTP body failed strict Base64 round-trip"));

    if (decodedXml != nullptr)
        *decodedXml = xml;
    if (error != nullptr)
        error->clear();
    return encoded;
}

QJsonObject BuildLoginResponse(qint64 userId, int languageId, const QString& sessionId) {
    QJsonObject body;
    body.insert(QStringLiteral("MessageId"), QStringLiteral("LoginResponse"));
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
    body.insert(QStringLiteral("MessageId"), QStringLiteral("NoticeNormalGetResponse"));
    body.insert(QStringLiteral("ResKind"), 0);
    body.insert(QStringLiteral("NoticeList"), QJsonArray{});
    return body;
}

QJsonObject BuildNoticeEmergencyResponse(const QString& checkTime) {
    QJsonObject body;
    body.insert(QStringLiteral("MessageId"), QStringLiteral("NoticeEmergencyGetResponse"));
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
