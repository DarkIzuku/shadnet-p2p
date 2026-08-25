// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QJsonObject>
#include <QString>

class Database;

namespace Bloodborne {

enum class OnlineError {
    None,
    InvalidRequest,
    NotFound,
    Forbidden,
    Database,
};

struct OnlineResult {
    OnlineError error = OnlineError::None;
    QJsonObject response;
    QString detail;

    bool IsSuccess() const {
        return error == OnlineError::None;
    }
};

class OnlineService {
public:
    explicit OnlineService(Database& db, int ghostLifetimeSeconds = 600,
                           bool websiteMetricsEnabled = false);

    OnlineResult UploadMessengerShell(qint64 userId, const QJsonObject& request);

    OnlineResult CreateBloodMessages(qint64 userId, const QJsonObject& request);
    OnlineResult GetBloodMessages(qint64 userId, const QJsonObject& request);
    OnlineResult EvaluateBloodMessage(qint64 userId, const QJsonObject& request);
    OnlineResult GetBloodEvaluations(qint64 userId, const QJsonObject& request);
    OnlineResult SearchBloodMessages(qint64 userId, const QJsonObject& request);
    OnlineResult DeleteBloodMessage(qint64 userId, const QJsonObject& request);

    OnlineResult CreateTombMessage(qint64 userId, const QJsonObject& request);
    OnlineResult GetTombMessages(qint64 userId, const QJsonObject& request);
    OnlineResult GetDeathVision(qint64 userId, const QJsonObject& request);

    OnlineResult CreateWanderingGhost(qint64 userId, const QJsonObject& request);
    OnlineResult GetWanderingGhosts(qint64 userId, const QJsonObject& request);

private:
    Database& m_db;
    int m_ghostLifetimeSeconds;
    bool m_websiteMetricsEnabled;
};

} // namespace Bloodborne
