# Root Chalice co-op investigation

## Current boundary

The captured Root Chalice run reaches summon discovery, every shadNet filter,
room creation, claim delivery, signaling `ESTABLISHED`, and
`MUTUAL_ACTIVATED`. Unlike the vanilla Chalice control, the Root guest does not
subsequently call `sceNpMatching2JoinRoom`.

The current shadNet implementation stores the original
`/summon_messenger/create` JSON and returns it without rebuilding its members.
Unknown top-level members, nested objects and arrays, opaque `SummonData`, and
unsigned game-owned character identifiers are therefore preserved. The only
documented response-side modification is the version-3 availability count at
decoded `SummonData` offset `0x79`; location spoofing remains exclusive to the
separate seamless-anywhere path. Claim delivery likewise copies raw members
from `/summon_messenger/request` and replaces only the response envelope.

This rules out a general loss of Root fields in the broker, but it does not
prove why Bloodborne declines to submit `JoinRoom`. The cause remains after the
REST broker and before the guest's NP JoinRoom API call. No server-side forced
join, Root-specific ChannelId, glyph, account, delay, or fabricated field is
safe at this boundary.

## Diagnostic

Set the following temporarily and reproduce one vanilla Chalice plus one Root
Chalice with the same two clients:

```ini
[Debug]
BloodborneSummonTrace=true
```

The `[BLOODBORNE SUMMON TRACE]` records include sanitized request and response
JSON metadata, body sizes and SHA-256 values, host-placement size/hash, channel
identity, broker state, and every discovery filter. Session identifiers,
passwords and opaque blobs are redacted to length and SHA-256 metadata.

The next useful client-side diagnostic must observe Bloodborne's decision after
signaling activation and before its call to `sceNpMatching2JoinRoom`. It should
reuse already verified Bloodborne 1.09 trace sites, not force the call or guess
new offsets. The cumulative readback build is intentionally left unchanged by
this server-only work.

When reporting a retry, include the complete shadNet session log from both the
vanilla control and Root attempt so their sanitized body hashes and filter
outcomes can be compared in sequence.

