# Bloodborne Makeshift Altar and activity feed

## Makeshift Altar phase 1

The summon broker recognizes the captured Quick Search pair only when all of
the following are true:

- the host's search request is inside a Chalice, uses `SummonMethod=0`, and
  carries a positive, real `ChannelId`;
- the candidate advertisement uses `SummonMethod=1` in Hunter's Dream
  (`AreaId=352321536`, `AreaRegionId=210000`) with `ChannelId=0`.

For that pair only, discovery does not compare `AreaId`, `AreaRegionId`,
`ChannelId`, position, or distance. User/session isolation, data version,
summon type, password, level, candidate state, lifetime, and result limit
remain active. The returned record keeps the candidate's identity and opaque
summon payload, but its top-level location fields use the host request. This
preserves the real destination Chalice `ChannelId`; the candidate's zero is
never treated as a global wildcard and different Root host requests keep their
own channel.

This is deliberately a discovery fix. It does not synthesize a teleport,
warp, room join, host placement, or dungeon load. A client test must confirm
whether Bloodborne naturally proceeds from the discovered candidate through
room, claim, signaling, and dungeon loading.

## Activity feed compatibility

`POST /v1/users/{npid}/feed` accepts an authenticated JSON request owned by
the bearer account. It returns HTTP 200 with an empty body because captures
show the client checks the HTTP status but do not establish a response schema.
No activity data is persisted and no database migration is required.

The optional grouped setting is:

```ini
[Debug]
BloodborneFeedTrace=true
```

The default is `false`. The legacy flat key `BloodborneFeedTrace` is also
accepted. When enabled, the server logs the target npid, byte count, SHA-256,
top-level field types, and sanitized JSON. Authorization, bearer, token,
password, passphrase, secret, session, credential, and cookie values are
redacted. The Authorization request header is never logged.
