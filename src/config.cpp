// SPDX-FileCopyrightText: Copyright 2019-2026 rpcsn Project
// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include <algorithm>
#include <QDebug>
#include <QFile>
#include <QSettings>
#include <QTextStream>
#include <QVariant>
#include "config.h"

void ConfigManager::LoadBannedDomains() {
    m_bannedDomains.clear();
    QFile f("domains_banlist.txt");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    QTextStream in(&f);
    while (!in.atEnd()) {
        QString d = in.readLine().trimmed().toLower();
        if (!d.isEmpty() && !d.startsWith('#'))
            m_bannedDomains.insert(d);
    }
    qInfo() << "Loaded" << m_bannedDomains.size() << "banned domains";
}

void ConfigManager::Parse(const QString& path) {
    QWriteLocker lk(&m_lock);

    m_path = path;

    QSettings s(path, QSettings::IniFormat);

    auto value = [&](const QString& key, const QString& legacyKey,
                     const QVariant& def) -> QVariant {
        if (s.contains(key)) {
            return s.value(key, def);
        }
        if (!legacyKey.isEmpty() && s.contains(legacyKey)) {
            return s.value(legacyKey, def);
        }
        s.setValue(key, def);
        return def;
    };
    auto str = [&](const QString& key, const QString& legacyKey, const QString& def) -> QString {
        return value(key, legacyKey, def).toString();
    };
    auto boolean = [&](const QString& key, const QString& legacyKey, bool def) -> bool {
        return value(key, legacyKey, def).toBool();
    };
    auto strList = [&](const QString& key, const QString& legacyKey) -> QStringList {
        QString raw = value(key, legacyKey, QString()).toString();
        return raw.isEmpty() ? QStringList{} : raw.split(',', Qt::SkipEmptyParts);
    };

    m_host = str("Server/Host", "Host", "127.0.0.1");
    m_unsecured_port = str("Server/UnsecuredPort", "UnsecuredPort", "31313");
    m_matchingUdpPort = str("Network/MatchingUdpPort", "MatchingUdpPort", "31314");
    m_webapiPort = str("Network/WebApiPort", "WebApiPort", "31315");
    m_statsEnabled = boolean("Stats/Enabled", "StatsEnabled", true);
    m_matching2Enabled = boolean("Network/Matching2Enabled", "Matching2Enabled", true);
    m_bloodborneSeamlessCoop = boolean("Bloodborne/SeamlessCoop", "BloodborneSeamlessCoop", false);
    m_bloodborneSummonLocationMode =
        str("BloodborneSummon/LocationMode", "BloodborneSummonLocationMode", "Vanilla").trimmed();
    if (m_bloodborneSummonLocationMode.compare("Vanilla", Qt::CaseInsensitive) != 0 &&
        m_bloodborneSummonLocationMode.compare("SameRegion", Qt::CaseInsensitive) != 0 &&
        m_bloodborneSummonLocationMode.compare("SameArea", Qt::CaseInsensitive) != 0) {
        qWarning() << "Invalid BloodborneSummon/LocationMode" << m_bloodborneSummonLocationMode
                   << "; using Vanilla";
        m_bloodborneSummonLocationMode = QStringLiteral("Vanilla");
    }
    m_bloodborneSummonTraceEnabled =
        boolean("Debug/BloodborneSummonTrace", "BloodborneSummonTrace", false);
    m_bloodborneBootstrapEnabled =
        boolean("Bloodborne/BootstrapEnabled", "BloodborneBootstrapEnabled", false);
    m_bloodbornePublicBaseUrl =
        str("Bloodborne/PublicBaseUrl", "BloodbornePublicBaseUrl", "").trimmed();
    m_bloodborneReferenceProxyEnabled =
        boolean("Bloodborne/ReferenceProxyEnabled", "BloodborneReferenceProxyEnabled", false);
    m_bloodborneReferenceProxyUrl =
        str("Bloodborne/ReferenceProxyUrl", "BloodborneReferenceProxyUrl",
            "http://thehuntersdream.com:18671")
            .trimmed();
    m_bloodborneWelcomeNoticeEnabled =
        boolean("Website/WelcomeNoticeEnabled", "BloodborneWelcomeNoticeEnabled", false);
    m_bloodborneWelcomeNoticeTitle =
        str("Website/WelcomeNoticeTitle", "BloodborneWelcomeNoticeTitle", "The Hunter's Dream");
    m_bloodborneWelcomeNoticeBody = str("Website/WelcomeNoticeBody", "BloodborneWelcomeNoticeBody",
                                        "Welcome to the private Bloodborne server.");
    m_bloodborneWelcomeMessageEnabled =
        boolean("Website/WelcomeMessageEnabled", "BloodborneWelcomeMessageEnabled", false);
    m_bloodborneWelcomeMessage = str("Website/WelcomeMessage", "BloodborneWelcomeMessage", "");
    m_bloodborneGhostLifetimeSeconds =
        str("Bloodborne/GhostLifetimeSeconds", "BloodborneGhostLifetimeSeconds", "600").toInt();
    m_serverLogEnabled = boolean("Debug/ServerLogEnabled", "ServerLogEnabled", true);
    m_serverLogDirectory = str("Debug/ServerLogDirectory", "ServerLogDirectory", "logs").trimmed();
    m_serverLogKeepDays = str("Debug/ServerLogKeepDays", "ServerLogKeepDays", "30").toInt();
    m_serverLogFlushImmediately =
        boolean("Debug/ServerLogFlushImmediately", "ServerLogFlushImmediately", true);
    m_bloodborneWebsiteEnabled = boolean("Website/Enabled", "BloodborneWebsiteEnabled", true);
    m_bloodborneWebsitePort = str("Website/Port", "BloodborneWebsitePort", "31316").trimmed();
    m_bloodborneWebsiteRegistrationEnabled =
        boolean("Website/RegistrationEnabled", "BloodborneWebsiteRegistrationEnabled", true);
    m_bloodborneWebsiteExternalAssetsEnabled =
        boolean("Website/ExternalAssetsEnabled", "BloodborneWebsiteExternalAssetsEnabled", true);
    m_bloodborneWebsiteExternalAssetsPath =
        str("Website/ExternalAssetsPath", "BloodborneWebsiteExternalAssetsPath", "web").trimmed();
    m_bloodborneWebsiteChatEnabled =
        boolean("Website/ChatEnabled", "BloodborneWebsiteChatEnabled", true);
    m_bloodborneWebsiteChatMaxMessageLength = std::clamp(
        str("Website/ChatMaxMessageLength", "BloodborneWebsiteChatMaxMessageLength", "400").toInt(),
        1, 4000);
    m_bloodborneWebsiteChatHistoryLimit = std::clamp(
        str("Website/ChatHistoryLimit", "BloodborneWebsiteChatHistoryLimit", "100").toInt(), 1,
        500);
    m_bloodborneWebsiteChatResetHours = std::clamp(
        str("Website/ChatResetHours", "BloodborneWebsiteChatResetHours", "24").toInt(), 1, 8760);
    m_bloodborneWebsiteDownloadMaxFileSizeMiB = std::clamp(
        str("Downloads/MaxFileSizeMiB", "BloodborneWebsiteDownloadMaxFileSizeMiB", "512").toInt(),
        1, 2048);
    m_statsPort = str("Stats/Port", "StatsPort", "31320");
    m_statsPath = str("Stats/Path", "StatsPath", "stats");
    m_statsCacheLife = str("Stats/CacheLife", "StatsCacheLife", "30").toInt();
    m_emailValidated = boolean("Accounts/EmailValidated", "EmailValidated", false);
    m_adminsList = strList("Accounts/AdminsList", "AdminsList");
    m_registrationSecretKey = str("Accounts/RegistrationSecretKey", "RegistrationSecretKey", "");

    s.sync();

    if (s.status() != QSettings::NoError)
        qWarning() << "QSettings error reading" << path;
    else
        qInfo() << "Config loaded from" << path;

    if (!m_registrationSecretKey.isEmpty())
        qInfo() << "Registration requires secret key";
    else
        qInfo() << "Registration is open (no secret key set)";

    LoadBannedDomains();
}

bool ConfigManager::Load(const QString& path) {
    Parse(path);
    return true;
}

void ConfigManager::Reload(const QString& path) {
    Parse(path.isEmpty() ? m_path : path);
    qInfo() << "Config reloaded";
}
