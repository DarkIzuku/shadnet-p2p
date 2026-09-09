// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "bloodborne_seamless_party.h"

#include <algorithm>
#include <cmath>
#include <QUuid>

namespace Bloodborne {
namespace {

bool IsFinitePlacement(const SeamlessTravelEvent& event) {
    return std::isfinite(event.positionX) && std::isfinite(event.positionY) &&
           std::isfinite(event.positionZ) && std::isfinite(event.orientation);
}

bool IsTravelPhase(SeamlessTravelPhase phase) {
    return phase >= SeamlessTravelPhase::TravelBegin && phase <= SeamlessTravelPhase::TravelFailed;
}

} // namespace

SeamlessPartyService::SeamlessPartyService() : SeamlessPartyService(Options{}) {}

SeamlessPartyService::SeamlessPartyService(Options options) : m_options(options) {
    m_options.partyTtlMs = std::max<qint64>(1'000, m_options.partyTtlMs);
    m_options.travelTimeoutMs = std::max<qint64>(5'000, m_options.travelTimeoutMs);
    m_options.maxMembers = std::clamp(m_options.maxMembers, 2, 16);
}

void SeamlessPartyService::SetEnabled(bool enabled) {
    QMutexLocker lock(&m_mutex);
    m_options.enabled = enabled;
    if (!enabled) {
        m_parties.clear();
        m_userToParty.clear();
    }
}

bool SeamlessPartyService::IsEnabled() const {
    QMutexLocker lock(&m_mutex);
    return m_options.enabled;
}

SeamlessHandleResult SeamlessPartyService::RejectLocked(const QString& reason) const {
    SeamlessHandleResult result;
    result.reason = reason;
    return result;
}

SeamlessHandleResult SeamlessPartyService::AcceptLocked(const Party& party,
                                                        const QString& reason) const {
    SeamlessHandleResult result;
    result.accepted = true;
    result.reason = reason;
    result.partyId = party.id;
    result.generation = party.generation;
    result.sequenceId = party.lastSequenceId;
    result.state = party.state;
    return result;
}

std::optional<QString> SeamlessPartyService::PartyIdForUserLocked(qint64 userId) const {
    const auto it = m_userToParty.constFind(userId);
    if (it == m_userToParty.constEnd())
        return std::nullopt;
    return it.value();
}

void SeamlessPartyService::RemovePartyLocked(const QString& partyId) {
    auto party = m_parties.find(partyId);
    if (party == m_parties.end())
        return;
    for (auto member = party->members.cbegin(); member != party->members.cend(); ++member)
        m_userToParty.remove(member.key());
    m_parties.erase(party);
}

void SeamlessPartyService::PurgeExpiredLocked(qint64 nowMs) {
    QVector<QString> expired;
    QVector<QString> timedOutTravel;
    for (auto it = m_parties.cbegin(); it != m_parties.cend(); ++it) {
        const bool partyExpired = nowMs - it->lastActivityMs > m_options.partyTtlMs;
        const bool travelExpired =
            it->activeTravel.has_value() && nowMs - it->travelStartedMs > m_options.travelTimeoutMs;
        if (partyExpired) {
            expired.append(it.key());
        } else if (travelExpired) {
            timedOutTravel.append(it.key());
        }
    }
    for (const auto& id : timedOutTravel) {
        auto party = m_parties.find(id);
        if (party == m_parties.end())
            continue;
        party->activeTravel.reset();
        party->readyMembers.clear();
        party->arrivedMembers.clear();
        party->readyNotificationSent = false;
        party->state = SeamlessPartyState::RecoveringRoom;
        ++party->generation;
        party->lastActivityMs = nowMs;
    }
    for (const auto& id : expired)
        RemovePartyLocked(id);
}

void SeamlessPartyService::DeliverToMembersLocked(const Party& party, qint64 exceptUserId,
                                                  const SeamlessTravelEvent& event,
                                                  QVector<SeamlessDelivery>& deliveries) const {
    for (auto it = party.members.cbegin(); it != party.members.cend(); ++it) {
        if (it.key() == exceptUserId || !it->connected)
            continue;
        deliveries.append({it.key(), event});
    }
}

SeamlessHandleResult SeamlessPartyService::Handle(qint64 senderUserId, const QString& senderNpid,
                                                  const SeamlessTravelEvent& input,
                                                  const std::optional<SeamlessRoomSnapshot>& room,
                                                  qint64 nowMs) {
    QMutexLocker lock(&m_mutex);
    PurgeExpiredLocked(nowMs);
    if (!m_options.enabled)
        return RejectLocked(QStringLiteral("disabled"));
    if (senderUserId <= 0 || senderNpid.isEmpty())
        return RejectLocked(QStringLiteral("invalid_sender"));
    if (input.protocolVersion != 1)
        return RejectLocked(QStringLiteral("unsupported_protocol"));
    if (!IsFinitePlacement(input))
        return RejectLocked(QStringLiteral("invalid_placement"));

    std::optional<QString> partyId = PartyIdForUserLocked(senderUserId);
    if (input.phase == SeamlessTravelPhase::TravelBegin && !partyId.has_value()) {
        if (!room.has_value() || room->roomId == 0 || room->leaderUserId != senderUserId ||
            room->leaderNpid != senderNpid)
            return RejectLocked(QStringLiteral("leader_room_required"));
        if (room->members.size() < 2 || room->members.size() > m_options.maxMembers)
            return RejectLocked(QStringLiteral("invalid_party_size"));

        QSet<qint64> uniqueMembers;
        Party party;
        party.id =
            QStringLiteral("sp-") + QUuid::createUuid().toString(QUuid::WithoutBraces).toLower();
        party.leaderUserId = senderUserId;
        party.leaderNpid = senderNpid;
        party.activeRoomId = room->roomId;
        party.lastActivityMs = nowMs;
        for (const auto& member : room->members) {
            if (member.userId <= 0 || member.npid.isEmpty() ||
                uniqueMembers.contains(member.userId))
                return RejectLocked(QStringLiteral("invalid_room_members"));
            const auto existingParty = m_userToParty.constFind(member.userId);
            if (existingParty != m_userToParty.constEnd())
                return RejectLocked(QStringLiteral("member_already_in_party"));
            uniqueMembers.insert(member.userId);
            MemberState state;
            state.identity = member;
            state.lastSeenMs = nowMs;
            party.members.insert(member.userId, state);
        }
        if (!party.members.contains(senderUserId))
            return RejectLocked(QStringLiteral("leader_not_in_room"));
        partyId = party.id;
        m_parties.insert(party.id, party);
        for (const auto& member : room->members)
            m_userToParty.insert(member.userId, party.id);
    }

    if (!partyId.has_value())
        return RejectLocked(QStringLiteral("party_missing"));
    auto partyIt = m_parties.find(*partyId);
    if (partyIt == m_parties.end())
        return RejectLocked(QStringLiteral("party_missing"));
    Party& party = partyIt.value();
    auto memberIt = party.members.find(senderUserId);
    if (memberIt == party.members.end() || memberIt->identity.npid != senderNpid)
        return RejectLocked(QStringLiteral("not_a_party_member"));
    if (!input.partyId.isEmpty() && input.partyId != party.id)
        return RejectLocked(QStringLiteral("party_mismatch"));
    if (input.generation != 0 && input.generation != party.generation)
        return RejectLocked(QStringLiteral("generation_mismatch"));

    memberIt->connected = true;
    memberIt->lastSeenMs = nowMs;
    party.lastActivityMs = nowMs;

    if (input.phase == SeamlessTravelPhase::LeaveParty) {
        if (senderUserId == party.leaderUserId) {
            const QString id = party.id;
            auto result = AcceptLocked(party, QStringLiteral("party_closed"));
            RemovePartyLocked(id);
            result.state = SeamlessPartyState::Disconnected;
            return result;
        }
        party.members.erase(memberIt);
        m_userToParty.remove(senderUserId);
        return AcceptLocked(party, QStringLiteral("member_left"));
    }

    if (input.phase == SeamlessTravelPhase::Heartbeat)
        return AcceptLocked(party, QStringLiteral("heartbeat"));
    if (!IsTravelPhase(input.phase))
        return RejectLocked(QStringLiteral("invalid_phase"));

    if (input.phase == SeamlessTravelPhase::TravelBegin) {
        if (senderUserId != party.leaderUserId)
            return RejectLocked(QStringLiteral("leader_required"));
        if (party.activeTravel.has_value())
            return RejectLocked(QStringLiteral("travel_in_progress"));
        if (input.destinationMap == 0 || input.warpParamId < 0 || input.mode > 8)
            return RejectLocked(QStringLiteral("invalid_destination"));
        if (input.sequenceId != 0 && input.sequenceId <= party.lastSequenceId)
            return RejectLocked(QStringLiteral("stale_sequence"));

        party.lastSequenceId = input.sequenceId == 0 ? party.lastSequenceId + 1 : input.sequenceId;
        SeamlessTravelEvent event = input;
        event.partyId = party.id;
        event.generation = party.generation;
        event.sequenceId = party.lastSequenceId;
        event.leaderUserId = party.leaderUserId;
        event.leaderNpid = party.leaderNpid;
        event.activeRoomId = party.activeRoomId;
        event.timestampMs = nowMs;
        party.activeTravel = event;
        party.readyMembers.clear();
        party.arrivedMembers.clear();
        party.readyNotificationSent = false;
        party.readyMembers.insert(senderUserId);
        party.state = SeamlessPartyState::TravelPreparing;
        party.travelStartedMs = nowMs;

        auto result = AcceptLocked(party, QStringLiteral("travel_started"));
        DeliverToMembersLocked(party, senderUserId, event, result.deliveries);
        return result;
    }

    if (!party.activeTravel.has_value())
        return RejectLocked(QStringLiteral("travel_missing"));
    if (input.sequenceId != party.lastSequenceId)
        return RejectLocked(QStringLiteral("stale_sequence"));

    SeamlessTravelEvent event = *party.activeTravel;
    event.phase = input.phase;
    event.timestampMs = nowMs;
    event.failureReason = input.failureReason.left(128);

    if (input.phase == SeamlessTravelPhase::TravelReady) {
        if (party.state != SeamlessPartyState::TravelPreparing)
            return RejectLocked(QStringLiteral("invalid_travel_state"));
        if (senderUserId == party.leaderUserId)
            return RejectLocked(QStringLiteral("leader_already_ready"));
        party.readyMembers.insert(senderUserId);
        auto result = AcceptLocked(party, QStringLiteral("member_ready"));
        bool allReady = true;
        for (auto it = party.members.cbegin(); it != party.members.cend(); ++it) {
            if (it->connected && !party.readyMembers.contains(it.key())) {
                allReady = false;
                break;
            }
        }
        if (allReady && !party.readyNotificationSent && senderUserId != party.leaderUserId) {
            party.readyNotificationSent = true;
            result.reason = QStringLiteral("party_ready");
            result.deliveries.append({party.leaderUserId, event});
        }
        return result;
    }
    if (input.phase == SeamlessTravelPhase::TravelCommit) {
        if (senderUserId != party.leaderUserId)
            return RejectLocked(QStringLiteral("leader_required"));
        if (party.state != SeamlessPartyState::TravelPreparing)
            return RejectLocked(QStringLiteral("invalid_travel_state"));
        for (auto it = party.members.cbegin(); it != party.members.cend(); ++it) {
            if (it->connected && !party.readyMembers.contains(it.key()))
                return RejectLocked(QStringLiteral("members_not_ready"));
        }
        party.state = SeamlessPartyState::Traveling;
        auto result = AcceptLocked(party, QStringLiteral("travel_committed"));
        DeliverToMembersLocked(party, senderUserId, event, result.deliveries);
        return result;
    }
    if (input.phase == SeamlessTravelPhase::TravelArrived) {
        if (party.state != SeamlessPartyState::Traveling &&
            party.state != SeamlessPartyState::WorldLoading &&
            party.state != SeamlessPartyState::Rebinding)
            return RejectLocked(QStringLiteral("invalid_travel_state"));
        party.arrivedMembers.insert(senderUserId);
        party.state = SeamlessPartyState::Rebinding;
        auto result = AcceptLocked(party, QStringLiteral("member_arrived"));
        if (senderUserId != party.leaderUserId)
            result.deliveries.append({party.leaderUserId, event});
        bool allArrived = true;
        for (auto it = party.members.cbegin(); it != party.members.cend(); ++it) {
            if (it->connected && !party.arrivedMembers.contains(it.key())) {
                allArrived = false;
                break;
            }
        }
        if (allArrived) {
            party.state = SeamlessPartyState::Connected;
            party.activeTravel.reset();
            party.readyMembers.clear();
            party.arrivedMembers.clear();
            party.readyNotificationSent = false;
            result.state = party.state;
            result.reason = QStringLiteral("travel_complete");
        }
        return result;
    }

    party.state = SeamlessPartyState::RecoveringRoom;
    auto result = AcceptLocked(party, QStringLiteral("travel_failed"));
    if (senderUserId != party.leaderUserId)
        result.deliveries.append({party.leaderUserId, event});
    return result;
}

void SeamlessPartyService::MarkRoomLost(qint64 userId, quint64 roomId, qint64 nowMs) {
    QMutexLocker lock(&m_mutex);
    PurgeExpiredLocked(nowMs);
    const auto id = PartyIdForUserLocked(userId);
    if (!id.has_value())
        return;
    auto party = m_parties.find(*id);
    if (party == m_parties.end() || (roomId != 0 && party->activeRoomId != roomId))
        return;
    auto member = party->members.find(userId);
    if (member != party->members.end()) {
        member->roomPresent = false;
        member->lastSeenMs = nowMs;
    }
    // Losing the vanilla room does not cancel an in-flight seamless transition. The control
    // party remains authoritative so Ready/Commit/Arrived can finish while room recovery is
    // handled independently. Outside a travel, expose the loss as a recovery state.
    if (!party->activeTravel.has_value())
        party->state = SeamlessPartyState::RecoveringRoom;
    party->lastActivityMs = nowMs;
}

void SeamlessPartyService::MarkConnected(qint64 userId, qint64 nowMs) {
    QMutexLocker lock(&m_mutex);
    PurgeExpiredLocked(nowMs);
    const auto id = PartyIdForUserLocked(userId);
    if (!id.has_value())
        return;
    auto party = m_parties.find(*id);
    if (party == m_parties.end())
        return;
    auto member = party->members.find(userId);
    if (member != party->members.end()) {
        member->connected = true;
        member->lastSeenMs = nowMs;
        party->lastActivityMs = nowMs;
    }
}

void SeamlessPartyService::MarkDisconnected(qint64 userId, qint64 nowMs) {
    QMutexLocker lock(&m_mutex);
    PurgeExpiredLocked(nowMs);
    const auto id = PartyIdForUserLocked(userId);
    if (!id.has_value())
        return;
    auto party = m_parties.find(*id);
    if (party == m_parties.end())
        return;
    auto member = party->members.find(userId);
    if (member != party->members.end()) {
        member->connected = false;
        member->roomPresent = false;
        member->lastSeenMs = nowMs;
        party->state = userId == party->leaderUserId ? SeamlessPartyState::Disconnected
                                                     : SeamlessPartyState::RecoveringPeer;
        party->lastActivityMs = nowMs;
    }
}

std::optional<SeamlessPartySnapshot> SeamlessPartyService::SnapshotForUser(qint64 userId,
                                                                           qint64 nowMs) {
    QMutexLocker lock(&m_mutex);
    PurgeExpiredLocked(nowMs);
    const auto id = PartyIdForUserLocked(userId);
    if (!id.has_value())
        return std::nullopt;
    const auto party = m_parties.constFind(*id);
    if (party == m_parties.constEnd())
        return std::nullopt;
    SeamlessPartySnapshot snapshot;
    snapshot.partyId = party->id;
    snapshot.leaderUserId = party->leaderUserId;
    snapshot.leaderNpid = party->leaderNpid;
    snapshot.activeRoomId = party->activeRoomId;
    snapshot.generation = party->generation;
    snapshot.sequenceId = party->lastSequenceId;
    snapshot.state = party->state;
    for (auto member = party->members.cbegin(); member != party->members.cend(); ++member)
        snapshot.members.append(member->identity);
    return snapshot;
}

int SeamlessPartyService::Size(qint64 nowMs) {
    QMutexLocker lock(&m_mutex);
    PurgeExpiredLocked(nowMs);
    return m_parties.size();
}

} // namespace Bloodborne
