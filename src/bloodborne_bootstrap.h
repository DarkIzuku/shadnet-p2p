// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <array>

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QString>

namespace Bloodborne {

inline constexpr char ServerStatusInfoContentType[] = "text/plain; charset=utf-8";

struct BootstrapApi {
    const char* name;
    const char* path;
};

const std::array<BootstrapApi, 37>& BootstrapApis();
QByteArray BuildServerStatusInfo(const QString& publicBaseUrl, QByteArray* decodedXml = nullptr,
                                 QString* error = nullptr);

QJsonObject BuildLoginResponse(qint64 userId, int languageId, const QString& sessionId);
QJsonObject BuildServerTimeResponse();
QJsonObject BuildNoticeNormalResponse();
QJsonObject BuildNoticeEmergencyResponse(const QString& checkTime);
QJsonObject BuildSyncCharaIdResponse(const QList<qint64>& charaIds);

} // namespace Bloodborne
