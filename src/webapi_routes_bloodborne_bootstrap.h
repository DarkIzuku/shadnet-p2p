// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QString>

class Database;
class QHttpServer;
struct SharedState;

namespace WebApiRoutes {

void RegisterBloodborneBootstrapRoutes(QHttpServer& http, Database& db, SharedState& shared,
                                       const QString& publicBaseUrl);

} // namespace WebApiRoutes
