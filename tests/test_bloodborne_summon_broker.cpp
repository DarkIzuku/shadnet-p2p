// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include <cstdlib>
#include <iostream>

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStringList>

#include "bloodborne_summon_broker.h"

namespace {

QJsonObject Parse(const QByteArray &raw) {
  QJsonParseError error{};
  const QJsonDocument document = QJsonDocument::fromJson(raw, &error);
  if (error.error != QJsonParseError::NoError || !document.isObject()) {
    std::cerr << "JSON parse failed: " << error.errorString().toStdString()
              << '\n';
    std::exit(1);
  }
  return document.object();
}

bool Check(bool condition, const char *expression, int line) {
  if (!condition) {
    std::cerr << "check failed at line " << line << ": " << expression << '\n';
  }
  return condition;
}

QStringList capturedMessages;

void CaptureMessage(QtMsgType, const QMessageLogContext &,
                    const QString &message) {
  capturedMessages.append(message);
}

QByteArray Compact(const QJsonObject &object) {
  return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

struct TraceRun {
  int found = 0;
  QString messages;
};

TraceRun RunTraceSearch(const QJsonObject &advertisement,
                        const QJsonObject &search, bool advertise = true) {
  Bloodborne::SummonBroker broker(1'000);
  if (advertise) {
    const QByteArray rawAdvertisement = Compact(advertisement);
    broker.Advertise(advertisement, rawAdvertisement, 100);
  }
  capturedMessages.clear();
  const int found = broker.Search(search, 110).size();
  return {found, capturedMessages.join(QLatin1Char('\n'))};
}

#define CHECK(expression)                                                      \
  do {                                                                         \
    if (!Check((expression), #expression, __LINE__))                           \
      return 1;                                                                \
  } while (false)

} // namespace

int main() {
  qunsetenv("SHADNET_BLOODBORNE_SUMMON_TRACE");
  const QByteArray advertisement =
      R"({"MessageId":"SummonDataCreateRequest","SessionId":"guest-session","UserId":2465,"CharaId":9223372036854775808,"AreaId":385875968,"AreaRegionId":230100,"ChannelId":0,"MatchingLevel":46,"SummonData":"opaque-game-data","SummonDataVersion":3,"SummonMethod":0,"SummonType":0,"SummonWord":null,"PosX":143,"PosY":-116,"PosZ":-87})";
  const QByteArray remoteAdvertisement =
      R"({"MessageId":"SummonDataCreateRequest","SessionId":"remote-session","UserId":3000,"CharaId":9223372036854775808,"AreaId":111,"AreaRegionId":222,"ChannelId":9,"MatchingLevel":200,"SummonData":"remote-game-data","SummonDataVersion":3,"SummonMethod":0,"SummonType":0,"SummonWord":null,"PosX":999,"PosY":888,"PosZ":777})";
  const QByteArray search =
      R"({"MessageId":"SummonDataGetListRequest","SessionId":"host-session","UserId":2466,"AreaId":385875968,"AreaRegionId":230100,"ChannelId":0,"MatchingLevel":46,"SummonDataVersion":3,"SummonMethod":0,"SummonTypeList":[{"SummonType":0}],"SummonWord":null,"DistanceThreshold":100,"GetMaxCount":20,"PosX":143,"PosY":-116,"PosZ":-87})";
  const QByteArray claim =
      R"({"MessageId":"SummonDataSummonRequest","SessionId":"guest-session","UserId":2466,"TargetUserId":2465,"TargetCharaId":9223372036854775808,"HostData":"host-owned-data","ResKind":99})";
  const QByteArray remoteClaim =
      R"({"MessageId":"SummonDataSummonRequest","SessionId":"remote-session","UserId":2466,"TargetUserId":3000,"TargetCharaId":9223372036854775808,"HostData":"remote-host-owned-data","ResKind":99})";
  const QByteArray conflictingClaim =
      R"({"MessageId":"SummonDataSummonRequest","SessionId":"guest-session","UserId":2467,"TargetUserId":2465})";
  const QByteArray targetUserClaim =
      R"({"MessageId":"SummonDataSummonRequest","SessionId":"host-owned-session","UserId":2466,"TargetUserId":2465,"TargetCharaId":9223372036854775808})";
  const QByteArray removal =
      R"({"MessageId":"SummonDataRemoveRequest","SessionId":"guest-session","UserId":2465})";
  const QByteArray forceRemoval =
      R"({"MessageId":"SummonDataRemoveRequest","SessionId":"guest-session","UserId":2465,"Force":true})";
  const QByteArray hostPlacement =
      "1,17010000,43110000,c2e80000,42800000,c016cbe4,-241109";
  const QByteArray unrelatedSearch =
      R"({"MessageId":"SummonDataGetListRequest","SessionId":"other-session","UserId":2467,"AreaId":385875968,"AreaRegionId":230100,"ChannelId":0,"MatchingLevel":46,"SummonDataVersion":3,"SummonMethod":0,"SummonTypeList":[{"SummonType":0}],"SummonWord":null,"DistanceThreshold":100,"GetMaxCount":20,"PosX":143,"PosY":-116,"PosZ":-87})";

  Bloodborne::SummonBroker broker(1'000);
  auto advertised = broker.Advertise(Parse(advertisement), advertisement, 100);
  CHECK(advertised.state == Bloodborne::SummonBroker::State::Advertised);
  CHECK(advertised.pendingClaim.isEmpty());
  CHECK(broker.StateFor(QStringLiteral("guest-session"), 2465, 100) ==
        Bloodborne::SummonBroker::State::Advertised);

  const QList<QByteArray> found = broker.Search(Parse(search), 110);
  CHECK(found.size() == 1);
  CHECK(found.front() == advertisement);

  QByteArray summonPayload(0xE0, '\0');
  QByteArray binaryAdvertisementRaw = advertisement;
  binaryAdvertisementRaw.replace("opaque-game-data", summonPayload.toBase64());
  QJsonObject binaryAdvertisement = Parse(binaryAdvertisementRaw);
  Bloodborne::SummonBroker binaryBroker(1'000);
  CHECK(binaryBroker.Advertise(binaryAdvertisement, binaryAdvertisementRaw, 100)
            .state == Bloodborne::SummonBroker::State::Advertised);
  QList<QByteArray> binaryFound = binaryBroker.Search(Parse(search), 110);
  CHECK(binaryFound.size() == 1);
  QByteArray returnedPayload =
      QByteArray::fromBase64(Parse(binaryFound.front())
                                 .value(QStringLiteral("SummonData"))
                                 .toString()
                                 .toLatin1());
  CHECK(returnedPayload.size() == summonPayload.size());
  CHECK(static_cast<unsigned char>(returnedPayload[0x79]) == 1);
  CHECK(static_cast<unsigned char>(summonPayload[0x79]) == 0);
  CHECK(binaryFound.front().contains("\"CharaId\":9223372036854775808"));

  summonPayload[0x79] = 3;
  binaryAdvertisementRaw = advertisement;
  binaryAdvertisementRaw.replace("opaque-game-data", summonPayload.toBase64());
  binaryAdvertisement = Parse(binaryAdvertisementRaw);
  CHECK(binaryBroker.Advertise(binaryAdvertisement, binaryAdvertisementRaw, 120)
            .state == Bloodborne::SummonBroker::State::Advertised);
  binaryFound = binaryBroker.Search(Parse(search), 121);
  CHECK(binaryFound.size() == 1);
  CHECK(binaryFound.front() == binaryAdvertisementRaw);

  const auto claimed = broker.Claim(Parse(claim), claim, 120, hostPlacement);
  CHECK(claimed.status == Bloodborne::SummonBroker::ClaimStatus::Claimed);
  CHECK(claimed.targetSessionId == QStringLiteral("guest-session"));
  CHECK(claimed.targetUserId == 2465);
  CHECK(broker.Search(Parse(search), 121).isEmpty());
  CHECK(broker.StateFor(QStringLiteral("guest-session"), 2465, 121) ==
        Bloodborne::SummonBroker::State::Claimed);

  const auto repeatedClaim = broker.Claim(Parse(claim), claim, 122);
  CHECK(repeatedClaim.status ==
        Bloodborne::SummonBroker::ClaimStatus::AlreadyClaimed);
  const auto conflict =
      broker.Claim(Parse(conflictingClaim), conflictingClaim, 123);
  CHECK(conflict.status == Bloodborne::SummonBroker::ClaimStatus::Conflict);

  const auto delivered =
      broker.Advertise(Parse(advertisement), advertisement, 130);
  CHECK(delivered.state == Bloodborne::SummonBroker::State::Delivered);
  CHECK(delivered.pendingClaim == claim);
  CHECK(delivered.pendingHostPlacement.isEmpty());
  CHECK(broker.StateFor(QStringLiteral("guest-session"), 2465, 130) ==
        Bloodborne::SummonBroker::State::Delivered);

  const QByteArray deliveryResponse =
      Bloodborne::BuildClaimDeliveryResponse(delivered.pendingClaim);
  CHECK(
      deliveryResponse.contains("\"MessageId\":\"SummonDataCreateResponse\""));
  CHECK(!deliveryResponse.contains("SummonDataSummonRequest"));
  CHECK(deliveryResponse.contains("\"ResKind\":0"));
  CHECK(!deliveryResponse.contains("\"ResKind\":99"));
  CHECK(deliveryResponse.contains("\"TargetCharaId\":9223372036854775808"));
  CHECK(deliveryResponse.contains("\"HostData\":\"host-owned-data\""));

  const auto redelivered =
      broker.Advertise(Parse(advertisement), advertisement, 140);
  CHECK(redelivered.state == Bloodborne::SummonBroker::State::Delivered);
  CHECK(redelivered.pendingClaim == claim);

  auto consumed = broker.Consume(Parse(removal), 150);
  CHECK(consumed.consumed == 1);
  CHECK(consumed.retained == 0);
  CHECK(broker.StateFor(QStringLiteral("guest-session"), 2465, 150) ==
        Bloodborne::SummonBroker::State::Consumed);
  CHECK(broker.Search(Parse(search), 151).isEmpty());

  const auto readvertised =
      broker.Advertise(Parse(advertisement), advertisement, 160);
  CHECK(readvertised.state == Bloodborne::SummonBroker::State::Advertised);
  CHECK(readvertised.pendingClaim.isEmpty());
  CHECK(broker.Search(Parse(search), 161).size() == 1);
  consumed = broker.Consume(QJsonObject{}, 162);
  CHECK(consumed.consumed == 0);
  CHECK(consumed.retained == 0);
  CHECK(broker.Search(Parse(search), 163).size() == 1);

  const auto claimedByTarget =
      broker.Claim(Parse(targetUserClaim), targetUserClaim, 170);
  CHECK(claimedByTarget.status ==
        Bloodborne::SummonBroker::ClaimStatus::Claimed);
  CHECK(claimedByTarget.targetSessionId == QStringLiteral("guest-session"));
  CHECK(claimedByTarget.targetUserId == 2465);
  CHECK(broker.Size(1'171) == 0);

  Bloodborne::SummonBroker::Options seamlessOptions;
  seamlessOptions.ttlMs = 1'000;
  seamlessOptions.seamlessTtlMs = 5'000;
  seamlessOptions.seamlessCoop = true;
  Bloodborne::SummonBroker seamless(seamlessOptions);
  CHECK(seamless.IsSeamlessCoopEnabled());
  CHECK(seamless.IsSeamlessAnywhereSummonsEnabled());
  CHECK(seamless.Advertise(Parse(advertisement), advertisement, 200).state ==
        Bloodborne::SummonBroker::State::Advertised);
  CHECK(seamless.Claim(Parse(claim), claim, 210).status ==
        Bloodborne::SummonBroker::ClaimStatus::Claimed);

  auto retained = seamless.Consume(Parse(removal), 220);
  CHECK(retained.consumed == 0);
  CHECK(retained.retained == 1);
  CHECK(retained.pendingHostPlacement.isEmpty());
  CHECK(seamless.StateFor(QStringLiteral("guest-session"), 2465, 220) ==
        Bloodborne::SummonBroker::State::Claimed);
  CHECK(seamless.Search(Parse(search), 221).size() == 1);
  CHECK(seamless.Search(Parse(unrelatedSearch), 222).isEmpty());

  const auto seamlessDelivery =
      seamless.Advertise(Parse(advertisement), advertisement, 230);
  CHECK(seamlessDelivery.state == Bloodborne::SummonBroker::State::Delivered);
  CHECK(seamlessDelivery.pendingClaim == claim);
  retained = seamless.Consume(Parse(removal), 240);
  CHECK(retained.consumed == 0);
  CHECK(retained.retained == 1);
  CHECK(seamless.StateFor(QStringLiteral("guest-session"), 2465, 1'241) ==
        Bloodborne::SummonBroker::State::Delivered);
  CHECK(seamless.Size(5'241) == 0);

  CHECK(seamless.Advertise(Parse(advertisement), advertisement, 6'000).state ==
        Bloodborne::SummonBroker::State::Advertised);
  CHECK(seamless.Claim(Parse(claim), claim, 6'010).status ==
        Bloodborne::SummonBroker::ClaimStatus::Claimed);
  retained = seamless.Consume(Parse(forceRemoval), 6'020);
  CHECK(retained.consumed == 1);
  CHECK(retained.retained == 0);
  CHECK(seamless.StateFor(QStringLiteral("guest-session"), 2465, 6'020) ==
        Bloodborne::SummonBroker::State::Consumed);

  Bloodborne::SummonBroker unclaimedSeamless(seamlessOptions);
  CHECK(unclaimedSeamless.Advertise(Parse(advertisement), advertisement, 7'000)
            .state == Bloodborne::SummonBroker::State::Advertised);
  retained = unclaimedSeamless.Consume(Parse(removal), 7'010);
  CHECK(retained.consumed == 1);
  CHECK(retained.retained == 0);
  CHECK(unclaimedSeamless.StateFor(QStringLiteral("guest-session"), 2465,
                                   7'010) ==
        Bloodborne::SummonBroker::State::Consumed);

  Bloodborne::SummonBroker classicRemote(1'000);
  CHECK(classicRemote
            .Advertise(Parse(remoteAdvertisement), remoteAdvertisement, 8'000)
            .state == Bloodborne::SummonBroker::State::Advertised);
  CHECK(classicRemote.Search(Parse(search), 8'010).isEmpty());

  Bloodborne::SummonBroker anywhere(seamlessOptions);
  CHECK(
      anywhere.Advertise(Parse(remoteAdvertisement), remoteAdvertisement, 9'000)
          .state == Bloodborne::SummonBroker::State::Advertised);
  CHECK(anywhere.Search(Parse(search), 9'010, hostPlacement).isEmpty());
  CHECK(anywhere.StateFor(QStringLiteral("remote-session"), 3000, 9'010) ==
        Bloodborne::SummonBroker::State::Preparing);
  const auto preparation = anywhere.Advertise(Parse(remoteAdvertisement),
                                              remoteAdvertisement, 9'011);
  CHECK(preparation.state == Bloodborne::SummonBroker::State::Preparing);
  CHECK(preparation.pendingClaim.isEmpty());
  CHECK(preparation.pendingHostPlacement == hostPlacement);

  retained = anywhere.Consume(
      Parse(QByteArray(
          R"({"MessageId":"SummonDataRemoveRequest","SessionId":"remote-session","UserId":3000})")),
      9'012);
  CHECK(retained.consumed == 0);
  CHECK(retained.retained == 1);
  CHECK(retained.pendingHostPlacement == hostPlacement);

  QByteArray destinationAdvertisementRaw = remoteAdvertisement;
  destinationAdvertisementRaw.replace("\"AreaId\":111", "\"AreaId\":385941504");
  const QJsonObject destinationAdvertisement =
      Parse(destinationAdvertisementRaw);
  const auto prepared = anywhere.Advertise(destinationAdvertisement,
                                           destinationAdvertisementRaw, 9'013);
  CHECK(prepared.state == Bloodborne::SummonBroker::State::Advertised);
  CHECK(prepared.pendingClaim.isEmpty());

  const QList<QByteArray> remoteFound =
      anywhere.Search(Parse(search), 9'014, hostPlacement);
  CHECK(remoteFound.size() == 1);
  CHECK(remoteFound.front().contains("\"CharaId\":9223372036854775808"));
  CHECK(remoteFound.front().contains("\"AreaId\":385875968"));
  CHECK(remoteFound.front().contains("\"AreaRegionId\":230100"));
  CHECK(remoteFound.front().contains("\"ChannelId\":0"));
  CHECK(remoteFound.front().contains("\"MatchingLevel\":46"));
  CHECK(remoteFound.front().contains("\"PosX\":143"));
  CHECK(remoteFound.front().contains("\"PosY\":-116"));
  CHECK(remoteFound.front().contains("\"PosZ\":-87"));
  CHECK(!remoteFound.front().contains("\"AreaId\":111"));
  CHECK(!remoteFound.front().contains("\"PosX\":999"));

  CHECK(anywhere.Claim(Parse(remoteClaim), remoteClaim, 9'020, hostPlacement)
            .status == Bloodborne::SummonBroker::ClaimStatus::Claimed);
  retained = anywhere.Consume(
      Parse(QByteArray(
          R"({"MessageId":"SummonDataRemoveRequest","SessionId":"remote-session","UserId":3000})")),
      9'025);
  CHECK(retained.consumed == 0);
  CHECK(retained.retained == 1);
  CHECK(retained.pendingHostPlacement == hostPlacement);
  const auto destinationDelivery = anywhere.Advertise(
      destinationAdvertisement, destinationAdvertisementRaw, 9'040);
  CHECK(destinationDelivery.state ==
        Bloodborne::SummonBroker::State::Delivered);
  CHECK(destinationDelivery.pendingClaim == remoteClaim);
  CHECK(destinationDelivery.pendingHostPlacement == hostPlacement);
  const QByteArray remoteDeliveryResponse =
      Bloodborne::BuildClaimDeliveryResponse(destinationDelivery.pendingClaim);
  CHECK(remoteDeliveryResponse.contains(
      "\"HostData\":\"remote-host-owned-data\""));
  CHECK(!remoteDeliveryResponse.contains("\"SeamlessWarp\""));

  const QJsonObject traceAdvertisement = Parse(advertisement);
  const QJsonObject traceSearch = Parse(search);
  const QtMessageHandler previousMessageHandler =
      qInstallMessageHandler(CaptureMessage);

  capturedMessages.clear();
  Bloodborne::SummonBroker traceOffBroker(1'000);
  CHECK(
      traceOffBroker.Advertise(traceAdvertisement, advertisement, 100).state ==
      Bloodborne::SummonBroker::State::Advertised);
  const QList<QByteArray> traceOffResults =
      traceOffBroker.Search(traceSearch, 110);
  CHECK(traceOffResults.size() == 1);
  CHECK(!capturedMessages.join(QLatin1Char('\n'))
             .contains("[BLOODBORNE_SUMMON_TRACE]"));

  qputenv("SHADNET_BLOODBORNE_SUMMON_TRACE", "1");
  const TraceRun acceptedTrace =
      RunTraceSearch(traceAdvertisement, traceSearch);
  CHECK(acceptedTrace.found == 1);
  CHECK(acceptedTrace.messages.contains("[BLOODBORNE_SUMMON_TRACE]"));
  CHECK(acceptedTrace.messages.contains("\"result\":\"ACCEPTED\""));
  CHECK(acceptedTrace.messages.contains(
      "\"result\":\"ADVERTISEMENT_FOUND_AND_RETURNED\""));
  CHECK(acceptedTrace.messages.contains("\"requester_user\":2466"));
  CHECK(acceptedTrace.messages.contains("\"candidate_user\":2465"));
  CHECK(acceptedTrace.messages.contains("\"SessionId\":\"host-session\""));
  CHECK(acceptedTrace.messages.contains("\"SessionId\":\"guest-session\""));
  CHECK(acceptedTrace.messages.contains("\"SummonWordPresent\":false"));

  const QStringList allChecks = {
      QStringLiteral("CANDIDATE_STATE"),
      QStringLiteral("DIFFERENT_USER"),
      QStringLiteral("DIFFERENT_SESSION"),
      QStringLiteral("SUMMON_DATA_VERSION"),
      QStringLiteral("SUMMON_METHOD"),
      QStringLiteral("AREA_ID"),
      QStringLiteral("AREA_REGION_ID"),
      QStringLiteral("CHANNEL_ID"),
      QStringLiteral("SUMMON_TYPE"),
      QStringLiteral("SUMMON_WORD"),
      QStringLiteral("MATCHING_LEVEL"),
      QStringLiteral("DISTANCE"),
  };
  for (const QString &check : allChecks) {
    CHECK(acceptedTrace.messages.contains(
        QStringLiteral("\"%1\":\"PASS\"").arg(check)));
  }

  struct RejectionScenario {
    QString primaryReason;
    QJsonObject advertisement;
    QJsonObject request;
  };
  QList<RejectionScenario> rejectionScenarios;
  auto addAdvertisementRejection = [&](const QString &reason,
                                       const QString &field,
                                       const QJsonValue &value) {
    QJsonObject changed = traceAdvertisement;
    changed.insert(field, value);
    rejectionScenarios.append({reason, changed, traceSearch});
  };
  addAdvertisementRejection(QStringLiteral("DIFFERENT_USER"),
                            QStringLiteral("UserId"), 2466);
  addAdvertisementRejection(QStringLiteral("DIFFERENT_SESSION"),
                            QStringLiteral("SessionId"),
                            QStringLiteral("host-session"));
  addAdvertisementRejection(QStringLiteral("SUMMON_DATA_VERSION"),
                            QStringLiteral("SummonDataVersion"), 2);
  addAdvertisementRejection(QStringLiteral("SUMMON_METHOD"),
                            QStringLiteral("SummonMethod"), 1);
  addAdvertisementRejection(QStringLiteral("AREA_ID"), QStringLiteral("AreaId"),
                            385875969);
  addAdvertisementRejection(QStringLiteral("AREA_REGION_ID"),
                            QStringLiteral("AreaRegionId"), 230101);
  addAdvertisementRejection(QStringLiteral("CHANNEL_ID"),
                            QStringLiteral("ChannelId"), 9);
  addAdvertisementRejection(QStringLiteral("SUMMON_TYPE"),
                            QStringLiteral("SummonType"), 1);
  addAdvertisementRejection(QStringLiteral("MATCHING_LEVEL"),
                            QStringLiteral("MatchingLevel"), 200);
  addAdvertisementRejection(QStringLiteral("DISTANCE"), QStringLiteral("PosX"),
                            999);

  QJsonObject passwordAdvertisement = traceAdvertisement;
  passwordAdvertisement.insert(QStringLiteral("SummonWord"),
                               QStringLiteral("wrong-password"));
  QJsonObject passwordSearch = traceSearch;
  passwordSearch.insert(QStringLiteral("SummonWord"),
                        QStringLiteral("expected-password"));
  rejectionScenarios.append(
      {QStringLiteral("SUMMON_WORD"), passwordAdvertisement, passwordSearch});

  QString channelTrace;
  for (const RejectionScenario &scenario : rejectionScenarios) {
    const TraceRun rejected =
        RunTraceSearch(scenario.advertisement, scenario.request);
    CHECK(rejected.found == 0);
    CHECK(rejected.messages.contains(QStringLiteral("\"primary_reason\":\"%1\"")
                                         .arg(scenario.primaryReason)));
    CHECK(rejected.messages.contains(
        QStringLiteral("\"%1\":\"FAIL\"").arg(scenario.primaryReason)));
    CHECK(rejected.messages.contains(
        "\"result\":\"ADVERTISEMENT_FOUND_BUT_FILTERED\""));
    if (scenario.primaryReason == QStringLiteral("CHANNEL_ID")) {
      channelTrace = rejected.messages;
    }
  }

  Bloodborne::SummonBroker stateFilteredBroker(1'000);
  stateFilteredBroker.Advertise(traceAdvertisement, advertisement, 100);
  stateFilteredBroker.Claim(Parse(claim), claim, 105);
  capturedMessages.clear();
  CHECK(stateFilteredBroker.Search(traceSearch, 110).isEmpty());
  const QString stateFilteredTrace = capturedMessages.join(QLatin1Char('\n'));
  CHECK(stateFilteredTrace.contains("\"CANDIDATE_STATE\":\"FAIL\""));
  CHECK(stateFilteredTrace.contains("\"primary_reason\":\"CANDIDATE_STATE\""));

  CHECK(channelTrace.contains("\"channel_id\":9"));
  CHECK(channelTrace.contains(
      "\"metadata_source\":\"UNAVAILABLE_IN_SUMMON_BROKER\""));
  CHECK(channelTrace.contains("\"exists_in_db\":null"));
  CHECK(channelTrace.contains("\"glyph\":null"));
  CHECK(channelTrace.contains("\"share_level\":null"));
  CHECK(channelTrace.contains("\"status\":null"));
  CHECK(channelTrace.contains("\"owner_user_id\":null"));
  CHECK(channelTrace.contains("\"vanilla_fixed\":null"));
  CHECK(channelTrace.contains("\"community\":null"));

  QJsonObject multipleFailures = traceAdvertisement;
  multipleFailures.insert(QStringLiteral("SummonDataVersion"), 2);
  multipleFailures.insert(QStringLiteral("AreaId"), 385875969);
  const TraceRun allFailureChecks =
      RunTraceSearch(multipleFailures, traceSearch);
  CHECK(allFailureChecks.messages.contains("\"SUMMON_DATA_VERSION\":\"FAIL\""));
  CHECK(allFailureChecks.messages.contains("\"AREA_ID\":\"FAIL\""));
  CHECK(allFailureChecks.messages.contains("\"CHANNEL_ID\":\"PASS\""));
  CHECK(allFailureChecks.messages.contains("\"DISTANCE\":\"PASS\""));

  const TraceRun noAdvertisement =
      RunTraceSearch(QJsonObject{}, traceSearch, false);
  CHECK(noAdvertisement.found == 0);
  CHECK(noAdvertisement.messages.contains("\"result\":\"NO_ADVERTISEMENT\""));
  CHECK(!noAdvertisement.messages.contains("\"event\":\"CANDIDATE\""));

  qunsetenv("SHADNET_BLOODBORNE_SUMMON_TRACE");
  qInstallMessageHandler(previousMessageHandler);

  std::cout << "Bloodborne summon broker state test passed\n";
  return 0;
}
