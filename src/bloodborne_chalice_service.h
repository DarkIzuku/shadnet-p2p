// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QJsonObject>

#include "bloodborne_online_service.h"

class Database;

namespace Bloodborne {

class ChaliceService {
public:
    explicit ChaliceService(Database& db);

    OnlineResult Upload(qint64 userId, const QJsonObject& request);
    OnlineResult Share(qint64 userId, const QJsonObject& request);
    OnlineResult Search(qint64 userId, const QJsonObject& request);
    OnlineResult WordSearch(qint64 userId, const QJsonObject& request);
    OnlineResult GetInfo(qint64 userId, const QJsonObject& request);
    OnlineResult GetDetailsInfo(qint64 userId, const QJsonObject& request);
    OnlineResult RandomJoin(qint64 userId, const QJsonObject& request);

private:
    Database& m_db;
};

} // namespace Bloodborne
