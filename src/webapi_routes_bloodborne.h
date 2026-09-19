// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

class QHttpServer;
class Database;
class QString;
struct SharedState;

namespace WebApiRoutes {

void RegisterBloodborneRoutes(QHttpServer& http, bool seamlessCoop, const QString& locationMode,
                              bool summonTrace, SharedState& shared,
                              Database* websiteMetricsDatabase = nullptr);

} // namespace WebApiRoutes
