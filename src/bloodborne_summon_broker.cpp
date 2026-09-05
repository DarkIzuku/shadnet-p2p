// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include "bloodborne_summon_broker.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QMutexLocker>
#include <QSet>

namespace Bloodborne {
namespace {

constexpr qsizetype MaxHostPlacementSize = 128;
constexpr qint64 HuntersDreamAreaId = 352321536;
constexpr qint64 HuntersDreamAreaRegionId = 210000;

qint64 Integer(const QJsonObject& object, const QString& key, qint64 fallback = 0) {
    const QJsonValue value = object.value(key);
    return value.isDouble() ? value.toVariant().toLongLong() : fallback;
}

std::optional<qint64> OptionalInteger(const QJsonObject& object, const QString& key) {
    const QJsonValue value = object.value(key);
    if (!value.isDouble()) {
        return std::nullopt;
    }
    return value.toVariant().toLongLong();
}

QString RecordKey(const QString& sessionId, qint64 userId) {
    return sessionId + QLatin1Char(':') + QString::number(userId);
}

QString RecordKey(const QJsonObject& advertisement) {
    return RecordKey(advertisement.value(QStringLiteral("SessionId")).toString(),
                     Integer(advertisement, QStringLiteral("UserId"), -1));
}

bool SameIfPresent(const QJsonObject& request, const QJsonObject& sign, const QString& key) {
    const QJsonValue expected = request.value(key);
    return expected.isUndefined() || expected.isNull() || expected == sign.value(key);
}

bool SameRequired(const QJsonObject& request, const QJsonObject& sign, const QString& key) {
    const QJsonValue expected = request.value(key);
    const QJsonValue actual = sign.value(key);
    return !expected.isUndefined() && !expected.isNull() && !actual.isUndefined() &&
           !actual.isNull() && expected == actual;
}

bool IsSeamlessActiveState(SummonBroker::State state) {
    return state == SummonBroker::State::Preparing || state == SummonBroker::State::Claimed ||
           state == SummonBroker::State::Delivered;
}

bool IsMakeshiftQuickSearchPair(const QJsonObject& request, const QJsonObject& candidate) {
    const std::optional<qint64> requestMethod =
        OptionalInteger(request, QStringLiteral("SummonMethod"));
    const std::optional<qint64> candidateMethod =
        OptionalInteger(candidate, QStringLiteral("SummonMethod"));
    const std::optional<qint64> requestArea = OptionalInteger(request, QStringLiteral("AreaId"));
    const std::optional<qint64> candidateArea =
        OptionalInteger(candidate, QStringLiteral("AreaId"));
    const std::optional<qint64> candidateRegion =
        OptionalInteger(candidate, QStringLiteral("AreaRegionId"));
    const std::optional<qint64> requestChannel =
        OptionalInteger(request, QStringLiteral("ChannelId"));
    const std::optional<qint64> candidateChannel =
        OptionalInteger(candidate, QStringLiteral("ChannelId"));

    // The captured flow is asymmetric. The Chalice host performs GetList with
    // Method=0 and the real destination ChannelId, while the hunter waiting at
    // the Makeshift Altar advertises Method=1 from Hunter's Dream with
    // ChannelId=0. Requiring this complete shape keeps ChannelId=0 and Method=1
    // from becoming global wildcards for ordinary summons.
    return requestMethod == 0 && candidateMethod == 1 && requestArea.has_value() &&
           *requestArea != HuntersDreamAreaId && requestChannel.has_value() &&
           *requestChannel > 0 && candidateArea == HuntersDreamAreaId &&
           candidateRegion == HuntersDreamAreaRegionId && candidateChannel == 0;
}

bool ForceConsumeRequested(const QJsonObject& request) {
    return request.value(QStringLiteral("Force")).toBool(false) ||
           request.value(QStringLiteral("SeamlessLeave")).toBool(false);
}

std::optional<qint64> HostPlacementMap(const QByteArray& placement) {
    constexpr qsizetype MapBegin = 2;
    if (!placement.startsWith("1,")) {
        return std::nullopt;
    }
    const qsizetype end = placement.indexOf(',', MapBegin);
    if (end < 0) {
        return std::nullopt;
    }

    bool ok = false;
    const qulonglong packedMap = placement.mid(MapBegin, end - MapBegin).toULongLong(&ok, 16);
    if (!ok || packedMap == 0 || packedMap > std::numeric_limits<quint32>::max()) {
        return std::nullopt;
    }
    return static_cast<qint64>(packedMap);
}

struct SearchMatchChecks {
    bool differentUser = true;
    bool differentSession = true;
    bool version = true;
    bool method = true;
    bool area = true;
    bool region = true;
    bool channel = true;
    bool summonType = true;
    bool password = true;
    bool level = true;
    bool distance = true;
    bool makeshiftQuickSearch = false;

    bool Matches() const {
        return differentUser && differentSession && version && method && area && region &&
               channel && summonType && password && level && distance;
    }
};

SearchMatchChecks EvaluateSearchMatch(const QJsonObject& request, const QJsonObject& sign,
                                      bool anywhereSummons,
                                      SummonBroker::LocationMode locationMode) {
    SearchMatchChecks checks;
    const qint64 requester = Integer(request, QStringLiteral("UserId"), -1);
    const QString requesterSession = request.value(QStringLiteral("SessionId")).toString();
    checks.differentUser = Integer(sign, QStringLiteral("UserId"), -1) != requester;
    checks.differentSession =
        sign.value(QStringLiteral("SessionId")).toString() != requesterSession;
    checks.version = SameIfPresent(request, sign, QStringLiteral("SummonDataVersion"));
    checks.makeshiftQuickSearch = IsMakeshiftQuickSearchPair(request, sign);
    checks.method =
        checks.makeshiftQuickSearch || SameIfPresent(request, sign, QStringLiteral("SummonMethod"));
    if (!anywhereSummons && !checks.makeshiftQuickSearch) {
        checks.area = SameRequired(request, sign, QStringLiteral("AreaId"));
        checks.region = locationMode == SummonBroker::LocationMode::SameArea ||
                        SameRequired(request, sign, QStringLiteral("AreaRegionId"));
        // Channel identity is deliberately mandatory in every classic location mode. Root
        // Chalices with different channels must never share summon advertisements.
        checks.channel = SameRequired(request, sign, QStringLiteral("ChannelId"));
    }

    QSet<int> summonTypes;
    for (const QJsonValue& value : request.value(QStringLiteral("SummonTypeList")).toArray()) {
        const QJsonObject filter = value.toObject();
        if (filter.contains(QStringLiteral("SummonType"))) {
            summonTypes.insert(static_cast<int>(Integer(filter, QStringLiteral("SummonType"))));
        }
    }
    if (!summonTypes.isEmpty() &&
        !summonTypes.contains(static_cast<int>(Integer(sign, QStringLiteral("SummonType"))))) {
        checks.summonType = false;
    }

    const QString requestWord = request.value(QStringLiteral("SummonWord")).toString();
    const QString signWord = sign.value(QStringLiteral("SummonWord")).toString();
    if ((checks.makeshiftQuickSearch && requestWord != signWord) ||
        (!checks.makeshiftQuickSearch && !requestWord.isEmpty() && requestWord != signWord)) {
        checks.password = false;
    }

    const qint64 requestLevel = Integer(request, QStringLiteral("MatchingLevel"), -1);
    const qint64 signLevel = Integer(sign, QStringLiteral("MatchingLevel"), -1);
    if (!anywhereSummons && (checks.makeshiftQuickSearch || requestWord.isEmpty()) &&
        requestLevel >= 0 && signLevel >= 0) {
        const qint64 levelRange = 10 + requestLevel / 5;
        if (std::abs(requestLevel - signLevel) > levelRange) {
            checks.level = false;
        }
    }

    const qint64 distance = Integer(request, QStringLiteral("DistanceThreshold"), -1);
    if (!anywhereSummons && !checks.makeshiftQuickSearch &&
        locationMode == SummonBroker::LocationMode::Vanilla && distance >= 0 &&
        request.contains(QStringLiteral("PosX")) && request.contains(QStringLiteral("PosY")) &&
        request.contains(QStringLiteral("PosZ"))) {
        const qint64 deltaX =
            Integer(request, QStringLiteral("PosX")) - Integer(sign, QStringLiteral("PosX"));
        const qint64 deltaY =
            Integer(request, QStringLiteral("PosY")) - Integer(sign, QStringLiteral("PosY"));
        const qint64 deltaZ =
            Integer(request, QStringLiteral("PosZ")) - Integer(sign, QStringLiteral("PosZ"));
        if (deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ > distance * distance) {
            checks.distance = false;
        }
    }
    return checks;
}

QJsonObject SearchTrace(const QJsonObject& request, const QJsonObject& sign,
                        const SearchMatchChecks& checks, SummonBroker::State state,
                        SummonBroker::LocationMode locationMode, bool anywhereSummons) {
    QJsonObject trace;
    trace.insert(QStringLiteral("request_user_id"), Integer(request, QStringLiteral("UserId"), -1));
    trace.insert(QStringLiteral("candidate_user_id"), Integer(sign, QStringLiteral("UserId"), -1));
    trace.insert(QStringLiteral("state"), static_cast<int>(state));
    trace.insert(QStringLiteral("location_mode"), SummonLocationModeName(locationMode));
    trace.insert(QStringLiteral("seamless_anywhere"), anywhereSummons);
    trace.insert(QStringLiteral("makeshift_quick_search"), checks.makeshiftQuickSearch);
    trace.insert(QStringLiteral("request_area"), Integer(request, QStringLiteral("AreaId"), -1));
    trace.insert(QStringLiteral("candidate_area"), Integer(sign, QStringLiteral("AreaId"), -1));
    trace.insert(QStringLiteral("request_region"),
                 Integer(request, QStringLiteral("AreaRegionId"), -1));
    trace.insert(QStringLiteral("candidate_region"),
                 Integer(sign, QStringLiteral("AreaRegionId"), -1));
    trace.insert(QStringLiteral("request_channel"),
                 Integer(request, QStringLiteral("ChannelId"), -1));
    trace.insert(QStringLiteral("candidate_channel"),
                 Integer(sign, QStringLiteral("ChannelId"), -1));
    QJsonObject filters;
    filters.insert(QStringLiteral("different_user"), checks.differentUser);
    filters.insert(QStringLiteral("different_session"), checks.differentSession);
    filters.insert(QStringLiteral("version"), checks.version);
    filters.insert(QStringLiteral("method"), checks.method);
    filters.insert(QStringLiteral("area"), checks.area);
    filters.insert(QStringLiteral("region"), checks.region);
    filters.insert(QStringLiteral("channel"), checks.channel);
    filters.insert(QStringLiteral("summon_type"), checks.summonType);
    filters.insert(QStringLiteral("password"), checks.password);
    filters.insert(QStringLiteral("level"), checks.level);
    filters.insert(QStringLiteral("distance"), checks.distance);
    trace.insert(QStringLiteral("filters"), filters);
    trace.insert(QStringLiteral("accepted"), checks.Matches());
    return trace;
}

struct TopLevelMember {
    QByteArray key;
    QByteArray raw;
};

QList<TopLevelMember> TopLevelMembers(const QByteArray& rawObject) {
    const QByteArray raw = rawObject.trimmed();
    QList<TopLevelMember> members;
    if (raw.size() < 2 || raw.front() != '{' || raw.back() != '}') {
        return members;
    }

    int position = 1;
    const int end = raw.size() - 1;
    while (position < end) {
        while (position < end &&
               (raw[position] == ',' || raw[position] == ' ' || raw[position] == '\t' ||
                raw[position] == '\r' || raw[position] == '\n')) {
            ++position;
        }
        if (position >= end) {
            break;
        }

        const int memberStart = position;
        if (raw[position] != '"') {
            return {};
        }
        const int keyStart = ++position;
        bool escaped = false;
        while (position < end) {
            const char current = raw[position];
            if (!escaped && current == '"') {
                break;
            }
            escaped = !escaped && current == '\\';
            if (current != '\\') {
                escaped = false;
            }
            ++position;
        }
        if (position >= end) {
            return {};
        }
        const QByteArray key = raw.mid(keyStart, position - keyStart);
        ++position;
        while (position < end && (raw[position] == ' ' || raw[position] == '\t' ||
                                  raw[position] == '\r' || raw[position] == '\n')) {
            ++position;
        }
        if (position >= end || raw[position] != ':') {
            return {};
        }
        ++position;

        bool inString = false;
        escaped = false;
        int objectDepth = 0;
        int arrayDepth = 0;
        while (position < end) {
            const char current = raw[position];
            if (inString) {
                if (!escaped && current == '"') {
                    inString = false;
                }
                escaped = !escaped && current == '\\';
                if (current != '\\') {
                    escaped = false;
                }
            } else {
                if (current == '"') {
                    inString = true;
                    escaped = false;
                } else if (current == '{') {
                    ++objectDepth;
                } else if (current == '}') {
                    --objectDepth;
                } else if (current == '[') {
                    ++arrayDepth;
                } else if (current == ']') {
                    --arrayDepth;
                } else if (current == ',' && objectDepth == 0 && arrayDepth == 0) {
                    break;
                }
            }
            ++position;
        }

        int memberEnd = position;
        while (memberEnd > memberStart &&
               (raw[memberEnd - 1] == ' ' || raw[memberEnd - 1] == '\t' ||
                raw[memberEnd - 1] == '\r' || raw[memberEnd - 1] == '\n')) {
            --memberEnd;
        }
        members.append({key, raw.mid(memberStart, memberEnd - memberStart)});
    }
    return members;
}

QList<QByteArray> TopLevelMembersExceptEnvelope(const QByteArray& rawObject) {
    QList<QByteArray> out;
    for (const TopLevelMember& member : TopLevelMembers(rawObject)) {
        if (member.key != "MessageId" && member.key != "ResKind") {
            out.append(member.raw);
        }
    }
    return out;
}

QByteArray CompactJsonValue(const QJsonValue& value) {
    QJsonObject wrapper;
    wrapper.insert(QStringLiteral("_"), value);
    const QByteArray raw = QJsonDocument(wrapper).toJson(QJsonDocument::Compact);
    const int colon = raw.indexOf(':');
    if (colon < 0 || raw.size() < colon + 2 || raw.back() != '}') {
        return {};
    }
    return raw.mid(colon + 1, raw.size() - colon - 2);
}

QByteArray BuildRawMember(const QByteArray& key, const QJsonValue& value) {
    const QByteArray rawValue = CompactJsonValue(value);
    if (rawValue.isEmpty()) {
        return {};
    }
    QByteArray out;
    out.reserve(key.size() + rawValue.size() + 4);
    out.append('"');
    out.append(key);
    out.append("\":");
    out.append(rawValue);
    return out;
}

bool IsAdvertisementLocationField(const QByteArray& key) {
    return key == "AreaId" || key == "AreaRegionId" || key == "ChannelId";
}

bool ShouldSpoofAdvertisementField(const QByteArray& key, bool locationOnly) {
    return IsAdvertisementLocationField(key) ||
           (!locationOnly &&
            (key == "MatchingLevel" || key == "PosX" || key == "PosY" || key == "PosZ"));
}

std::optional<QString> SummonDataWithAvailableResult(const QJsonObject& advertisement) {
    constexpr qsizetype PayloadSize = 0xE0;
    constexpr qsizetype AvailableResultCountOffset = 0x79;
    if (Integer(advertisement, QStringLiteral("SummonDataVersion"), -1) != 3) {
        return std::nullopt;
    }

    const QByteArray encoded =
        advertisement.value(QStringLiteral("SummonData")).toString().toLatin1();
    QByteArray payload = QByteArray::fromBase64(encoded, QByteArray::AbortOnBase64DecodingErrors);
    if (payload.size() != PayloadSize || payload[AvailableResultCountOffset] != 0) {
        return std::nullopt;
    }

    // Bloodborne advertises this response-side count as zero. A search result represents one
    // available candidate; the native manager consumes one while creating its pending record.
    payload[AvailableResultCountOffset] = 1;
    return QString::fromLatin1(payload.toBase64());
}

QByteArray PrepareAdvertisementForSearch(const QByteArray& rawAdvertisement,
                                         const QJsonObject& advertisement,
                                         const QJsonObject& request, bool spoofLocation,
                                         bool locationOnly = false) {
    const QList<TopLevelMember> members = TopLevelMembers(rawAdvertisement);
    if (members.isEmpty()) {
        return rawAdvertisement;
    }

    const std::optional<QString> summonData = SummonDataWithAvailableResult(advertisement);
    QSet<QByteArray> emitted;
    QByteArray out;
    out.append('{');
    bool first = true;
    auto appendMember = [&](const QByteArray& rawMember) {
        if (rawMember.isEmpty()) {
            return;
        }
        if (!first) {
            out.append(',');
        }
        out.append(rawMember);
        first = false;
    };

    for (const TopLevelMember& member : members) {
        emitted.insert(member.key);
        const QString key = QString::fromLatin1(member.key);
        const QJsonValue replacement = request.value(key);
        if (member.key == "SummonData" && summonData.has_value()) {
            appendMember(BuildRawMember(member.key, *summonData));
        } else if (spoofLocation && ShouldSpoofAdvertisementField(member.key, locationOnly) &&
                   !replacement.isUndefined() && !replacement.isNull()) {
            appendMember(BuildRawMember(member.key, replacement));
        } else {
            appendMember(member.raw);
        }
    }

    if (spoofLocation) {
        for (const QByteArray& key : {QByteArray("AreaId"), QByteArray("AreaRegionId"),
                                      QByteArray("ChannelId"), QByteArray("MatchingLevel"),
                                      QByteArray("PosX"), QByteArray("PosY"), QByteArray("PosZ")}) {
            if (emitted.contains(key)) {
                continue;
            }
            const QJsonValue replacement = request.value(QString::fromLatin1(key));
            if (!replacement.isUndefined() && !replacement.isNull()) {
                appendMember(BuildRawMember(key, replacement));
            }
        }
    }
    out.append('}');
    return out;
}

} // namespace

SummonBroker::SummonBroker() : SummonBroker(Options{}) {}

SummonBroker::SummonBroker(qint64 ttlMs)
    : m_ttlMs(std::max<qint64>(1, ttlMs)), m_seamlessTtlMs(15 * 60 * 1000), m_seamlessCoop(false),
      m_seamlessAnywhereSummons(false), m_locationMode(LocationMode::Vanilla), m_trace(false) {}

SummonBroker::SummonBroker(Options options)
    : m_ttlMs(std::max<qint64>(1, options.ttlMs)),
      m_seamlessTtlMs(std::max<qint64>(m_ttlMs, options.seamlessTtlMs)),
      m_seamlessCoop(options.seamlessCoop),
      m_seamlessAnywhereSummons(options.seamlessAnywhereSummons || options.seamlessCoop),
      m_locationMode(options.locationMode), m_trace(options.trace) {}

SummonBroker::AdvertiseResult SummonBroker::Advertise(const QJsonObject& body,
                                                      const QByteArray& rawBody, qint64 nowMs) {
    QMutexLocker lock(&m_mutex);
    PurgeExpiredLocked(nowMs);

    const QString key = RecordKey(body);
    auto it = m_records.find(key);
    if (it == m_records.end() || it->state == State::Consumed) {
        Record record;
        record.state = State::Advertised;
        record.advertisement = body;
        record.rawAdvertisement = rawBody;
        record.updatedAtMs = nowMs;
        m_records.insert(key, std::move(record));
        return {State::Advertised, {}, {}};
    }

    it->advertisement = body;
    it->rawAdvertisement = rawBody;
    it->updatedAtMs = nowMs;
    if (it->state == State::Preparing) {
        const std::optional<qint64> hostMap = HostPlacementMap(it->hostPlacement);
        if (hostMap.has_value() && Integer(body, QStringLiteral("AreaId"), -1) != *hostMap) {
            return {State::Preparing, {}, it->hostPlacement};
        }
        it->state = State::Advertised;
        it->preparationRequester = -1;
    }
    if (it->state == State::Claimed) {
        const std::optional<qint64> hostMap = HostPlacementMap(it->hostPlacement);
        if (m_seamlessCoop && hostMap.has_value() &&
            Integer(body, QStringLiteral("AreaId"), -1) != *hostMap) {
            return {State::Claimed, {}, it->hostPlacement};
        }
        it->state = State::Delivered;
    }
    if (it->state == State::Delivered) {
        return {State::Delivered, it->rawClaim, it->hostPlacement};
    }
    return {it->state, {}, {}};
}

QList<QByteArray> SummonBroker::Search(const QJsonObject& request, qint64 nowMs,
                                       const QByteArray& hostPlacement) {
    QMutexLocker lock(&m_mutex);
    PurgeExpiredLocked(nowMs);

    struct Candidate {
        qint64 updatedAtMs;
        QByteArray rawAdvertisement;
    };
    std::vector<Candidate> candidates;
    const qint64 requester = Integer(request, QStringLiteral("UserId"), -1);
    const bool anywhereSummons = m_seamlessCoop && m_seamlessAnywhereSummons;
    const std::optional<qint64> hostMap = HostPlacementMap(hostPlacement);
    for (auto it = m_records.begin(); it != m_records.end(); ++it) {
        const SearchMatchChecks checks =
            EvaluateSearchMatch(request, it->advertisement, anywhereSummons, m_locationMode);
        if (m_trace) {
            qInfo().noquote() << "[BLOODBORNE SUMMON TRACE] candidate"
                              << QJsonDocument(SearchTrace(request, it->advertisement, checks,
                                                           it->state, m_locationMode,
                                                           anywhereSummons))
                                     .toJson(QJsonDocument::Compact);
        }
        if (it->state == State::Advertised && checks.Matches()) {
            if (anywhereSummons && hostMap.has_value() && requester >= 0 &&
                Integer(it->advertisement, QStringLiteral("AreaId"), -1) != *hostMap) {
                it->state = State::Preparing;
                it->hostPlacement = hostPlacement;
                it->preparationRequester = requester;
                it->updatedAtMs = nowMs;
                continue;
            }
            // A Makeshift advertisement does not know its destination yet. Keep
            // the candidate identity and payload, but return the host's real
            // Chalice location/channel so the client never treats ChannelId=0 as
            // the destination.
            const bool useRequestLocation = anywhereSummons || checks.makeshiftQuickSearch;
            candidates.push_back(
                {it->updatedAtMs,
                 PrepareAdvertisementForSearch(it->rawAdvertisement, it->advertisement, request,
                                               useRequestLocation, checks.makeshiftQuickSearch)});
        } else if (m_seamlessCoop && IsSeamlessActiveState(it->state) && requester >= 0 &&
                   Integer(it->claim, QStringLiteral("UserId"), -2) == requester &&
                   checks.Matches()) {
            const bool useRequestLocation = anywhereSummons || checks.makeshiftQuickSearch;
            candidates.push_back(
                {it->updatedAtMs,
                 PrepareAdvertisementForSearch(it->rawAdvertisement, it->advertisement, request,
                                               useRequestLocation, checks.makeshiftQuickSearch)});
        }
    }
    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& left, const Candidate& right) {
                  return left.updatedAtMs > right.updatedAtMs;
              });

    const int maxCount =
        std::max(0, static_cast<int>(Integer(request, QStringLiteral("GetMaxCount"), 20)));
    QList<QByteArray> results;
    for (const Candidate& candidate : candidates) {
        if (results.size() >= maxCount) {
            break;
        }
        results.append(candidate.rawAdvertisement);
    }
    return results;
}

SummonBroker::ClaimResult SummonBroker::Claim(const QJsonObject& request,
                                              const QByteArray& rawRequest, qint64 nowMs,
                                              const QByteArray& hostPlacement) {
    QMutexLocker lock(&m_mutex);
    PurgeExpiredLocked(nowMs);

    const QString requestedSession = request.value(QStringLiteral("SessionId")).toString();
    const std::optional<qint64> targetUser =
        OptionalInteger(request, QStringLiteral("TargetUserId"));
    const QJsonValue targetChara = request.value(QStringLiteral("TargetCharaId"));

    auto target = m_records.end();
    if (!requestedSession.isEmpty()) {
        for (auto it = m_records.begin(); it != m_records.end(); ++it) {
            if (it->advertisement.value(QStringLiteral("SessionId")).toString() ==
                requestedSession) {
                target = it;
                break;
            }
        }
    }
    if (target == m_records.end() && targetUser.has_value()) {
        for (auto it = m_records.begin(); it != m_records.end(); ++it) {
            if (Integer(it->advertisement, QStringLiteral("UserId"), -1) != *targetUser) {
                continue;
            }
            if (!targetChara.isUndefined() && !targetChara.isNull() &&
                it->advertisement.value(QStringLiteral("CharaId")) != targetChara) {
                continue;
            }
            if (target == m_records.end() || it->updatedAtMs > target->updatedAtMs) {
                target = it;
            }
        }
    }
    if (target == m_records.end() || target->state == State::Consumed) {
        return {ClaimStatus::NotFound, {}, -1};
    }

    ClaimResult result;
    result.targetSessionId = target->advertisement.value(QStringLiteral("SessionId")).toString();
    result.targetUserId = Integer(target->advertisement, QStringLiteral("UserId"), -1);
    if (target->state == State::Claimed || target->state == State::Delivered) {
        result.status =
            target->claim == request ? ClaimStatus::AlreadyClaimed : ClaimStatus::Conflict;
        return result;
    }

    target->state = State::Claimed;
    target->claim = request;
    target->rawClaim = rawRequest;
    if (m_seamlessCoop && !hostPlacement.isEmpty() &&
        hostPlacement.size() <= MaxHostPlacementSize && !hostPlacement.contains('\r') &&
        !hostPlacement.contains('\n')) {
        target->hostPlacement = hostPlacement;
    } else {
        target->hostPlacement.clear();
    }
    target->updatedAtMs = nowMs;
    result.status = ClaimStatus::Claimed;
    return result;
}

SummonBroker::ConsumeResult SummonBroker::Consume(const QJsonObject& request, qint64 nowMs) {
    QMutexLocker lock(&m_mutex);
    PurgeExpiredLocked(nowMs);

    const QString sessionId = request.value(QStringLiteral("SessionId")).toString();
    const std::optional<qint64> userId = OptionalInteger(request, QStringLiteral("UserId"));
    if (sessionId.isEmpty() && !userId.has_value()) {
        return {};
    }

    ConsumeResult result;
    const bool forceConsume = ForceConsumeRequested(request);
    for (auto it = m_records.begin(); it != m_records.end(); ++it) {
        const bool sessionMatches =
            sessionId.isEmpty() ||
            it->advertisement.value(QStringLiteral("SessionId")).toString() == sessionId;
        const bool userMatches =
            !userId.has_value() ||
            Integer(it->advertisement, QStringLiteral("UserId"), -1) == *userId;
        if (sessionMatches && userMatches && it->state != State::Consumed) {
            it->updatedAtMs = nowMs;
            if (m_seamlessCoop && !forceConsume && IsSeamlessActiveState(it->state)) {
                ++result.retained;
                if (!it->hostPlacement.isEmpty()) {
                    result.pendingHostPlacement = it->hostPlacement;
                }
                continue;
            }
            it->state = State::Consumed;
            ++result.consumed;
        }
    }
    return result;
}

std::optional<SummonBroker::State> SummonBroker::StateFor(const QString& sessionId, qint64 userId,
                                                          qint64 nowMs) {
    QMutexLocker lock(&m_mutex);
    PurgeExpiredLocked(nowMs);
    const auto it = m_records.constFind(RecordKey(sessionId, userId));
    if (it == m_records.cend()) {
        return std::nullopt;
    }
    return it->state;
}

int SummonBroker::Size(qint64 nowMs) {
    QMutexLocker lock(&m_mutex);
    PurgeExpiredLocked(nowMs);
    return m_records.size();
}

bool SummonBroker::IsSeamlessCoopEnabled() const {
    return m_seamlessCoop;
}

bool SummonBroker::IsSeamlessAnywhereSummonsEnabled() const {
    return m_seamlessAnywhereSummons;
}

SummonBroker::LocationMode SummonBroker::GetLocationMode() const {
    return m_locationMode;
}

void SummonBroker::PurgeExpiredLocked(qint64 nowMs) {
    for (auto it = m_records.begin(); it != m_records.end();) {
        const qint64 ttl =
            m_seamlessCoop && IsSeamlessActiveState(it->state) ? m_seamlessTtlMs : m_ttlMs;
        if (nowMs - it->updatedAtMs > ttl) {
            it = m_records.erase(it);
        } else {
            ++it;
        }
    }
}

SummonBroker::LocationMode ParseSummonLocationMode(const QString& value, bool* valid) {
    const QString normalized = value.trimmed();
    if (normalized.compare(QStringLiteral("Vanilla"), Qt::CaseInsensitive) == 0) {
        if (valid != nullptr) {
            *valid = true;
        }
        return SummonBroker::LocationMode::Vanilla;
    }
    if (normalized.compare(QStringLiteral("SameRegion"), Qt::CaseInsensitive) == 0) {
        if (valid != nullptr) {
            *valid = true;
        }
        return SummonBroker::LocationMode::SameRegion;
    }
    if (normalized.compare(QStringLiteral("SameArea"), Qt::CaseInsensitive) == 0) {
        if (valid != nullptr) {
            *valid = true;
        }
        return SummonBroker::LocationMode::SameArea;
    }
    if (valid != nullptr) {
        *valid = false;
    }
    return SummonBroker::LocationMode::Vanilla;
}

QString SummonLocationModeName(SummonBroker::LocationMode mode) {
    switch (mode) {
    case SummonBroker::LocationMode::Vanilla:
        return QStringLiteral("Vanilla");
    case SummonBroker::LocationMode::SameRegion:
        return QStringLiteral("SameRegion");
    case SummonBroker::LocationMode::SameArea:
        return QStringLiteral("SameArea");
    }
    return QStringLiteral("Vanilla");
}

bool HasRequiredAdvertisementFields(const QJsonObject& body) {
    static const QSet<QString> required = {
        QStringLiteral("SessionId"),  QStringLiteral("UserId"),
        QStringLiteral("AreaId"),     QStringLiteral("AreaRegionId"),
        QStringLiteral("SummonData"), QStringLiteral("SummonDataVersion"),
        QStringLiteral("SummonType"), QStringLiteral("MatchingLevel"),
    };
    for (const QString& key : required) {
        if (!body.contains(key) || body.value(key).isNull()) {
            return false;
        }
    }
    return !body.value(QStringLiteral("SessionId")).toString().isEmpty() &&
           !body.value(QStringLiteral("SummonData")).toString().isEmpty();
}

QByteArray BuildClaimDeliveryResponse(const QByteArray& rawClaim) {
    const QList<QByteArray> members = TopLevelMembersExceptEnvelope(rawClaim);
    if (members.isEmpty()) {
        return {};
    }

    // Keep game-owned 64-bit identifiers byte-for-byte instead of round-tripping through
    // QJsonValue's double representation.
    QByteArray response = "{\"ResKind\":0,\"MessageId\":\"SummonDataCreateResponse\"";
    for (const QByteArray& member : members) {
        response.append(',');
        response.append(member);
    }
    response.append('}');
    return response;
}

} // namespace Bloodborne
