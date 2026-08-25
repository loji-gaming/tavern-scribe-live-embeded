# Tavern Scribe Live — ESP32 Table Effects

Firmware that plugs your **real table** into your [Tavern Scribe](https://www.tavernscribe.com) campaign. An ESP32 joins the same real-time party-chat path used by the app and fires relay channels when a shared dice message lands. No polling:

- 🎲 **Natural 20** → celebration channel flashes (gold light, glitter cannon, whatever you wire)
- 💀 **Natural 1** → doom channel flashes (blood-red bulb, thunder sound board, your call)

This is the starter rig. Fork it, rewire it, make it weirder — then use Tavern Scribe's signed webhooks when you need the broader campaign-event catalog.

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
   - **Access token** — a signed-in campaign member's token. This is a real login credential: protect it, keep the device under your control, and do not expose the configuration portal. A dedicated "table effects" member limits practical exposure. Scoped device keys are on the roadmap.
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

`src/ts_signalr_client.cpp` is a ~200-line reusable client: handshake, frame splitting on the `0x1e` record separator, invocations, type-6 keepalives, and exponential-backoff reconnect. The demonstrated contract is `PartyChatMessageReceived`; other app hub messages are internal implementation details unless they are separately documented.

## Two integration paths

This firmware demonstrates one focused real-time path: shared party-chat dice messages. Natural 20s and natural 1s can therefore reach an on-table relay without polling.

For the complete campaign-event catalog, use **signed outbound webhooks**, available now in Campaign Settings → Webhooks. Choose event types such as kills, character death and revival, level-ups, loot claims, map-zone triggers, session transitions, and published recaps or newspapers. Tavern Scribe sends a stable JSON envelope to your public HTTPS receiver with HMAC signatures, retry handling, a delivery log, test sends, and replay.

That separation is intentional:

- **SignalR party-chat dice path** — a low-latency developer starter for hardware physically at the table.
- **Signed webhooks** — the stable automation contract for n8n, Make, Zapier webhook triggers, secure Home Assistant relays, or your own service.

See the exact boundaries and payload examples in **[docs/EVENTS.md](docs/EVENTS.md)**, or explore the full [Tavern Scribe builder guide](https://www.tavernscribe.com/tinkers).

## Credits

Structure adapted from [loji-dev/esp32-websocket-relay](https://github.com/loji-dev/esp32-websocket-relay), the office-alert rig this grew out of.

## License

MIT — build cool things at your table.
