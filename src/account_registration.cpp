// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include "account_registration.h"

#include "client_session.h"
#include "config.h"
#include "database.h"

AccountRegistrationError RegisterShadNetAccount(const AccountRegistrationRequest& request,
                                                const ConfigManager& config, Database& database) {
    if (!ClientSession::IsValidNpid(request.username))
        return AccountRegistrationError::InvalidUsername;
    if (request.password.isEmpty())
        return AccountRegistrationError::InvalidPassword;
    if (request.email.isEmpty())
        return AccountRegistrationError::InvalidEmail;

    const int at = request.email.indexOf('@');
    if (at >= 0 && config.IsBannedDomain(request.email.mid(at + 1).toLower()))
        return AccountRegistrationError::BannedEmailProvider;

    QString avatarUrl = request.avatarUrl;
    if (avatarUrl.isEmpty())
        avatarUrl = QStringLiteral("https://shadps4.net/shadnet/avatars/default_01.png");

    const auto error =
        database.CreateAccount(request.username, request.password, avatarUrl, request.email);
    if (!error)
        return AccountRegistrationError::None;

    switch (*error) {
    case DbError::ExistingUsername:
        return AccountRegistrationError::ExistingUsername;
    case DbError::ExistingEmail:
        return AccountRegistrationError::ExistingEmail;
    default:
        return AccountRegistrationError::DatabaseError;
    }
}
