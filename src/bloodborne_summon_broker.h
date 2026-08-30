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
    enum class State { Advertised, Preparing, Claimed, Delivered, Consumed };
    enum class ClaimStatus { Claimed, AlreadyClaimed, NotFound, Conflict };
    enum class LocationMode { Vanilla, SameRegion, SameArea };

    struct Options {
        qint64 ttlMs = 130'000;
        qint64 seamlessTtlMs = 15 * 60 * 1000;
        bool seamlessCoop = false;
        bool seamlessAnywhereSummons = false;
        LocationMode locationMode = LocationMode::Vanilla;
        bool trace = false;
    };

    struct AdvertiseResult {
        State state = State::Advertised;
        QByteArray pendingClaim;
        QByteArray pendingHostPlacement;
    };

    struct ClaimResult {
        ClaimStatus status = ClaimStatus::NotFound;
        QString targetSessionId;
        qint64 targetUserId = -1;
    };

    struct ConsumeResult {
        int consumed = 0;
        int retained = 0;
        QByteArray pendingHostPlacement;
    };

    SummonBroker();
    explicit SummonBroker(qint64 ttlMs);
    explicit SummonBroker(Options options);

    AdvertiseResult Advertise(const QJsonObject& body, const QByteArray& rawBody, qint64 nowMs);
    QList<QByteArray> Search(const QJsonObject& request, qint64 nowMs,
                             const QByteArray& hostPlacement = {});
    ClaimResult Claim(const QJsonObject& request, const QByteArray& rawRequest, qint64 nowMs,
                      const QByteArray& hostPlacement = {});
    ConsumeResult Consume(const QJsonObject& request, qint64 nowMs);

    std::optional<State> StateFor(const QString& sessionId, qint64 userId, qint64 nowMs);
    int Size(qint64 nowMs);
    bool IsSeamlessCoopEnabled() const;
    bool IsSeamlessAnywhereSummonsEnabled() const;
    LocationMode GetLocationMode() const;

private:
    struct Record {
        State state = State::Advertised;
        QJsonObject advertisement;
        QByteArray rawAdvertisement;
        QJsonObject claim;
        QByteArray rawClaim;
        QByteArray hostPlacement;
        qint64 preparationRequester = -1;
        qint64 updatedAtMs = 0;
    };

    void PurgeExpiredLocked(qint64 nowMs);

    qint64 m_ttlMs;
    qint64 m_seamlessTtlMs;
    bool m_seamlessCoop;
    bool m_seamlessAnywhereSummons;
    LocationMode m_locationMode;
    bool m_trace;
    QMutex m_mutex;
    QHash<QString, Record> m_records;
};

SummonBroker::LocationMode ParseSummonLocationMode(const QString& value, bool* valid = nullptr);
QString SummonLocationModeName(SummonBroker::LocationMode mode);
bool HasRequiredAdvertisementFields(const QJsonObject& body);
QByteArray BuildClaimDeliveryResponse(const QByteArray& rawClaim);

} // namespace Bloodborne
