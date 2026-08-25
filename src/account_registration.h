// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QString>

class ConfigManager;
class Database;

enum class AccountRegistrationError {
    None,
    InvalidUsername,
    InvalidPassword,
    InvalidEmail,
    BannedEmailProvider,
    ExistingUsername,
    ExistingEmail,
    DatabaseError,
};

struct AccountRegistrationRequest {
    QString username;
    QString password;
    QString email;
    QString avatarUrl;
};

// Shared account creation path used by both the binary shadNet registration
// command and the optional community website. Transport authorization remains
// the caller's responsibility (secret key for TCP, website setting for HTTP).
AccountRegistrationError RegisterShadNetAccount(const AccountRegistrationRequest& request,
                                                const ConfigManager& config, Database& database);
