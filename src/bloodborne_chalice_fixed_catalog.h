// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <array>

#include <QtTypes>

namespace Bloodborne {

// Captured from 20260828-082944-864Z, ChannelSearch sequence 0014. Sequence 0069
// independently confirms the Type 0 / Depth 1 record byte for byte. These are the
// reference backend's ten FixedOrGeneral=2 vanilla records; none of the fields below
// are generated or inferred at runtime.
struct CapturedFixedChalice {
    qint64 channelId;
    const char* glyph;
    const char* date;
    const char* formData;
    int holyGrailTypeId;
    int ritualLevel;
    quint32 unlockFlag;
};

inline constexpr std::array<CapturedFixedChalice, 10> CapturedFixedChalices = {{
    {1, "3itx", "2026-01-27T00:21:00",
     "BQAAAIwAAAAAWhQdQRgAAP////////////////////8+pk47WgAAAEimTjtbAAAA//////////+g3UY7LQAAAP///////"
     "////////////////////////"
     "y0AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=",
     0, 2, 128},
    {2, "4j56", "2026-01-27T00:38:20",
     "BQAAAIwAAAAAWh8drxgAAP//////////QG2FO1YAAACspk47XgAAALamTjtfAAAAwHUROwoAAACg3UY7LQAAAP///////"
     "////////////////////////"
     "y0AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=",
     1, 3, 32768},
    {3, "irhi", "2026-01-27T00:48:57",
     "BQAAAIwAAAAAWjQdgRkAAP//////////gHqIO1gAAAAkp047YAAAAC6nTjthAAAA//////////+g3UY7LQAAAP///////"
     "////////////////////////"
     "y0AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=",
     2, 5, 1073741824},
    {4, "jjph", "2026-01-27T00:25:45",
     "BQAAAIwAAAAAWh4dpRgAAKBHXzs9AAAAoOaDO1UAAAA+pk47WgAAAEimTjtbAAAA//////////+g3UY7LQAAAP///////"
     "////////////////////////"
     "y0AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=",
     0, 3, 16384},
    {5, "kecz", "2026-01-26T00:23:00",
     "BQAAAIwAAAAAWjIdbRkAAEDOYDs+AAAA4POGO1cAAAA+pk47WgAAAEimTjtbAAAAXKZOO10AAACg3UY7LQAAAP///////"
     "////////////////////////"
     "y0AAAAAAAAAAAAAAAAAAAAtAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=",
     0, 5, 268435456},
    {6, "kjrn", "2026-01-27T00:34:20",
     "BQAAAIwAAAAAWhUdSxgAAP////////////////////+spk47XgAAAMB1ETsKAAAA//////////+g3UY7LQAAAP///////"
     "////////////////////////"
     "y0AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=",
     1, 2, 256},
    {7, "wma2", "2026-01-27T00:29:54",
     "BQAAAIwAAAAAWigdCRkAAEDOYDs+AAAAQANtO0YAAAA+pk47WgAAAEimTjtbAAAAXKZOO10AAACg3UY7LQAAAP///////"
     "////////////////////////"
     "y0AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=",
     0, 4, 2097152},
    {8, "it8w", "2026-01-27T00:51:22",
     "BQAAAIwAAAAAWjUdixkAAD6mTjtaAAAAIAGKO1kAAACSp047YgAAAEimTjtbAAAArKZOO14AAACg3UY7LQAAAP///////"
     "////////////////////////"
     "y0AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=",
     3, 5, 2147483648U},
    {9, "zmyp", "2026-01-27T00:40:21",
     "BQAAAIwAAAAAWiodHRkAAP////////////////////8kp047YAAAAP////////////////////+g3UY7LQAAAP///////"
     "////////////////////////"
     "y0AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=",
     2, 4, 8388608},
    {10, "3n7q", "2026-01-27T03:05:01",
     "BQAAAIwAAAAAWgod3RcAAP////////////////////8+pk47WgAAAP////////////////////+g3UY7LQAAAP///////"
     "////////////////////////"
     "y0AAAAAAAAAAAAAAAAAAAAtAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=",
     0, 1, 1},
}};

} // namespace Bloodborne
