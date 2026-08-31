// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <iostream>

#include <QSettings>
#include <QTemporaryDir>

#include "config.h"

namespace {

bool Check(bool condition, const char *expression, int line) {
  if (!condition) {
    std::cerr << "check failed at line " << line << ": " << expression << '\n';
  }
  return condition;
}

#define CHECK(expression)                                                      \
  do {                                                                         \
    if (!Check((expression), #expression, __LINE__))                           \
      return 1;                                                                \
  } while (false)

} // namespace

int main() {
  QTemporaryDir temporary;
  CHECK(temporary.isValid());

  const QString legacyPath = temporary.filePath(QStringLiteral("legacy.cfg"));
  {
    QSettings settings(legacyPath, QSettings::IniFormat);
    settings.setValue(QStringLiteral("Host"), QStringLiteral("0.0.0.0"));
    settings.setValue(QStringLiteral("Matching2Enabled"), false);
    settings.setValue(QStringLiteral("BloodborneSeamlessCoop"), true);
    settings.setValue(QStringLiteral("BloodborneSummonLocationMode"),
                      QStringLiteral("SameRegion"));
    settings.setValue(QStringLiteral("BloodborneFeedTrace"), true);
    settings.setValue(QStringLiteral("BloodborneWebsitePort"),
                      QStringLiteral("32000"));
    settings.setValue(QStringLiteral("RegistrationSecretKey"),
                      QStringLiteral("legacy-key"));
    settings.sync();
  }
  ConfigManager legacy;
  CHECK(legacy.Load(legacyPath));
  CHECK(legacy.GetHost() == QStringLiteral("0.0.0.0"));
  CHECK(!legacy.IsMatching2Enabled());
  CHECK(legacy.IsBloodborneSeamlessCoopEnabled());
  CHECK(legacy.GetBloodborneSummonLocationMode() ==
        QStringLiteral("SameRegion"));
  CHECK(legacy.IsBloodborneFeedTraceEnabled());
  CHECK(legacy.GetBloodborneWebsitePort() == QStringLiteral("32000"));
  CHECK(legacy.IsRegistrationAllowed(QStringLiteral("legacy-key")));
  CHECK(!legacy.IsRegistrationAllowed(QStringLiteral("wrong")));

  const QString groupedPath = temporary.filePath(QStringLiteral("grouped.cfg"));
  {
    QSettings settings(groupedPath, QSettings::IniFormat);
    settings.setValue(QStringLiteral("Server/Host"),
                      QStringLiteral("127.1.2.3"));
    settings.setValue(QStringLiteral("Host"), QStringLiteral("legacy-loses"));
    settings.setValue(QStringLiteral("Network/Matching2Enabled"), true);
    settings.setValue(QStringLiteral("BloodborneSummon/LocationMode"),
                      QStringLiteral("SameArea"));
    settings.setValue(QStringLiteral("Debug/BloodborneSummonTrace"), true);
    settings.setValue(QStringLiteral("Debug/BloodborneFeedTrace"), true);
    settings.setValue(QStringLiteral("Accounts/AdminsList"),
                      QStringLiteral("Izuku,Test"));
    settings.sync();
  }
  ConfigManager grouped;
  CHECK(grouped.Load(groupedPath));
  CHECK(grouped.GetHost() == QStringLiteral("127.1.2.3"));
  CHECK(grouped.IsMatching2Enabled());
  CHECK(grouped.GetBloodborneSummonLocationMode() ==
        QStringLiteral("SameArea"));
  CHECK(grouped.IsBloodborneSummonTraceEnabled());
  CHECK(grouped.IsBloodborneFeedTraceEnabled());
  CHECK(grouped.IsAdmin(QStringLiteral("Izuku")));

  const QString generatedPath =
      temporary.filePath(QStringLiteral("generated.cfg"));
  ConfigManager generated;
  CHECK(generated.Load(generatedPath));
  QSettings generatedSettings(generatedPath, QSettings::IniFormat);
  CHECK(generatedSettings.contains(QStringLiteral("Server/Host")));
  CHECK(generatedSettings.contains(QStringLiteral("Network/Matching2Enabled")));
  CHECK(generatedSettings.contains(
      QStringLiteral("BloodborneSummon/LocationMode")));
  CHECK(generatedSettings.value(QStringLiteral("BloodborneSummon/LocationMode"))
            .toString() == QStringLiteral("Vanilla"));
  CHECK(!generatedSettings.contains(QStringLiteral("BloodborneSeamlessCoop")));
  CHECK(
      generatedSettings.contains(QStringLiteral("Debug/BloodborneFeedTrace")));
  CHECK(!generatedSettings.value(QStringLiteral("Debug/BloodborneFeedTrace"))
             .toBool());

  std::cout << "Config compatibility test passed\n";
  return 0;
}
