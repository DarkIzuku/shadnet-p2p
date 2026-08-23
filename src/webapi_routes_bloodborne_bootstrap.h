// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QByteArray>
#include <QString>

#include "bloodborne_bootstrap.h"

class Database;
class QHttpServer;
struct SharedState;

namespace WebApiRoutes {

void RegisterBloodborneBootstrapRoutes(QHttpServer& http, Database& db, SharedState& shared,
                                       const QString& publicBaseUrl,
                                       const QByteArray& serverStatusInfo,
                                       bool referenceProxyEnabled = false,
                                       const Bloodborne::WelcomeNotice& welcomeNotice =
                                           Bloodborne::WelcomeNotice{});

} // namespace WebApiRoutes
