// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include <cstdlib>
#include <iostream>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include "bloodborne_summon_broker.h"

namespace {

QJsonObject Parse(const QByteArray& raw) {
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(raw, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        std::cerr << "JSON parse failed: " << error.errorString().toStdString() << '\n';
        std::exit(1);
    }
    return document.object();
}

bool Check(bool condition, const char* expression, int line) {
    if (!condition) {
        std::cerr << "check failed at line " << line << ": " << expression << '\n';
    }
    return condition;
}

#define CHECK(expression)                                                                           \
    do {                                                                                            \
        if (!Check((expression), #expression, __LINE__))                                           \
            return 1;                                                                               \
    } while (false)

} // namespace

int main() {
    const QByteArray advertisement = R"({"MessageId":"SummonDataCreateRequest","SessionId":"guest-session","UserId":2465,"CharaId":9223372036854775808,"AreaId":385875968,"AreaRegionId":230100,"ChannelId":0,"MatchingLevel":46,"SummonData":"opaque-game-data","SummonDataVersion":3,"SummonMethod":0,"SummonType":0,"SummonWord":null,"PosX":143,"PosY":-116,"PosZ":-87})";
    const QByteArray search = R"({"MessageId":"SummonDataGetListRequest","SessionId":"host-session","UserId":2466,"AreaId":385875968,"AreaRegionId":230100,"ChannelId":0,"MatchingLevel":46,"SummonDataVersion":3,"SummonMethod":0,"SummonTypeList":[{"SummonType":0}],"SummonWord":null,"DistanceThreshold":100,"GetMaxCount":20,"PosX":143,"PosY":-116,"PosZ":-87})";
    const QByteArray claim = R"({"MessageId":"SummonDataSummonRequest","SessionId":"guest-session","UserId":2466,"TargetUserId":2465,"TargetCharaId":9223372036854775808,"HostData":"host-owned-data","ResKind":99})";
    const QByteArray conflictingClaim = R"({"MessageId":"SummonDataSummonRequest","SessionId":"guest-session","UserId":2467,"TargetUserId":2465})";
    const QByteArray targetUserClaim = R"({"MessageId":"SummonDataSummonRequest","SessionId":"host-owned-session","UserId":2466,"TargetUserId":2465,"TargetCharaId":9223372036854775808})";
    const QByteArray removal =
        R"({"MessageId":"SummonDataRemoveRequest","SessionId":"guest-session","UserId":2465})";

    Bloodborne::SummonBroker broker(1'000);
    auto advertised = broker.Advertise(Parse(advertisement), advertisement, 100);
    CHECK(advertised.state == Bloodborne::SummonBroker::State::Advertised);
    CHECK(advertised.pendingClaim.isEmpty());
    CHECK(broker.StateFor(QStringLiteral("guest-session"), 2465, 100) ==
          Bloodborne::SummonBroker::State::Advertised);

    const QList<QByteArray> found = broker.Search(Parse(search), 110);
    CHECK(found.size() == 1);
    CHECK(found.front() == advertisement);

    const auto claimed = broker.Claim(Parse(claim), claim, 120);
    CHECK(claimed.status == Bloodborne::SummonBroker::ClaimStatus::Claimed);
    CHECK(claimed.targetSessionId == QStringLiteral("guest-session"));
    CHECK(claimed.targetUserId == 2465);
    CHECK(broker.Search(Parse(search), 121).isEmpty());
    CHECK(broker.StateFor(QStringLiteral("guest-session"), 2465, 121) ==
          Bloodborne::SummonBroker::State::Claimed);

    const auto repeatedClaim = broker.Claim(Parse(claim), claim, 122);
    CHECK(repeatedClaim.status == Bloodborne::SummonBroker::ClaimStatus::AlreadyClaimed);
    const auto conflict = broker.Claim(Parse(conflictingClaim), conflictingClaim, 123);
    CHECK(conflict.status == Bloodborne::SummonBroker::ClaimStatus::Conflict);

    const auto delivered = broker.Advertise(Parse(advertisement), advertisement, 130);
    CHECK(delivered.state == Bloodborne::SummonBroker::State::Delivered);
    CHECK(delivered.pendingClaim == claim);
    CHECK(broker.StateFor(QStringLiteral("guest-session"), 2465, 130) ==
          Bloodborne::SummonBroker::State::Delivered);

    const QByteArray deliveryResponse =
        Bloodborne::BuildClaimDeliveryResponse(delivered.pendingClaim);
    CHECK(deliveryResponse.contains("\"MessageId\":\"SummonDataCreateResponse\""));
    CHECK(!deliveryResponse.contains("SummonDataSummonRequest"));
    CHECK(deliveryResponse.contains("\"ResKind\":0"));
    CHECK(!deliveryResponse.contains("\"ResKind\":99"));
    CHECK(deliveryResponse.contains("\"TargetCharaId\":9223372036854775808"));
    CHECK(deliveryResponse.contains("\"HostData\":\"host-owned-data\""));

    const auto redelivered = broker.Advertise(Parse(advertisement), advertisement, 140);
    CHECK(redelivered.state == Bloodborne::SummonBroker::State::Delivered);
    CHECK(redelivered.pendingClaim == claim);

    CHECK(broker.Consume(Parse(removal), 150) == 1);
    CHECK(broker.StateFor(QStringLiteral("guest-session"), 2465, 150) ==
          Bloodborne::SummonBroker::State::Consumed);
    CHECK(broker.Search(Parse(search), 151).isEmpty());

    const auto readvertised = broker.Advertise(Parse(advertisement), advertisement, 160);
    CHECK(readvertised.state == Bloodborne::SummonBroker::State::Advertised);
    CHECK(readvertised.pendingClaim.isEmpty());
    CHECK(broker.Search(Parse(search), 161).size() == 1);
    CHECK(broker.Consume(QJsonObject{}, 162) == 0);
    CHECK(broker.Search(Parse(search), 163).size() == 1);

    const auto claimedByTarget = broker.Claim(Parse(targetUserClaim), targetUserClaim, 170);
    CHECK(claimedByTarget.status == Bloodborne::SummonBroker::ClaimStatus::Claimed);
    CHECK(claimedByTarget.targetSessionId == QStringLiteral("guest-session"));
    CHECK(claimedByTarget.targetUserId == 2465);
    CHECK(broker.Size(1'171) == 0);

    std::cout << "Bloodborne summon broker state test passed\n";
    return 0;
}