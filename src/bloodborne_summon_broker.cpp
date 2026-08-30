// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include "bloodborne_summon_broker.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QMutexLocker>
#include <QSet>
#include <QStringList>

namespace Bloodborne {
namespace {

constexpr qsizetype MaxHostPlacementSize = 128;

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

struct SearchMatchChecks {
    bool differentUser = true;
    bool differentSession = true;
    bool summonDataVersion = true;
    bool summonMethod = true;
    bool areaId = true;
    bool areaRegionId = true;
    bool channelId = true;
    bool summonType = true;
    bool summonWord = true;
    bool matchingLevel = true;
    bool distance = true;

    bool Matches() const {
        return differentUser && differentSession && summonDataVersion && summonMethod && areaId &&
               areaRegionId && channelId && summonType && summonWord && matchingLevel && distance;
    }
};

SearchMatchChecks EvaluateSearchMatch(const QJsonObject& request, const QJsonObject& sign,
                                      bool anywhereSummons) {
    SearchMatchChecks checks;
    const qint64 requester = Integer(request, QStringLiteral("UserId"), -1);
    const QString requesterSession = request.value(QStringLiteral("SessionId")).toString();
    checks.differentUser = Integer(sign, QStringLiteral("UserId"), -1) != requester;
    checks.differentSession =
        sign.value(QStringLiteral("SessionId")).toString() != requesterSession;
    checks.summonDataVersion = SameIfPresent(request, sign, QStringLiteral("SummonDataVersion"));
    checks.summonMethod = SameIfPresent(request, sign, QStringLiteral("SummonMethod"));
    checks.areaId = anywhereSummons || SameIfPresent(request, sign, QStringLiteral("AreaId"));
    checks.areaRegionId =
        anywhereSummons || SameIfPresent(request, sign, QStringLiteral("AreaRegionId"));
    checks.channelId = anywhereSummons || SameIfPresent(request, sign, QStringLiteral("ChannelId"));

    QSet<int> summonTypes;
    for (const QJsonValue& value : request.value(QStringLiteral("SummonTypeList")).toArray()) {
        const QJsonObject filter = value.toObject();
        if (filter.contains(QStringLiteral("SummonType"))) {
            summonTypes.insert(static_cast<int>(Integer(filter, QStringLiteral("SummonType"))));
        }
    }
    checks.summonType =
        summonTypes.isEmpty() ||
        summonTypes.contains(static_cast<int>(Integer(sign, QStringLiteral("SummonType"))));

    const QString requestWord = request.value(QStringLiteral("SummonWord")).toString();
    const QString signWord = sign.value(QStringLiteral("SummonWord")).toString();
    checks.summonWord = requestWord.isEmpty() || requestWord == signWord;

    const qint64 requestLevel = Integer(request, QStringLiteral("MatchingLevel"), -1);
    const qint64 signLevel = Integer(sign, QStringLiteral("MatchingLevel"), -1);
    if (!anywhereSummons && requestWord.isEmpty() && requestLevel >= 0 && signLevel >= 0) {
        const qint64 levelRange = 10 + requestLevel / 5;
        checks.matchingLevel = std::abs(requestLevel - signLevel) <= levelRange;
    }

    const qint64 distance = Integer(request, QStringLiteral("DistanceThreshold"), -1);
    if (!anywhereSummons && distance >= 0 && request.contains(QStringLiteral("PosX")) &&
        request.contains(QStringLiteral("PosY")) && request.contains(QStringLiteral("PosZ"))) {
        const qint64 deltaX =
            Integer(request, QStringLiteral("PosX")) - Integer(sign, QStringLiteral("PosX"));
        const qint64 deltaY =
            Integer(request, QStringLiteral("PosY")) - Integer(sign, QStringLiteral("PosY"));
        const qint64 deltaZ =
            Integer(request, QStringLiteral("PosZ")) - Integer(sign, QStringLiteral("PosZ"));
        checks.distance =
            deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ <= distance * distance;
    }
    return checks;
}

bool SummonMatchTraceEnabled() {
    return qgetenv("SHADNET_BLOODBORNE_SUMMON_TRACE").trimmed() == "1";
}

QString StateName(SummonBroker::State state) {
    switch (state) {
    case SummonBroker::State::Advertised:
        return QStringLiteral("ADVERTISED");
    case SummonBroker::State::Preparing:
        return QStringLiteral("PREPARING");
    case SummonBroker::State::Claimed:
        return QStringLiteral("CLAIMED");
    case SummonBroker::State::Delivered:
        return QStringLiteral("DELIVERED");
    case SummonBroker::State::Consumed:
        return QStringLiteral("CONSUMED");
    }
    return QStringLiteral("UNKNOWN");
}

QString PassFail(bool passed) {
    return passed ? QStringLiteral("PASS") : QStringLiteral("FAIL");
}

QJsonObject TraceFields(const QJsonObject& source, bool request) {
    QJsonObject fields;
    const QStringList commonKeys = {
        QStringLiteral("SessionId"),    QStringLiteral("UserId"),
        QStringLiteral("AreaId"),       QStringLiteral("AreaRegionId"),
        QStringLiteral("ChannelId"),    QStringLiteral("SummonDataVersion"),
        QStringLiteral("SummonMethod"), QStringLiteral("MatchingLevel"),
        QStringLiteral("PosX"),         QStringLiteral("PosY"),
        QStringLiteral("PosZ"),
    };
    for (const QString& key : commonKeys) {
        fields.insert(key, source.contains(key) ? source.value(key) : QJsonValue::Null);
    }
    if (request) {
        fields.insert(QStringLiteral("SummonTypeList"),
                      source.contains(QStringLiteral("SummonTypeList"))
                          ? source.value(QStringLiteral("SummonTypeList"))
                          : QJsonValue::Null);
        fields.insert(QStringLiteral("DistanceThreshold"),
                      source.contains(QStringLiteral("DistanceThreshold"))
                          ? source.value(QStringLiteral("DistanceThreshold"))
                          : QJsonValue::Null);
        fields.insert(QStringLiteral("GetMaxCount"),
                      source.contains(QStringLiteral("GetMaxCount"))
                          ? source.value(QStringLiteral("GetMaxCount"))
                          : QJsonValue::Null);
    } else {
        fields.insert(QStringLiteral("SummonType"), source.contains(QStringLiteral("SummonType"))
                                                        ? source.value(QStringLiteral("SummonType"))
                                                        : QJsonValue::Null);
    }
    fields.insert(QStringLiteral("SummonWordPresent"),
                  !source.value(QStringLiteral("SummonWord")).toString().isEmpty());
    return fields;
}

QJsonObject UnavailableChannelMetadata(qint64 channelId, const QString& observedIn) {
    QJsonObject metadata;
    metadata.insert(QStringLiteral("channel_id"), channelId);
    metadata.insert(QStringLiteral("observed_in"), observedIn);
    metadata.insert(QStringLiteral("exists_in_db"), QJsonValue::Null);
    metadata.insert(QStringLiteral("glyph"), QJsonValue::Null);
    metadata.insert(QStringLiteral("share_level"), QJsonValue::Null);
    metadata.insert(QStringLiteral("status"), QJsonValue::Null);
    metadata.insert(QStringLiteral("owner_user_id"), QJsonValue::Null);
    metadata.insert(QStringLiteral("vanilla_fixed"), QJsonValue::Null);
    metadata.insert(QStringLiteral("community"), QJsonValue::Null);
    metadata.insert(QStringLiteral("metadata_source"),
                    QStringLiteral("UNAVAILABLE_IN_SUMMON_BROKER"));
    return metadata;
}

QString FirstFailedCheck(bool stateEligible, const SearchMatchChecks& checks) {
    if (!stateEligible) {
        return QStringLiteral("CANDIDATE_STATE");
    }
    const std::pair<bool, const char*> orderedChecks[] = {
        {checks.differentUser, "DIFFERENT_USER"},
        {checks.differentSession, "DIFFERENT_SESSION"},
        {checks.summonDataVersion, "SUMMON_DATA_VERSION"},
        {checks.summonMethod, "SUMMON_METHOD"},
        {checks.areaId, "AREA_ID"},
        {checks.areaRegionId, "AREA_REGION_ID"},
        {checks.channelId, "CHANNEL_ID"},
        {checks.summonType, "SUMMON_TYPE"},
        {checks.summonWord, "SUMMON_WORD"},
        {checks.matchingLevel, "MATCHING_LEVEL"},
        {checks.distance, "DISTANCE"},
    };
    for (const auto& [passed, name] : orderedChecks) {
        if (!passed) {
            return QString::fromLatin1(name);
        }
    }
    return {};
}

void TraceSearchCandidate(const QJsonObject& request, const QJsonObject& advertisement,
                          SummonBroker::State state, bool stateEligible,
                          const SearchMatchChecks& checks, bool anywhereSummons) {
    QJsonObject trace;
    trace.insert(QStringLiteral("event"), QStringLiteral("CANDIDATE"));
    trace.insert(QStringLiteral("requester_user"), Integer(request, QStringLiteral("UserId"), -1));
    trace.insert(QStringLiteral("candidate_user"),
                 Integer(advertisement, QStringLiteral("UserId"), -1));
    trace.insert(QStringLiteral("request"), TraceFields(request, true));
    trace.insert(QStringLiteral("advertisement"), TraceFields(advertisement, false));
    trace.insert(QStringLiteral("candidate_state"), StateName(state));
    trace.insert(QStringLiteral("anywhere_summons"), anywhereSummons);

    QJsonObject traceChecks;
    traceChecks.insert(QStringLiteral("CANDIDATE_STATE"), PassFail(stateEligible));
    traceChecks.insert(QStringLiteral("DIFFERENT_USER"), PassFail(checks.differentUser));
    traceChecks.insert(QStringLiteral("DIFFERENT_SESSION"), PassFail(checks.differentSession));
    traceChecks.insert(QStringLiteral("SUMMON_DATA_VERSION"), PassFail(checks.summonDataVersion));
    traceChecks.insert(QStringLiteral("SUMMON_METHOD"), PassFail(checks.summonMethod));
    traceChecks.insert(QStringLiteral("AREA_ID"), PassFail(checks.areaId));
    traceChecks.insert(QStringLiteral("AREA_REGION_ID"), PassFail(checks.areaRegionId));
    traceChecks.insert(QStringLiteral("CHANNEL_ID"), PassFail(checks.channelId));
    traceChecks.insert(QStringLiteral("SUMMON_TYPE"), PassFail(checks.summonType));
    traceChecks.insert(QStringLiteral("SUMMON_WORD"), PassFail(checks.summonWord));
    traceChecks.insert(QStringLiteral("MATCHING_LEVEL"), PassFail(checks.matchingLevel));
    traceChecks.insert(QStringLiteral("DISTANCE"), PassFail(checks.distance));
    trace.insert(QStringLiteral("checks"), traceChecks);

    const bool accepted = stateEligible && checks.Matches();
    trace.insert(QStringLiteral("result"),
                 accepted ? QStringLiteral("ACCEPTED")
                          : QStringLiteral("ADVERTISEMENT_FOUND_BUT_FILTERED"));
    trace.insert(QStringLiteral("primary_reason"),
                 accepted ? QJsonValue::Null : QJsonValue(FirstFailedCheck(stateEligible, checks)));

    QJsonArray channelMetadata;
    const qint64 requestChannel = Integer(request, QStringLiteral("ChannelId"));
    const qint64 advertisementChannel = Integer(advertisement, QStringLiteral("ChannelId"));
    if (requestChannel != 0) {
        channelMetadata.append(
            UnavailableChannelMetadata(requestChannel, QStringLiteral("REQUEST")));
    }
    if (advertisementChannel != 0) {
        channelMetadata.append(
            UnavailableChannelMetadata(advertisementChannel, QStringLiteral("ADVERTISEMENT")));
    }
    if (!channelMetadata.isEmpty()) {
        trace.insert(QStringLiteral("channel_metadata"), channelMetadata);
    }

    qInfo().noquote() << "[BLOODBORNE_SUMMON_TRACE]"
                      << QJsonDocument(trace).toJson(QJsonDocument::Compact);
}

void TraceSearchResult(const QJsonObject& request, int advertisementsSeen, int matched,
                       int returned) {
    QJsonObject trace;
    trace.insert(QStringLiteral("event"), QStringLiteral("SEARCH_RESULT"));
    trace.insert(QStringLiteral("requester_user"), Integer(request, QStringLiteral("UserId"), -1));
    trace.insert(QStringLiteral("advertisements_seen"), advertisementsSeen);
    trace.insert(QStringLiteral("matching_candidates"), matched);
    trace.insert(QStringLiteral("get_max_count"),
                 Integer(request, QStringLiteral("GetMaxCount"), 20));
    trace.insert(QStringLiteral("returned"), returned);
    if (advertisementsSeen == 0) {
        trace.insert(QStringLiteral("result"), QStringLiteral("NO_ADVERTISEMENT"));
    } else if (returned == 0) {
        trace.insert(QStringLiteral("result"),
                     matched == 0 ? QStringLiteral("ADVERTISEMENT_FOUND_BUT_FILTERED")
                                  : QStringLiteral("ADVERTISEMENT_FOUND_BUT_RESULT_LIMITED"));
    } else {
        trace.insert(QStringLiteral("result"), QStringLiteral("ADVERTISEMENT_FOUND_AND_RETURNED"));
    }
    qInfo().noquote() << "[BLOODBORNE_SUMMON_TRACE]"
                      << QJsonDocument(trace).toJson(QJsonDocument::Compact);
}

bool IsSeamlessActiveState(SummonBroker::State state) {
    return state == SummonBroker::State::Preparing || state == SummonBroker::State::Claimed ||
           state == SummonBroker::State::Delivered;
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

bool ShouldSpoofAdvertisementField(const QByteArray& key) {
    return key == "AreaId" || key == "AreaRegionId" || key == "ChannelId" ||
           key == "MatchingLevel" || key == "PosX" || key == "PosY" || key == "PosZ";
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
                                         const QJsonObject& request, bool spoofLocation) {
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
        } else if (spoofLocation && ShouldSpoofAdvertisementField(member.key) &&
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
      m_seamlessAnywhereSummons(false) {}

SummonBroker::SummonBroker(Options options)
    : m_ttlMs(std::max<qint64>(1, options.ttlMs)),
      m_seamlessTtlMs(std::max<qint64>(m_ttlMs, options.seamlessTtlMs)),
      m_seamlessCoop(options.seamlessCoop),
      m_seamlessAnywhereSummons(options.seamlessAnywhereSummons || options.seamlessCoop) {}

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
    const bool traceEnabled = SummonMatchTraceEnabled();
    const int advertisementsSeen = static_cast<int>(m_records.size());
    for (auto it = m_records.begin(); it != m_records.end(); ++it) {
        const SearchMatchChecks checks =
            EvaluateSearchMatch(request, it->advertisement, anywhereSummons);
        const bool activeForRequester =
            m_seamlessCoop && IsSeamlessActiveState(it->state) && requester >= 0 &&
            Integer(it->claim, QStringLiteral("UserId"), -2) == requester;
        const bool stateEligible = it->state == State::Advertised || activeForRequester;
        if (traceEnabled) {
            TraceSearchCandidate(request, it->advertisement, it->state, stateEligible, checks,
                                 anywhereSummons);
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
            candidates.push_back({it->updatedAtMs, PrepareAdvertisementForSearch(
                                                       it->rawAdvertisement, it->advertisement,
                                                       request, anywhereSummons)});
        } else if (activeForRequester && checks.Matches()) {
            candidates.push_back({it->updatedAtMs, PrepareAdvertisementForSearch(
                                                       it->rawAdvertisement, it->advertisement,
                                                       request, anywhereSummons)});
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
    if (traceEnabled) {
        TraceSearchResult(request, advertisementsSeen, static_cast<int>(candidates.size()),
                          static_cast<int>(results.size()));
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
