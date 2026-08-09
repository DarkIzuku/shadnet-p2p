// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <optional>

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QMutex>
#include <QString>

namespace Bloodborne {

class SummonBroker {
public:
    enum class State { Advertised, Claimed, Delivered, Consumed };
    enum class ClaimStatus { Claimed, AlreadyClaimed, NotFound, Conflict };

    struct AdvertiseResult {
        State state = State::Advertised;
        QByteArray pendingClaim;
    };

    struct ClaimResult {
        ClaimStatus status = ClaimStatus::NotFound;
        QString targetSessionId;
        qint64 targetUserId = -1;
    };

    explicit SummonBroker(qint64 ttlMs = 130'000);

    AdvertiseResult Advertise(const QJsonObject& body, const QByteArray& rawBody, qint64 nowMs);
    QList<QByteArray> Search(const QJsonObject& request, qint64 nowMs);
    ClaimResult Claim(const QJsonObject& request, const QByteArray& rawRequest, qint64 nowMs);
    int Consume(const QJsonObject& request, qint64 nowMs);

    std::optional<State> StateFor(const QString& sessionId, qint64 userId, qint64 nowMs);
    int Size(qint64 nowMs);

private:
    struct Record {
        State state = State::Advertised;
        QJsonObject advertisement;
        QByteArray rawAdvertisement;
        QJsonObject claim;
        QByteArray rawClaim;
        qint64 updatedAtMs = 0;
    };

    void PurgeExpiredLocked(qint64 nowMs);

    qint64 m_ttlMs;
    QMutex m_mutex;
    QHash<QString, Record> m_records;
};

bool HasRequiredAdvertisementFields(const QJsonObject& body);
QByteArray BuildClaimDeliveryResponse(const QByteArray& rawClaim);

} // namespace Bloodborne