# Bloodborne Makeshift Altar and activity feed

## Makeshift Altar phase 1

The summon broker recognizes the captured Quick Search pair only when all of
the following are true:

- the search request and candidate both use `SummonMethod=1`;
- the search request is in Hunter's Dream (`AreaId=352321536`) with
  `ChannelId=0`;
- the candidate is outside Hunter's Dream and has a positive, real
  `ChannelId`.

For that pair only, discovery does not compare `AreaId`, `AreaRegionId`,
`ChannelId`, position, or distance. User/session isolation, data version,
method, summon type, password, level, candidate state, lifetime, and result
limit remain active. The candidate advertisement is returned unchanged, so
the target Chalice channel is not replaced with zero and different Root
Chalices remain distinct.

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
