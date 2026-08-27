// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>

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

} // namespace BloodborneTestFixtures
