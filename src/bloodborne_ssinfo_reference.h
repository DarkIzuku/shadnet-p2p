// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QByteArray>
#include <QString>

namespace Bloodborne {

inline constexpr int ReferenceServerStatusInfoSize = 15172;
inline constexpr int ReferenceDecodedServerStatusInfoSize = 11378;
inline constexpr char ReferenceServerStatusInfoSha256[] =
    "68084B4748720C23006975C0B22B822FF3680366F5C91FECF79528A42859E852";
inline constexpr char ReferenceDecodedServerStatusInfoSha256[] =
    "95F14F18D175034D9FB33B72B9F7029EC835133A968B877BB8AB4F3D122F1E70";

const QByteArray& ReferenceServerStatusInfo();
bool ValidateReferenceServerStatusInfo(QString* error, QByteArray* decodedXml = nullptr);

} // namespace Bloodborne
