# Tavern Scribe Live — ESP32 Table Effects

Firmware that plugs your **real table** into your [Tavern Scribe](https://www.tavernscribe.com) campaign. An ESP32 subscribes directly to your campaign's real-time room over websockets — the same room the app itself uses — and fires relay channels the instant a table moment happens. Zero polling, zero delay:

- 🎲 **Natural 20** → celebration channel flashes (gold light, glitter cannon, whatever you wire)
- 💀 **Natural 1** → doom channel flashes (blood-red bulb, thunder sound board, your call)

This is the starter rig. Fork it, rewire it, make it weirder — see [What else you can listen to](#what-else-you-can-listen-to).

> Full guide with hardware links: **https://www.tavernscribe.com/tinkers**

## Hardware

| Part | Notes |
|---|---|
| ESP32 dev board | ESP-WROOM-32 / "ESP32 DevKit V1" class, ~$8 |
| 2-channel relay module | Active-LOW, opto-isolated recommended, ~$6 |
| Lights / effects | Anything the relay can switch — LED bulbs, tower lights, a smart plug's dumb cousin |
| Jumper wires + 5V supply | Relay coils want solid 5V |

### Wiring (default pins — change in `include/config.h`)

| Function | GPIO |
|---|---|
| Nat-20 relay (celebration) | 16 |
| Nat-1 relay (doom) | 17 |
| Status LED | 2 (built-in) |
| Config reset | 0 (BOOT button, hold ~5s) |

> ⚠️ **Mains voltage is not a cantrip.** If you switch wall power through a relay, use a properly rated, enclosed module — or stay low-voltage (12V LED strips are plenty dramatic).

## Setup

1. Install [PlatformIO](https://platformio.org/) (`pip install platformio` or the VS Code extension).
2. `pio run -t upload` then `pio device monitor`.
3. On first boot the device opens a WiFi AP named `TavernScribe-Live-XXXX`. Connect to it — a captive portal opens.
4. Enter your WiFi credentials plus three Tavern Scribe fields:
   - **API host** — `api.tavernscribe.com` (prefilled)
   - **Campaign ID** — from your campaign's URL: `…/campaign/{THIS-PART}/recap`
   - **Access token** — a signed-in campaign member's token (any member works; a dedicated "table effects" account is tidy). Grab it from your browser's devtools while on the app (Application → Local Storage → access token), or ask in our [Discord](https://discord.gg/rRfHDgunkY) — a friendlier device-token flow is on the roadmap.
5. The status LED blinks while connecting and goes **solid when live**. Roll a d20 in party chat. Enjoy the light show.

## How it works

The firmware speaks the SignalR JSON protocol over a raw websocket — no SDK needed on-device:

```
wss://{host}/hubs/users?campaignId={id}&access_token={token}
  → {"protocol":"json","version":1}␞          (handshake)
  ← ReadyForRegistration                       (server invocation)
  → RegisterUserConnection(token)
  → JoinPartyChatGroup(campaignId)
  ← PartyChatMessageReceived(message)          (every table message, live)
       messageType == 1 → diceRollJson → { isNatural20, isNatural1, total, … }
```

`src/ts_signalr_client.cpp` is a ~200-line reusable client: handshake, frame splitting on the `0x1e` record separator, invocations, type-6 keepalives, exponential-backoff reconnect. Point your own handler at any hub event the app broadcasts.

## What else you can listen to

Everything the app broadcasts into the rooms you join is yours to react to — dice are just the demo. And the platform side is growing on purpose:

- **App events** (shipping now): notable moments — kills, deaths, level-ups, loot claims, recap published, **map zone triggers** — are being captured as first-class events.
- **Webhooks** (rolling out): the same moments POSTed to any URL, signed, with retries — for rigs that live behind n8n / Zapier / Home Assistant instead of on the table.
- **Map triggers → the real world**: a DM drops a trigger zone on the map; a token steps in; your fog machine coughs. That's the goal of this whole direction.

**Full event reference: [docs/EVENTS.md](docs/EVENTS.md)** — including `AppEventRecorded`, the live-room broadcast that pushes every notable moment (kills, deaths, level-ups, loot, recap published) into the room this firmware already sits in, the instant it happens. Watch the [tinkers page](https://www.tavernscribe.com/tinkers) and this repo as the webhook layer lands.

## Credits

Structure adapted from [loji-dev/esp32-websocket-relay](https://github.com/loji-dev/esp32-websocket-relay), the office-alert rig this grew out of.

## License

MIT — build cool things at your table.
