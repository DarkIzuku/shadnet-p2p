// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <optional>
#include <QHash>
#include <QMutex>
#include <QSet>
#include <QString>
#include <QVector>

namespace Bloodborne {

enum class SeamlessTravelPhase : quint32 {
    TravelBegin = 1,
    TravelReady = 2,
    TravelCommit = 3,
    TravelArrived = 4,
    TravelFailed = 5,
    Heartbeat = 6,
    LeaveParty = 7,
};

enum class SeamlessPartyState : quint32 {
    Connected = 1,
    TravelPreparing = 2,
    Traveling = 3,
    WorldLoading = 4,
    Rebinding = 5,
    RecoveringRoom = 6,
    RecoveringPeer = 7,
    Disconnected = 8,
};

struct SeamlessPartyMember {
    qint64 userId = 0;
    QString npid;
};

struct SeamlessRoomSnapshot {
    quint64 roomId = 0;
    qint64 leaderUserId = 0;
    QString leaderNpid;
    QVector<SeamlessPartyMember> members;
};

struct SeamlessTravelEvent {
    quint32 protocolVersion = 1;
    SeamlessTravelPhase phase = SeamlessTravelPhase::TravelBegin;
    QString partyId;
    quint64 generation = 0;
    quint64 sequenceId = 0;
    qint64 leaderUserId = 0;
    QString leaderNpid;
    quint64 activeRoomId = 0;
    quint32 sourceMap = 0;
    quint32 destinationMap = 0;
    qint32 warpParamId = -1;
    quint32 mode = 0;
    float positionX = 0.0F;
    float positionY = 0.0F;
    float positionZ = 0.0F;
    float orientation = 0.0F;
    qint64 timestampMs = 0;
    QString failureReason;
};

struct SeamlessDelivery {
    qint64 targetUserId = 0;
    SeamlessTravelEvent event;
};

struct SeamlessHandleResult {
    bool accepted = false;
    QString reason;
    QString partyId;
    quint64 generation = 0;
    quint64 sequenceId = 0;
    SeamlessPartyState state = SeamlessPartyState::Disconnected;
    QVector<SeamlessDelivery> deliveries;
};

struct SeamlessPartySnapshot {
    QString partyId;
    qint64 leaderUserId = 0;
    QString leaderNpid;
    quint64 activeRoomId = 0;
    quint64 generation = 0;
    quint64 sequenceId = 0;
    SeamlessPartyState state = SeamlessPartyState::Disconnected;
    QVector<SeamlessPartyMember> members;
};

class SeamlessPartyService {
public:
    struct Options {
        bool enabled = false;
        qint64 partyTtlMs = 15 * 60 * 1000;
        qint64 travelTimeoutMs = 90 * 1000;
        int maxMembers = 5;
    };

    SeamlessPartyService();
    explicit SeamlessPartyService(Options options);

    void SetEnabled(bool enabled);
    bool IsEnabled() const;

    SeamlessHandleResult Handle(qint64 senderUserId, const QString& senderNpid,
                                const SeamlessTravelEvent& event,
                                const std::optional<SeamlessRoomSnapshot>& room, qint64 nowMs);
    void MarkRoomLost(qint64 userId, quint64 roomId, qint64 nowMs);
    void MarkConnected(qint64 userId, qint64 nowMs);
    void MarkDisconnected(qint64 userId, qint64 nowMs);
    std::optional<SeamlessPartySnapshot> SnapshotForUser(qint64 userId, qint64 nowMs);
    int Size(qint64 nowMs);

private:
    struct MemberState {
        SeamlessPartyMember identity;
        bool connected = true;
        bool roomPresent = true;
        qint64 lastSeenMs = 0;
    };

    struct Party {
        QString id;
        qint64 leaderUserId = 0;
        QString leaderNpid;
        quint64 activeRoomId = 0;
        quint64 generation = 1;
        quint64 lastSequenceId = 0;
        SeamlessPartyState state = SeamlessPartyState::Connected;
        QHash<qint64, MemberState> members;
        QSet<qint64> readyMembers;
        QSet<qint64> arrivedMembers;
        bool readyNotificationSent = false;
        std::optional<SeamlessTravelEvent> activeTravel;
        qint64 lastActivityMs = 0;
        qint64 travelStartedMs = 0;
    };

    void PurgeExpiredLocked(qint64 nowMs);
    void RemovePartyLocked(const QString& partyId);
    SeamlessHandleResult RejectLocked(const QString& reason) const;
    SeamlessHandleResult AcceptLocked(const Party& party, const QString& reason = {}) const;
    void DeliverToMembersLocked(const Party& party, qint64 exceptUserId,
                                const SeamlessTravelEvent& event,
                                QVector<SeamlessDelivery>& deliveries) const;
    std::optional<QString> PartyIdForUserLocked(qint64 userId) const;

    mutable QMutex m_mutex;
    Options m_options;
    QHash<QString, Party> m_parties;
    QHash<qint64, QString> m_userToParty;
};

} // namespace Bloodborne
