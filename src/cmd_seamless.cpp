// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <limits>
#include <optional>
#include <QDateTime>
#include "bloodborne_seamless_party.h"
#include "client_session.h"
#include "proto_utils.h"
#include "shadnet.pb.h"

namespace {

using Bloodborne::SeamlessTravelEvent;
using Bloodborne::SeamlessTravelPhase;

std::optional<SeamlessTravelPhase> ParsePhase(quint32 value) {
    if (value < static_cast<quint32>(SeamlessTravelPhase::TravelBegin) ||
        value > static_cast<quint32>(SeamlessTravelPhase::LeaveParty))
        return std::nullopt;
    return static_cast<SeamlessTravelPhase>(value);
}

QString PhaseName(SeamlessTravelPhase phase) {
    switch (phase) {
    case SeamlessTravelPhase::TravelBegin:
        return QStringLiteral("TravelBegin");
    case SeamlessTravelPhase::TravelReady:
        return QStringLiteral("TravelReady");
    case SeamlessTravelPhase::TravelCommit:
        return QStringLiteral("TravelCommit");
    case SeamlessTravelPhase::TravelArrived:
        return QStringLiteral("TravelArrived");
    case SeamlessTravelPhase::TravelFailed:
        return QStringLiteral("TravelFailed");
    case SeamlessTravelPhase::Heartbeat:
        return QStringLiteral("Heartbeat");
    case SeamlessTravelPhase::LeaveParty:
        return QStringLiteral("LeaveParty");
    }
    return QStringLiteral("Unknown");
}

SeamlessTravelEvent FromProto(const shadnet::SeamlessTravelEvent& pb, SeamlessTravelPhase phase) {
    SeamlessTravelEvent event;
    event.protocolVersion = pb.protocol_version();
    event.phase = phase;
    event.partyId = QString::fromStdString(pb.party_id());
    event.generation = pb.generation();
    event.sequenceId = pb.travel_sequence_id();
    if (pb.leader_user_id() <= static_cast<quint64>(std::numeric_limits<qint64>::max())) {
        event.leaderUserId = static_cast<qint64>(pb.leader_user_id());
    }
    event.leaderNpid = QString::fromStdString(pb.leader_npid());
    event.activeRoomId = pb.active_room_id();
    event.sourceMap = pb.source_map();
    event.destinationMap = pb.destination_map();
    event.warpParamId = pb.warp_param_id();
    event.mode = pb.mode();
    event.positionX = pb.position_x();
    event.positionY = pb.position_y();
    event.positionZ = pb.position_z();
    event.orientation = pb.orientation();
    event.timestampMs = pb.timestamp_ms();
    event.failureReason = QString::fromStdString(pb.failure_reason());
    return event;
}

void ToProto(const SeamlessTravelEvent& event, shadnet::SeamlessTravelEvent* pb) {
    pb->set_protocol_version(event.protocolVersion);
    pb->set_phase(static_cast<quint32>(event.phase));
    pb->set_party_id(event.partyId.toStdString());
    pb->set_generation(event.generation);
    pb->set_travel_sequence_id(event.sequenceId);
    pb->set_leader_user_id(static_cast<quint64>(event.leaderUserId));
    pb->set_leader_npid(event.leaderNpid.toStdString());
    pb->set_active_room_id(event.activeRoomId);
    pb->set_source_map(event.sourceMap);
    pb->set_destination_map(event.destinationMap);
    pb->set_warp_param_id(event.warpParamId);
    pb->set_mode(event.mode);
    pb->set_position_x(event.positionX);
    pb->set_position_y(event.positionY);
    pb->set_position_z(event.positionZ);
    pb->set_orientation(event.orientation);
    pb->set_timestamp_ms(event.timestampMs);
    pb->set_failure_reason(event.failureReason.toStdString());
}

} // namespace

ErrorType ClientSession::CmdSeamlessControl(StreamExtractor& data, QByteArray& reply) {
    shadnet::SeamlessControlRequest request;
    if (!decodeProto(request, data) || data.error() || !request.has_event())
        return ErrorType::Malformed;
    if (!m_shared || !m_shared->seamlessParties)
        return ErrorType::Unsupported;

    const auto phase = ParsePhase(request.event().phase());
    if (!phase.has_value())
        return ErrorType::InvalidInput;

    std::optional<Bloodborne::SeamlessRoomSnapshot> roomSnapshot;
    if (m_matching.roomId != 0) {
        QReadLocker roomLock(&m_shared->matching.roomsLock);
        const auto room =
            m_shared->matching.rooms.constFind({m_matching.matchingKey, m_matching.roomId});
        if (room != m_shared->matching.rooms.constEnd()) {
            const auto owner = room->members.constFind(room->ownerMemberId);
            if (owner != room->members.constEnd()) {
                Bloodborne::SeamlessRoomSnapshot snapshot;
                snapshot.roomId = room->roomId;
                snapshot.leaderUserId = owner.value().userId;
                snapshot.leaderNpid = owner.value().npid;
                for (auto member = room->members.cbegin(); member != room->members.cend();
                     ++member) {
                    snapshot.members.append({member->userId, member->npid});
                }
                roomSnapshot = std::move(snapshot);
            }
        }
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const auto event = FromProto(request.event(), *phase);
    auto result =
        m_shared->seamlessParties->Handle(m_info.userId, m_info.npid, event, roomSnapshot, nowMs);
    QString leaderNpid;
    if (const auto party = m_shared->seamlessParties->SnapshotForUser(m_info.userId, nowMs);
        party.has_value()) {
        leaderNpid = party->leaderNpid;
    } else if (roomSnapshot.has_value()) {
        leaderNpid = roomSnapshot->leaderNpid;
    }

    shadnet::SeamlessControlReply response;
    response.set_accepted(result.accepted);
    response.set_reason(result.reason.toStdString());
    response.set_party_id(result.partyId.toStdString());
    response.set_generation(result.generation);
    response.set_travel_sequence_id(result.sequenceId);
    response.set_state(static_cast<quint32>(result.state));
    appendProto(reply, response);

    if (result.accepted) {
        for (const auto& delivery : result.deliveries) {
            shadnet::NotifySeamlessControl notification;
            ToProto(delivery.event, notification.mutable_event());
            notification.set_source_user_id(static_cast<quint64>(m_info.userId));
            notification.set_source_npid(m_info.npid.toStdString());
            QByteArray payload;
            appendProto(payload, notification);
            SendNotification(NotificationType::SeamlessControl, payload, delivery.targetUserId);
        }
    }

    qInfo().nospace().noquote() << "[BLOODBORNE SEAMLESS "
                                << (*phase == SeamlessTravelPhase::TravelBegin ||
                                            *phase == SeamlessTravelPhase::TravelReady ||
                                            *phase == SeamlessTravelPhase::TravelCommit ||
                                            *phase == SeamlessTravelPhase::TravelArrived ||
                                            *phase == SeamlessTravelPhase::TravelFailed
                                        ? "TRAVEL"
                                        : "PARTY")
                                << "] party="
                                << (result.partyId.isEmpty() ? QStringLiteral("-") : result.partyId)
                                << " seq=" << result.sequenceId << " leader="
                                << (leaderNpid.isEmpty() ? QStringLiteral("-") : leaderNpid)
                                << " member=" << m_info.npid << " source_map=0x"
                                << QString::number(event.sourceMap, 16) << " destination_map=0x"
                                << QString::number(event.destinationMap, 16)
                                << " warp_param=" << event.warpParamId
                                << " state=" << PhaseName(*phase)
                                << " accepted=" << (result.accepted ? "true" : "false")
                                << " result=" << result.reason;
    return ErrorType::NoError;
}
