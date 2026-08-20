# Event Reference

Two delivery channels, one event taxonomy. This page is the tinker's map of
what you can hear and where.

## Channel 1 — Live room (websocket, zero delay)

Join `party_chat:{campaignId}` on `/hubs/users` (this firmware's client does it
for you) and listen for these server invocations:

### `AppEventRecorded` — the app-event spine, live

Every notable campaign moment, one envelope, the instant it's recorded:

```json
{
  "id": "…", "eventType": "dice.natural_20",
  "actorUserId": "…", "characterId": "…", "sessionId": "…", "entityId": "…",
  "payload": { "name": "Elyra", "expression": "1d20+7", "total": 27 },
  "createdAt": "2026-08-20T21:03:11Z"
}
```

| eventType | Fires when | Key payload |
|---|---|---|
| `dice.natural_20` | a nat 20 lands in party chat / VTT dice | `name`, `expression`, `total`, `context?` |
| `dice.natural_1` | a nat 1 lands | same |
| `combat.enemy_killed` | a creature's fate is stamped killed (HP hit 0 or DM toggle) | `creatureName`, `sceneId?` |
| `character.died` | a character enters the graveyard | `characterName` |
| `character.revived` | …and comes back | `characterName` |
| `kill.confirmed` | the post-session AI confirms a kill from the transcript | `killerName`, `victimName`, `isCriticalHit?`, `killMethod?` |
| `party.level_up` | the DM levels the party | `newLevel` |
| `loot.claimed` | a player claims loot | `itemName`, `claimerName` |
| `member.joined` | someone accepts a campaign invite | `memberName` |
| `session.recording_started` | a recording session begins (Discord bot) | `source`, `sessionTitle?` |
| `session.table_started` | the VTT table goes live | `gameSessionId`, `mapId?` |
| `session.table_ended` | the table wraps | — |
| `session.processing_complete` | the AI finishes processing a session | `sessionTitle` |
| `recap.published` | the recap ships (share link included) | `sessionTitle`, `recapUrl` |
| `newspaper.published` | the campaign newspaper ships | `sessionTitle`, `newspaperUrl` |

Deliberately **not** in the live room:
- `map.zone_triggered` — zone triggers can be hidden traps; they stay on the
  DM-only `ZoneTriggered` channel (MapTokenHub) so a player's device can't
  spoil them. DM-owned rigs: listen there, or catch the webhook.
- `character.status_event` — post-session transcript facts, not live moments.

### Pre-existing broadcasts (same room, live today)

The room also carries the app's own real-time traffic — the starter firmware
uses the first one:

| Invocation | What it is |
|---|---|
| `PartyChatMessageReceived` | every table message; `messageType == 1` = dice, parse `diceRollJson` → `isNatural20` / `isNatural1` / `total` |
| `PartyLevelChanged` | party level changed |
| `SheetDataUpdated` | a character sheet changed (IDs only — refetch) |

The VTT hub (`/hubs/maptokens`, `campaign:{campaignId}` room) additionally
broadcasts token HP updates, initiative state, `GameSessionStarted`/`Ended`,
`LootItemClaimed`, music/weather/theater cues, and the DM-only `ZoneTriggered`.
There are ~80 constants in total — the two tables above are the curated
tinker-relevant set.

## Channel 2 — Signed webhooks (rolling out)

The same `eventType` taxonomy POSTed to any URL with HMAC signatures, retries,
and delivery logs — for rigs behind n8n / Zapier / Make / Home Assistant.
**Webhook-eligible set** = everything in the `AppEventRecorded` table above
**plus `map.zone_triggered`** (`zoneName`, `mapId`, `sourceEvent`) — the DM
subscribing their own endpoint to their own hidden traps is the point: token
steps in the zone, your fog machine coughs.

Watch this repo and https://www.tavernscribe.com/tinkers — this doc gets
updated as the hooks land.
