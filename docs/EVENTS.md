# Event and Transport Reference

Tavern Scribe exposes two useful integration paths today. They solve different
jobs and do not carry the same payloads.

## Path 1 — live party-chat dice messages

This firmware connects to `/hubs/users`, completes the SignalR JSON handshake,
registers the current member connection, and invokes
`JoinPartyChatGroup(campaignId)`. It then listens for:

### `PartyChatMessageReceived`

The event contains a party-chat message. When `messageType == 1`, the
`diceRollJson` property holds a shared dice result with fields including:

```json
{
  "isNatural20": true,
  "isNatural1": false,
  "total": 27
}
```

The reference firmware maps a natural 20 and natural 1 to two non-blocking
relay effects. This is the demonstrated low-latency device contract.

Players can produce these messages from Tavern Scribe's VTT or character-sheet
roll controls when sharing rolls to party chat is enabled. That also makes the
starter useful at a physical table where players keep sheets on phones or
laptops while continuing to use physical minis, maps, and dice.

### Important live-path boundaries

- This starter does **not** receive the full campaign event catalog through
  `PartyChatMessageReceived`.
- `AppEventRecorded` is an internal, DM-only refetch nudge on the map-token hub.
  Its small ids-only payload is not the external campaign-event envelope and is
  not the source used by this firmware.
- Map-zone triggers use their own DM-only live handling. Use a webhook when an
  external rig needs a stable `map.zone_triggered` event.
- Other hub messages are app implementation details unless explicitly
  documented as an integration contract.
- The current firmware authenticates with a campaign-member access token.
  Treat it as a credential, keep the device and configuration portal private,
  and rotate the token if the device is lost. Scoped device keys are roadmap.

## Path 2 — signed outbound webhooks

Webhooks are available now in **Campaign Settings → Webhooks**. A campaign DM
registers a public HTTPS receiver and selects the event types that endpoint
should receive.

Each delivery uses one stable, camelCase JSON envelope:

```json
{
  "id": "evt_…",
  "type": "dice.natural_20",
  "createdAt": "2026-08-25T20:15:00Z",
  "apiVersion": "2026-08-01",
  "campaign": {
    "id": "campaign-id",
    "name": "The Amber Court"
  },
  "session": {
    "id": "session-id"
  },
  "actor": {
    "characterId": "character-id"
  },
  "data": {
    "name": "Elyra Dawnwhisper",
    "expression": "1d20+7",
    "total": 27
  }
}
```

`session` and `actor` are omitted when they do not apply. The contents of
`data` vary by event type. Internal account ids are not part of the external
contract.

### Delivery headers and signature

Every POST includes:

```text
X-TavernScribe-Event: dice.natural_20
X-TavernScribe-Delivery: delivery-id
X-TavernScribe-Signature: t={unix},v1={lowercase-hmac-sha256}
```

To verify a request, compute HMAC-SHA256 with the subscription secret over the
exact UTF-8 string `{t}.{rawRequestBody}`, compare it to `v1` using a
constant-time comparison, and reject timestamps outside your tolerance window
(five minutes is recommended). Keep the raw body until verification is
complete; parsing and re-serializing JSON first can change the bytes.

Signing secrets use the `whsec_` prefix, are revealed once, and should be stored
like passwords. The management UI supports rotating a secret when needed.

### Current webhook event catalog

| Event type | Required `data` fields |
|---|---|
| `dice.natural_20` | `name`, `expression`, `total` |
| `dice.natural_1` | `name`, `expression`, `total` |
| `combat.enemy_killed` | `creatureName` |
| `character.died` | `characterName` |
| `character.revived` | `characterName` |
| `kill.confirmed` | `killerName`, `victimName` |
| `party.level_up` | `newLevel` |
| `loot.claimed` | `itemName`, `claimerName` |
| `member.joined` | `memberName` |
| `map.zone_triggered` | `zoneName` |
| `session.recording_started` | — |
| `session.table_started` | — |
| `session.table_ended` | — |
| `session.processing_complete` | `sessionTitle` |
| `recap.published` | `sessionTitle`, `recapUrl` |
| `newspaper.published` | `sessionTitle`, `newspaperUrl` |

The catalog endpoint in Tavern Scribe provides an example payload for every
eligible type, and the settings page can send a real test delivery before a
session.

### Operational behavior

- Failed deliveries retry on a 30 seconds → 5 minutes → 30 minutes → 2 hours →
  6 hours schedule.
- Delivery history is visible to the campaign DM, and terminal failures can be
  replayed.
- An endpoint returning HTTP 410 is automatically disabled. Repeated terminal
  failures also trip the subscription breaker.
- Destinations must be public HTTPS endpoints. Private, loopback, link-local,
  and other unsafe network targets are rejected, including after DNS
  resolution.
- Webhook delivery is asynchronous. Use the live party-chat path when a few
  seconds would weaken an on-table effect; use webhooks when durability and the
  full catalog matter more.

## Which path should I use?

| You are building… | Start with… |
|---|---|
| An ESP32 light that reacts to a shared crit at the table | This repository's live SignalR client |
| An n8n, Make, or Zapier flow | Signed webhooks |
| A Home Assistant automation | Signed webhooks through a secure public relay |
| An OBS overlay or custom stream service | Signed webhooks |
| A custom low-latency dice device | This repository, then keep the live scope narrow |
| A rig reacting to kills, level-ups, map zones, or published recaps | Signed webhooks |

The [Tavern Scribe builder guide](https://www.tavernscribe.com/tinkers) keeps
the product-level overview and project ideas in one place.
