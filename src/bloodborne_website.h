// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <memory>

#include <QObject>
#include <QString>

class ConfigManager;
struct SharedState;

class BloodborneWebsiteServer : public QObject {
    Q_OBJECT
public:
    explicit BloodborneWebsiteServer(QObject* parent = nullptr);
    ~BloodborneWebsiteServer() override;

    bool Start(ConfigManager* config, const QString& dbPath, SharedState* shared);
    void Stop();
    bool IsListening() const;
    quint16 ListeningPort() const;

public slots:
    void OnPlayerAuthenticated(qint64 userId, const QString& username, bool publicVisible);
    void OnPlayerDisconnected(qint64 userId, const QString& username, bool publicVisible);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};
