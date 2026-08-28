// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <array>

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTypes>

namespace BloodborneTestFixtures {

// Captured from the reference Bloodborne backend. The session value is
// redacted; HTTP route tests replace the envelope identity with the session
// created by Login.
inline QJsonObject OfficialWanderingGhostGetRequest() {
  static constexpr char Request[] = R"json({
  "AreaList": [{
    "AreaId": 4294967295,
    "AreaRegionId": -1,
    "ChannelId": 0,
    "GetCount": 5
  }],
  "GetMaxCount": 10,
  "JoinedCharaIdList": [],
  "MatchingLevel": -1,
  "MessageId": "WanderingGhostGetRequest",
  "SessionId": "<captured-session-redacted>",
  "UserId": 2880,
  "WanderingGhostDataVersion": 5
})json";
  return QJsonDocument::fromJson(QByteArray(Request)).object();
}

// The object shape is synthetic and covers only the observed JSON type. Its
// keys must not be treated as the vanilla Bloodborne field contract.
inline QJsonObject ObjectJoinedCharaWanderingGhostGetRequest() {
  QJsonObject request = OfficialWanderingGhostGetRequest();
  QJsonObject syntheticJoinedChara;
  syntheticJoinedChara.insert(QStringLiteral("FixtureField"), 123);
  request.insert(QStringLiteral("JoinedCharaIdList"),
                 QJsonArray{syntheticJoinedChara});
  return request;
}

// Captured from 20260828-082944-864Z, sequence 0016. Only the session value is
// redacted; the FormData and every gameplay field are preserved exactly.
inline QJsonObject OfficialChannelUploadRequest() {
  static constexpr char Request[] = R"json({
  "CharaId": 9223372036854776000,
  "FixedOrGeneral": 1,
  "FormData": "BQAAAIwAAABhAAod1BcAAA43lwAhAAAAEzeXACMAAAAiN5cAMgAAAP///////////////////////////////////////////////////////////////0l6dWt1AAAAAAAAAAAAAABTAEEAVgBFACAAKABtAGEAbABlACkAAAAAAAAAAAAAAAAAAAA=",
  "FormDataVersion": 0,
  "HolyGrailTypeId": 0,
  "MessageId": "ChannelUploadRequest",
  "RitualLevel": 1,
  "SessionId": "<captured-session-redacted>",
  "ShareLevel": 0,
  "Status": 1,
  "SubFeatureFlag": 0,
  "UnlockFlagList": [{"UnlockFlag": 0},{"UnlockFlag": 0},{"UnlockFlag": 1}],
  "UserId": 2880,
  "WishMaterialList": []
})json";
  return QJsonDocument::fromJson(QByteArray(Request)).object();
}

// Exact fixed-ritual search that reproduced the local disconnect. Captured in
// 20260828-082944-864Z, sequence 0069; only the session is redacted.
inline QJsonObject OfficialFixedChannelSearchRequest() {
  static constexpr char Request[] = R"json({
  "FixedOrGeneralList": [{"FixedOrGeneral": 2}],
  "FormDataVersion": 0,
  "GetCount": 1,
  "HolyGrailTypeId": 0,
  "MessageId": "ChannelSearchRequest",
  "RitualLevel": 1,
  "SessionId": "<captured-session-redacted>",
  "Status": 1,
  "SubFeatureFlag": 256,
  "UnlockedFlagList": [{"UnlockedFlag": 0},{"UnlockedFlag": 1},{"UnlockedFlag": 0}],
  "UserId": 2880
})json";
  return QJsonDocument::fromJson(QByteArray(Request)).object();
}

struct OfficialFixedChalice {
  qint64 channelId;
  const char *glyph;
  int holyGrailTypeId;
  int ritualLevel;
  quint32 unlockFlag;
  const char *formData;
};

// The ten FixedOrGeneral=2 objects from sequence 0014. Keeping this expectation
// separate from the production catalog makes every FormData comparison a real
// capture-vs-database assertion rather than a fixture comparing with itself.
inline constexpr std::array<OfficialFixedChalice, 10> OfficialFixedChalices = {{
    {1, "3itx", 0, 2, 128,
     "BQAAAIwAAAAAWhQdQRgAAP////////////////////8+pk47WgAAAEimTjtbAAAA/////////"
     "/+g3UY7LQAAAP///////////////////////////////"
     "y0AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA="},
    {2, "4j56", 1, 3, 32768,
     "BQAAAIwAAAAAWh8drxgAAP//////////"
     "QG2FO1YAAACspk47XgAAALamTjtfAAAAwHUROwoAAACg3UY7LQAAAP///////////////////"
     "////////////"
     "y0AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA="},
    {3, "irhi", 2, 5, 1073741824,
     "BQAAAIwAAAAAWjQdgRkAAP//////////gHqIO1gAAAAkp047YAAAAC6nTjthAAAA/////////"
     "/+g3UY7LQAAAP///////////////////////////////"
     "y0AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA="},
    {4, "jjph", 0, 3, 16384,
     "BQAAAIwAAAAAWh4dpRgAAKBHXzs9AAAAoOaDO1UAAAA+pk47WgAAAEimTjtbAAAA/////////"
     "/+g3UY7LQAAAP///////////////////////////////"
     "y0AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA="},
    {5, "kecz", 0, 5, 268435456,
     "BQAAAIwAAAAAWjIdbRkAAEDOYDs+AAAA4POGO1cAAAA+"
     "pk47WgAAAEimTjtbAAAAXKZOO10AAACg3UY7LQAAAP///////////////////////////////"
     "y0AAAAAAAAAAAAAAAAAAAAtAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA="},
    {6, "kjrn", 1, 2, 256,
     "BQAAAIwAAAAAWhUdSxgAAP////////////////////+spk47XgAAAMB1ETsKAAAA/////////"
     "/+g3UY7LQAAAP///////////////////////////////"
     "y0AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA="},
    {7, "wma2", 0, 4, 2097152,
     "BQAAAIwAAAAAWigdCRkAAEDOYDs+AAAAQANtO0YAAAA+"
     "pk47WgAAAEimTjtbAAAAXKZOO10AAACg3UY7LQAAAP///////////////////////////////"
     "y0AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA="},
    {8, "it8w", 3, 5, 2147483648U,
     "BQAAAIwAAAAAWjUdixkAAD6mTjtaAAAAIAGKO1kAAACSp047YgAAAEimTjtbAAAArKZOO14AA"
     "ACg3UY7LQAAAP///////////////////////////////"
     "y0AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA="},
    {9, "zmyp", 2, 4, 8388608,
     "BQAAAIwAAAAAWiodHRkAAP////////////////////8kp047YAAAAP///////////////////"
     "/+g3UY7LQAAAP///////////////////////////////"
     "y0AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA="},
    {10, "3n7q", 0, 1, 1,
     "BQAAAIwAAAAAWgod3RcAAP////////////////////8+pk47WgAAAP///////////////////"
     "/+g3UY7LQAAAP///////////////////////////////"
     "y0AAAAAAAAAAAAAAAAAAAAtAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA="},
}};

// Captured search with nullable optional filters, sequence 0014.
inline QJsonObject OfficialNullableChannelSearchRequest() {
  static constexpr char Request[] = R"json({
  "FixedOrGeneralList": [{"FixedOrGeneral": 0},{"FixedOrGeneral": 2}],
  "FormDataVersion": 0,
  "GetCount": 100,
  "HolyGrailTypeId": null,
  "MessageId": "ChannelSearchRequest",
  "RitualLevel": null,
  "SessionId": "<captured-session-redacted>",
  "Status": 1,
  "SubFeatureFlag": null,
  "UnlockedFlagList": [{"UnlockedFlag": 260},{"UnlockedFlag": 278970752},{"UnlockedFlag": 3491807616}],
  "UserId": 2880
})json";
  return QJsonDocument::fromJson(QByteArray(Request)).object();
}

// Captured Quick Search request, sequence 0037.
inline QJsonObject OfficialChannelRandomJoinRequest() {
  static constexpr char Request[] = R"json({
  "CharaId": 9223372036854776000,
  "FormDataVersion": 0,
  "MessageId": "ChannelRandomJoinRequest",
  "RandomJoinTargetList": [
    {"HolyGrailTypeId": 0,"RitualLevel": 5},
    {"HolyGrailTypeId": 1,"RitualLevel": 3},
    {"HolyGrailTypeId": 2,"RitualLevel": 5},
    {"HolyGrailTypeId": 3,"RitualLevel": 5}
  ],
  "SessionId": "<captured-session-redacted>",
  "UnlockedFlagList": [{"UnlockedFlag": 260},{"UnlockedFlag": 278970752},{"UnlockedFlag": 3491807617}],
  "UserId": 2880
})json";
  return QJsonDocument::fromJson(QByteArray(Request)).object();
}

} // namespace BloodborneTestFixtures
