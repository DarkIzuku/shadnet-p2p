<!-- SPDX-FileCopyrightText: Copyright 2026 shadNet Project -->
<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# The Hunter's Requiem community website

The Hunter's Requiem is an optional website served by the same `shadnet` process. It has its own
Qt HTTP listener and never registers routes on the Bloodborne WebAPI port. No Node, Python, web
server, or Internet-hosted asset is required.

## Configuration

Add these values beneath `[General]` in the `shadnet.cfg` beside the executable:

```ini
BloodborneWebsiteEnabled=true
BloodborneWebsitePort=31316
BloodborneWebsiteRegistrationEnabled=true
BloodborneWebsiteExternalAssetsEnabled=true
BloodborneWebsiteExternalAssetsPath=web
BloodborneWebsiteChatEnabled=true
BloodborneWebsiteChatMaxMessageLength=400
BloodborneWebsiteChatHistoryLimit=100
BloodborneWebsiteChatResetHours=24
```

Open `http://SERVER_ADDRESS:31316/`. `BloodborneWebsiteEnabled=false` leaves the listener closed
and does not create the website data directory. Registration can be closed independently while
profiles and live status remain readable.

The game WebAPI remains on `31315`. Do not assign both listeners the same port. The website works
over a LAN, Radmin, or Tailscale address. Plain HTTP is supported for private networks; an optional
HTTPS reverse proxy can be placed in front later. Web cookies automatically gain `Secure` when the
request arrives with `X-Forwarded-Proto: https`.

## Editable frontend assets

Release artifacts include an editable `web/` folder beside the executable. With the default
settings above, a valid `web/index.html` makes the website serve that folder instead of its
embedded Qt resources. The path is resolved relative to the executable directory, not the shell's
current working directory; an absolute path is also accepted. Edit `index.html`, `assets/site.css`,
`assets/site.js`, or another file below `web/`, save it, and refresh the browser. External files are
read on every request with `Cache-Control: no-cache`, so recompilation and a server restart are not
required.

If the folder or `index.html` is absent, the embedded website starts normally. Set
`BloodborneWebsiteExternalAssetsEnabled=false` to force that fallback even when the folder exists.
The server rejects decoded `..` path components, directory requests, missing files, and symlinks
whose canonical destination is outside the configured frontend root. It never lists directories.

Expected startup messages are:

```text
Bloodborne website assets: external path=D:/path/to/shadnet/web
```

or:

```text
Bloodborne website assets: embedded Qt resources
```

## Accounts and registration

Website registration uses the same `RegisterShadNetAccount` service as the binary TCP `Create`
command: the same NPID rules, banned-domain rule, password hashing, salt, token, account tables,
and default avatar apply. The TCP command still requires its configured `RegistrationSecretKey`.
The website does not send that private key to a browser; its separate authorization switch is
`BloodborneWebsiteRegistrationEnabled`.

The MVP form intentionally asks only for username and password. It assigns a unique internal
address beneath the reserved `.invalid` domain because shadNet's account schema requires an email
value. It is not mailed or exposed. The resulting account is a normal shadNet account and the same
username/password can be used by shadPS4 or BB Launcher.

Website sessions are independent of Bloodborne `SessionId`. A random 256-bit token is sent only in
an `HttpOnly; SameSite=Lax` cookie; SQLite stores its SHA-256 hash. State-changing authenticated
requests also require a session-derived CSRF value. Passwords, session tokens, cookies, game
authorization codes, addresses, and signaling information are never returned by public APIs.

## Pages and API

The embedded bilingual ES/EN site includes:

- `/`, `/players`, and `/player/<username>` for public community status;
- `/register` and `/login` for the shared shadNet account system;
- `/account` for the authenticated profile and avatar upload;
- `/communion` for the global Hunter's Communion chat;
- `/api/status`, `/api/players`, `/api/players/<username>`, and `/api/activity`;
- `/api/register`, `/api/login`, `/api/logout`, `/api/account`, and
  `/api/account/avatar`.

## Hunter's Communion

The bilingual **Hunter's Communion / Comunión de Cazadores** is a website-only global chat. Public
visitors may read it, while posting requires an existing shadNet account, a valid independent web
session, same-origin validation, and the session-derived CSRF header. The backend ignores any
client-supplied author, account, or avatar fields and resolves all three from the authenticated
account and profile.

The website API is:

- `GET /api/chat/messages` — returns the newest configured history window in chronological order;
- `GET /api/chat/messages?after=<id>` — returns only rows with a larger ID for two-second
  incremental polling;
- `POST /api/chat/messages` — accepts JSON `{ "message": "..." }` from an authenticated session.

Messages are UTF-8 text, not HTML. The browser inserts them using `textContent`, preserves line
breaks, and renders their UTC server timestamp in the visitor's local time zone. The default
length limit is 400 Unicode code points. Posting is limited per authenticated account to one
accepted message per second and no more than five accepted messages in any ten-second window.

Migration 6 adds `bloodborne_web_chat_message`, which relates every message to the real `account`
row, and `bloodborne_web_chat_state`, which persists `last_chat_reset`. When chat is enabled, the
server checks the reset before opening the website listener and then every minute without relying
on a browser request. Once `BloodborneWebsiteChatResetHours` has elapsed, one transaction deletes
only chat rows and advances the reset time. Accounts, profiles, avatars, web/game sessions,
activity, statistics, Blood Messenger, Tomb Messenger, Death Vision, Wandering Ghost, summons,
and multiplayer state are untouched.

Set `BloodborneWebsiteChatEnabled=false` to hide the navigation entry and make both chat endpoints
return a `chat_disabled` response without writing. `BloodborneWebsiteChatHistoryLimit` controls
the initial and incremental response cap; `BloodborneWebsiteChatMaxMessageLength` and
`BloodborneWebsiteChatResetHours` control validation and retention.

The interface language preference is stored only in browser `localStorage`. Usernames and all
player-created data are inserted with DOM `textContent` and are never translated or interpreted as
HTML.

Avatars are limited to 2 MB. PNG, JPEG, and WebP input is decoded, bounded, scaled to at most
512×512, stripped of source metadata, and re-encoded as PNG. Server-generated UUID filenames are
stored beneath `data/bloodborne-website/avatars/`; browser filenames and absolute paths are never
used.

## Metrics and exact definitions

The website never derives online status from log files:

- **Hunters online** is the number of currently authenticated TCP clients that did not request
  Appear Offline.
- **Registered hunters** is the exact number of rows in the existing `account` table.
- **Co-op sessions** currently means active Matching2 rooms containing at least two members. It is
  not labeled as historical successful co-op.
- **Summons advertised** counts accepted `/summon_messenger/create` calls for that account.
- **Summon claims** counts first transitions to the broker's `Claimed` state; repeated or failed
  claims do not increment it. Neither counter is mislabeled as a completed co-op session.
- **Messages** on the home page is the current number of persisted Blood Messenger records.
- Profile message, bloodstain, and wandering-ghost counters are seeded from existing world-data
  tables during migration and incremented only after a successful local create transaction.
- Total sessions and online time are recorded from authenticated TCP connection/disconnection
  events while the website feature is enabled. Sessions left open by a crash are marked
  interrupted on the next startup; their uncertain crash-to-restart interval is not invented.

Historical successful co-op sessions remain intentionally unavailable because current server
events do not prove that both games entered multiplayer. Active Matching2 room membership is the
only multiplayer value currently shown.

Recent activity stores sanitized event type, internal account relation, and time for connections,
disconnections, messages, bloodstains, ghosts, accepted summon advertisements, and first successful
summon claims. Public output joins only the username. It does
not store or expose IP, ports, cookies, credentials, `SessionId`, or tokens.

## SQLite migration

Migration 5 adds these non-destructive tables alongside the existing schema:

- `bloodborne_player_session`
- `bloodborne_player_stats`
- `bloodborne_web_session`
- `bloodborne_web_profile`
- `bloodborne_activity`

Migration 6 adds only:

- `bloodborne_web_chat_message`
- `bloodborne_web_chat_state`

Existing account, Blood Messenger, Tomb/Death Vision, Wandering Ghost, SummonBroker, Matching2,
STUN, and signaling tables and contracts are not replaced.

## Manual check

1. Start `shadnet` and find `Bloodborne website listening on` in the session log.
2. Open `http://SERVER_ADDRESS:31316/` from a browser on the private network.
3. Create an account, sign in, upload an avatar, and confirm it appears on `/players`.
4. Log into shadNet from a client with that account. Its profile becomes online without exposing
   the client's address.
5. Stop the game normally and confirm Last seen, session count, and online time update.

Disable development reference proxy mode as usual. The community website never contacts The
Hunter's Dream or any other upstream service.
