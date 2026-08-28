<!--
SPDX-FileCopyrightText: Copyright 2026 shadNet Project
SPDX-License-Identifier: GPL-2.0-or-later
-->

# Bloodborne Chalice Dungeons

shadNet implements the locally captured vanilla Bloodborne contracts for:

- `POST /channel/upload`
- `POST /channel/share`
- `POST /channel/search`
- `POST /channel/word_search`
- `POST /channel/get_info`
- `POST /channel/get_details_info`
- `POST /channel/random_join`
- `POST /penalty/notify_user_properties_move_count`
- `POST /penalty/check_user_priority_move_count`

The game endpoints and The Hunter's Requiem read the same `bloodborne_chalice` SQLite
table. `FormData` is preserved exactly as uploaded. Character IDs are stored as decimal text
because the vanilla JSON number observed in the capture exceeds `INT64_MAX`.

Rows are classified by `origin`:

- `vanilla_fixed`: ten captured `FixedOrGeneral=2` ritual records, bootstrapped locally with their
  original IDs, glyphs, dates, flags and byte-exact `FormData`;
- `community`: player records created by `ChannelUpload` and controlled by `ChannelShare`.

Migration 8 is non-destructive and idempotent. If an older community database already used one of
the captured IDs `1` through `10`, that community row is moved to the next free ID before the fixed
record is inserted. The SQLite autoincrement sequence is then advanced past every stored row.

Newly generated glyphs are eight lower-case Bloodborne-compatible characters. SQLite enforces a
unique glyph index and generation retries a collision instead of deriving the glyph from the
internal `ChannelId`.

Only `ShareLevel=2` (Open) community Chalices are visible to Quick Search and the public website
API. Fixed ritual rows remain available to compatible game `ChannelSearch`, glyph search and direct
details/info calls, but are hidden from The Hunter's Requiem community list. The captured reference
also hid the creator's own `ShareLevel=0` glyph from `ChannelWordSearch`.

The captured fixed combinations are Type 0 / Depths 1-5, Type 1 / Depths 2-3, Type 2 / Depths 4-5,
and Type 3 / Depth 5. The Type 0 / Depth 1 request returns captured `ChannelId=10`, glyph `3n7q` and
its 140-byte decoded `FormData` without contacting Hunter's Dream at runtime.

## Captured-contract boundary

`UnlockedFlagList` and `WishMaterialList` are validated and preserved, but shadNet does not invent
an undocumented interpretation for their inner values. Likewise, `FormData` is not decoded into
bosses, enemies, rites or a room layout. The website reserves structured map data for a future
SVG/Canvas renderer and currently reports `not_decoded`.

The following APIs are deliberately **not declared supported** because no complete real request
and response contract has been captured:

- `/channel/add_material`
- `/channel/add_material_complete_notify` (advertised by vanilla configuration as
  `/channel/notify_add_material_complete`)

Calls to those paths return HTTP 501 and emit `[BLOODBORNE CHALICE PENDING]` without logging a
session token or payload data.
