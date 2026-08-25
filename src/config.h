// SPDX-FileCopyrightText: Copyright 2019-2026 rpcsn Project
// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QReadLocker>
#include <QReadWriteLock>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QWriteLocker>

class ConfigManager {
public:
    bool Load(const QString& path = "shadnet.cfg");
    void Reload(const QString& path = "shadnet.cfg");

    QString GetHost() const {
        QReadLocker lk(&m_lock);
        return m_host;
    }
    QString GetUnsecuredPort() const {
        QReadLocker lk(&m_lock);
        return m_unsecured_port;
    }
    QString GetMatchingUdpPort() const {
        QReadLocker lk(&m_lock);
        return m_matchingUdpPort;
    }
    QString GetWebApiPort() const {
        QReadLocker lk(&m_lock);
        return m_webapiPort;
    }
    bool IsStatsEnabled() const {
        QReadLocker lk(&m_lock);
        return m_statsEnabled;
    }
    QString GetStatsPort() const {
        QReadLocker lk(&m_lock);
        return m_statsPort;
    }
    QString GetStatsPath() const {
        QReadLocker lk(&m_lock);
        return m_statsPath;
    }
    int GetStatsCacheLife() const {
        QReadLocker lk(&m_lock);
        return m_statsCacheLife;
    }
    bool IsMatching2Enabled() const {
        QReadLocker lk(&m_lock);
        return m_matching2Enabled;
    }
    bool IsBloodborneSeamlessCoopEnabled() const {
        QReadLocker lk(&m_lock);
        return m_bloodborneSeamlessCoop;
    }
    bool IsBloodborneBootstrapEnabled() const {
        QReadLocker lk(&m_lock);
        return m_bloodborneBootstrapEnabled;
    }
    QString GetBloodbornePublicBaseUrl() const {
        QReadLocker lk(&m_lock);
        return m_bloodbornePublicBaseUrl;
    }
    bool IsBloodborneReferenceProxyEnabled() const {
        QReadLocker lk(&m_lock);
        return m_bloodborneReferenceProxyEnabled;
    }
    QString GetBloodborneReferenceProxyUrl() const {
        QReadLocker lk(&m_lock);
        return m_bloodborneReferenceProxyUrl;
    }
    bool IsBloodborneWelcomeNoticeEnabled() const {
        QReadLocker lk(&m_lock);
        return m_bloodborneWelcomeNoticeEnabled;
    }
    QString GetBloodborneWelcomeNoticeTitle() const {
        QReadLocker lk(&m_lock);
        return m_bloodborneWelcomeNoticeTitle;
    }
    QString GetBloodborneWelcomeNoticeBody() const {
        QReadLocker lk(&m_lock);
        return m_bloodborneWelcomeNoticeBody;
    }
    bool IsBloodborneWelcomeMessageEnabled() const {
        QReadLocker lk(&m_lock);
        return m_bloodborneWelcomeMessageEnabled;
    }
    QString GetBloodborneWelcomeMessage() const {
        QReadLocker lk(&m_lock);
        return m_bloodborneWelcomeMessage;
    }
    int GetBloodborneGhostLifetimeSeconds() const {
        QReadLocker lk(&m_lock);
        return m_bloodborneGhostLifetimeSeconds;
    }
    bool IsServerLogEnabled() const {
        QReadLocker lk(&m_lock);
        return m_serverLogEnabled;
    }
    QString GetServerLogDirectory() const {
        QReadLocker lk(&m_lock);
        return m_serverLogDirectory;
    }
    int GetServerLogKeepDays() const {
        QReadLocker lk(&m_lock);
        return m_serverLogKeepDays;
    }
    bool IsServerLogFlushImmediately() const {
        QReadLocker lk(&m_lock);
        return m_serverLogFlushImmediately;
    }
    bool IsBloodborneWebsiteEnabled() const {
        QReadLocker lk(&m_lock);
        return m_bloodborneWebsiteEnabled;
    }
    QString GetBloodborneWebsitePort() const {
        QReadLocker lk(&m_lock);
        return m_bloodborneWebsitePort;
    }
    bool IsBloodborneWebsiteRegistrationEnabled() const {
        QReadLocker lk(&m_lock);
        return m_bloodborneWebsiteRegistrationEnabled;
    }

    bool IsEmailValidated() const {
        QReadLocker lk(&m_lock);
        return m_emailValidated;
    }
    bool IsBannedDomain(const QString& d) const {
        QReadLocker lk(&m_lock);
        return m_bannedDomains.contains(d.toLower());
    }
    bool IsAdmin(const QString& npid) const {
        QReadLocker lk(&m_lock);
        return m_adminsList.contains(npid);
    }

    // Returns true if registration is allowed for the given secret_key.
    // When RegistrationSecretKey is empty, all registrations are allowed.
    // When set, only requests carrying the matching key are accepted.
    bool IsRegistrationAllowed(const QString& key) const {
        QReadLocker lk(&m_lock);
        return m_registrationSecretKey.isEmpty() ||
               (!key.isEmpty() && key == m_registrationSecretKey);
    }

    void SetHost(const QString& v) {
        QWriteLocker lk(&m_lock);
        m_host = v;
    }
    void SetUnsecuredPort(const QString& v) {
        QWriteLocker lk(&m_lock);
        m_unsecured_port = v;
    }
    void SetEmailValidated(bool v) {
        QWriteLocker lk(&m_lock);
        m_emailValidated = v;
    }
    void SetAdminsList(const QStringList& v) {
        QWriteLocker lk(&m_lock);
        m_adminsList = v;
    }
    void LoadBannedDomains();

private:
    void Parse(const QString& path);

    mutable QReadWriteLock m_lock;
    QString m_path;

    // config values
    QString m_host = "0.0.0.0";
    QString m_unsecured_port = "31313";
    QString m_matchingUdpPort = "31314";
    QString m_webapiPort = "31315";
    bool m_statsEnabled = true;
    bool m_matching2Enabled = true;
    bool m_bloodborneSeamlessCoop = false;
    bool m_bloodborneBootstrapEnabled = false;
    QString m_bloodbornePublicBaseUrl;
    bool m_bloodborneReferenceProxyEnabled = false;
    QString m_bloodborneReferenceProxyUrl = "http://thehuntersdream.com:18671";
    bool m_bloodborneWelcomeNoticeEnabled = false;
    QString m_bloodborneWelcomeNoticeTitle = "The Hunter's Dream";
    QString m_bloodborneWelcomeNoticeBody = "Welcome to the private Bloodborne server.";
    bool m_bloodborneWelcomeMessageEnabled = false;
    QString m_bloodborneWelcomeMessage;
    int m_bloodborneGhostLifetimeSeconds = 600;
    bool m_serverLogEnabled = true;
    QString m_serverLogDirectory = "logs";
    int m_serverLogKeepDays = 30;
    bool m_serverLogFlushImmediately = true;
    bool m_bloodborneWebsiteEnabled = true;
    QString m_bloodborneWebsitePort = "31316";
    bool m_bloodborneWebsiteRegistrationEnabled = true;
    QString m_statsPort = "31320";
    QString m_statsPath = "stats";
    int m_statsCacheLife = 30; // seconds the stats JSON is cached before recompute
    bool m_emailValidated = false;
    QStringList m_adminsList;
    QSet<QString> m_bannedDomains;
    // When non-empty, registrations must supply this key or they are rejected.
    QString m_registrationSecretKey;
};
