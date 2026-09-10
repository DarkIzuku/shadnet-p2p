// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "bloodborne_seamless_party.h"
#include <cstdlib>
#include <iostream>

namespace {
int failures = 0;
#define CHECK(expr)                                                            \
  do {                                                                         \
    if (!(expr)) {                                                             \
      std::cerr << "CHECK failed: " #expr " at line " << __LINE__ << '\n';     \
      ++failures;                                                              \
    }                                                                          \
  } while (false)

Bloodborne::SeamlessRoomSnapshot MakeRoom() {
  Bloodborne::SeamlessRoomSnapshot room;
  room.roomId = 91;
  room.leaderUserId = 1;
  room.leaderNpid = QStringLiteral("Izuku");
  room.members = {
      {1, QStringLiteral("Izuku"), Bloodborne::SeamlessPeerRole::Host},
      {2, QStringLiteral("Hiryu"), Bloodborne::SeamlessPeerRole::Cooperator}};
  return room;
}

Bloodborne::SeamlessRoomSnapshot MakeMixedRoom() {
  auto room = MakeRoom();
  room.members.append(
      {3, QStringLiteral("Maria"), Bloodborne::SeamlessPeerRole::Invader});
  return room;
}

Bloodborne::SeamlessTravelEvent MakeBegin(quint64 sequence = 1) {
  Bloodborne::SeamlessTravelEvent event;
  event.phase = Bloodborne::SeamlessTravelPhase::TravelBegin;
  event.sequenceId = sequence;
  event.sourceMap = 0x18010000;
  event.destinationMap = 0x15000000;
  event.warpParamId = 2102952;
  event.mode = 2;
  return event;
}
} // namespace

int main() {
  Bloodborne::SeamlessPartyService disabled;
  CHECK(
      !disabled.Handle(1, QStringLiteral("Izuku"), MakeBegin(), MakeRoom(), 100)
           .accepted);

  Bloodborne::SeamlessPartyService::Options options;
  options.enabled = true;
  options.partyTtlMs = 1'000;
  options.travelTimeoutMs = 500;
  Bloodborne::SeamlessPartyService service(options);

  auto begin =
      service.Handle(1, QStringLiteral("Izuku"), MakeBegin(), MakeRoom(), 100);
  CHECK(begin.accepted);
  CHECK(!begin.partyId.isEmpty());
  CHECK(begin.sequenceId == 1);
  CHECK(begin.state == Bloodborne::SeamlessPartyState::TravelPreparing);
  CHECK(begin.deliveries.size() == 1);
  CHECK(begin.deliveries[0].targetUserId == 2);
  CHECK(begin.deliveries[0].event.warpParamId == 2102952);

  auto overlappingBegin = service.Handle(1, QStringLiteral("Izuku"),
                                         MakeBegin(2), std::nullopt, 105);
  CHECK(!overlappingBegin.accepted);
  CHECK(overlappingBegin.reason == QStringLiteral("travel_in_progress"));

  auto guestBegin = MakeBegin(2);
  guestBegin.partyId = begin.partyId;
  guestBegin.generation = begin.generation;
  CHECK(
      !service.Handle(2, QStringLiteral("Hiryu"), guestBegin, std::nullopt, 110)
           .accepted);

  Bloodborne::SeamlessTravelEvent ready;
  ready.phase = Bloodborne::SeamlessTravelPhase::TravelReady;
  ready.partyId = begin.partyId;
  ready.generation = begin.generation;
  ready.sequenceId = begin.sequenceId;
  auto earlyArrived = ready;
  earlyArrived.phase = Bloodborne::SeamlessTravelPhase::TravelArrived;
  auto earlyArrivedResult = service.Handle(2, QStringLiteral("Hiryu"),
                                           earlyArrived, std::nullopt, 115);
  CHECK(!earlyArrivedResult.accepted);
  CHECK(earlyArrivedResult.reason == QStringLiteral("invalid_travel_state"));

  auto readyResult =
      service.Handle(2, QStringLiteral("Hiryu"), ready, std::nullopt, 120);
  CHECK(readyResult.accepted);
  CHECK(readyResult.deliveries.size() == 1);
  CHECK(readyResult.deliveries[0].targetUserId == 1);
  auto duplicateReady =
      service.Handle(2, QStringLiteral("Hiryu"), ready, std::nullopt, 121);
  CHECK(duplicateReady.accepted);
  CHECK(duplicateReady.deliveries.isEmpty());

  Bloodborne::SeamlessTravelEvent commit = ready;
  commit.phase = Bloodborne::SeamlessTravelPhase::TravelCommit;
  auto commitResult =
      service.Handle(1, QStringLiteral("Izuku"), commit, std::nullopt, 130);
  CHECK(commitResult.accepted);
  CHECK(commitResult.state == Bloodborne::SeamlessPartyState::Traveling);
  CHECK(commitResult.deliveries.size() == 1);
  CHECK(commitResult.deliveries[0].targetUserId == 2);
  auto duplicateCommit =
      service.Handle(1, QStringLiteral("Izuku"), commit, std::nullopt, 131);
  CHECK(!duplicateCommit.accepted);
  CHECK(duplicateCommit.reason == QStringLiteral("invalid_travel_state"));

  Bloodborne::SeamlessTravelEvent arrived = ready;
  arrived.phase = Bloodborne::SeamlessTravelPhase::TravelArrived;
  CHECK(service.Handle(2, QStringLiteral("Hiryu"), arrived, std::nullopt, 140)
            .accepted);
  auto hostArrived =
      service.Handle(1, QStringLiteral("Izuku"), arrived, std::nullopt, 150);
  CHECK(hostArrived.accepted);
  CHECK(hostArrived.reason == QStringLiteral("travel_complete"));
  CHECK(hostArrived.state == Bloodborne::SeamlessPartyState::Connected);

  auto stale = service.Handle(1, QStringLiteral("Izuku"), MakeBegin(1),
                              std::nullopt, 160);
  CHECK(!stale.accepted);
  CHECK(stale.reason == QStringLiteral("stale_sequence"));

  auto next = service.Handle(1, QStringLiteral("Izuku"), MakeBegin(2),
                             std::nullopt, 170);
  CHECK(next.accepted);
  service.MarkRoomLost(2, 91, 180);
  auto retained = service.SnapshotForUser(2, 180);
  CHECK(retained.has_value());
  CHECK(retained->state == Bloodborne::SeamlessPartyState::TravelPreparing);
  service.MarkDisconnected(2, 190);
  CHECK(service.SnapshotForUser(1, 190).has_value());
  CHECK(service.Size(1'191) == 0);

  Bloodborne::SeamlessPartyService mixedService(options);
  const auto mixedBegin = mixedService.Handle(
      1, QStringLiteral("Izuku"), MakeBegin(), MakeMixedRoom(), 2'000);
  CHECK(mixedBegin.accepted);
  CHECK(mixedBegin.deliveries.size() == 1);
  CHECK(mixedBegin.deliveries[0].targetUserId == 2);
  const auto mixedSnapshot = mixedService.SnapshotForUser(1, 2'001);
  CHECK(mixedSnapshot.has_value());
  CHECK(mixedSnapshot->members.size() == 2);
  CHECK(!mixedService.SnapshotForUser(3, 2'001).has_value());
  mixedService.MarkDisconnected(3, 2'002);
  CHECK(mixedService.SnapshotForUser(1, 2'002).has_value());

  if (failures != 0)
    return EXIT_FAILURE;
  std::cout << "bloodborne seamless party tests passed\n";
  return EXIT_SUCCESS;
}
