# Tavern Scribe Live — ESP32 Table Effects

Firmware that plugs a physical table into a Tavern Scribe campaign. An ESP32
connects to the receive-only automation socket and switches relay channels when
named campaign events arrive — no polling and no user login baked into the box.

The starter maps two names by default:

- `room.celebration` → primary relay (GPIO 16)
- `room.doom` → secondary relay (GPIO 17)

Create those events (or rename the constants in `include/config.h`), attach
them to VTT zone actions, and let a door, trap, boss arena, or manual test
change the room. Fork it, add LEDs or sound, and make it yours.

> Product guide and project ideas: **https://www.tavernscribe.com/tinkers**

## Hardware

| Part | Notes |
|---|---|
| ESP32 dev board | ESP-WROOM-32 / ESP32 DevKit V1 class |
| 2-channel relay module | Active-LOW, opto-isolated recommended |
| Lights / effects | Prefer low-voltage LED strips or properly enclosed modules |
| Jumper wires + 5V supply | Relay coils need a stable supply |

| Function | GPIO |
|---|---|
| Primary relay | 16 |
| Secondary relay | 17 |
| Status LED | 2 |
| Config reset | 0 (hold BOOT about 5 seconds) |

> **Mains voltage is not a cantrip.** Use properly rated, enclosed equipment or
> stay low-voltage.

## Tavern Scribe setup

1. Open the campaign and go to **Settings → Integrations → Device access & custom events**.
2. Create a named event such as `room.celebration`. Choose whether it is safe
   for table-scoped devices or only DM-scoped devices.
3. Optionally add **Broadcast an event** to a VTT zone and select that event.
4. Create a device key. Give the key only the event names this device needs.
5. Copy the `tsdk_...` value when it is shown. Tavern Scribe stores only its
   SHA-256 digest and cannot reveal the key again.

Device keys are campaign-bound, receive-only, long-running, and independently
revocable. They cannot call campaign APIs, mutate game state, or impersonate a
player. Revocation cuts off an already-connected socket at the next event.
The firmware stores the key in the ESP32's NVS, so treat physical access to the
device as access to that key and revoke it if the device is lost or repurposed.

## Firmware setup

1. Install [PlatformIO](https://platformio.org/).
2. Run `pio run -t upload`, then `pio device monitor`.
3. On first boot, join the `TavernScribe-Live-XXXX` Wi-Fi network.
4. In the captive portal, enter Wi-Fi, the API host
   (`api.tavernscribe.com`), and the one-time `tsdk_...` device key.
5. The status LED goes solid when the live socket is ready.

## Wire contract

The client uses the SignalR JSON protocol directly:

```text
wss://{host}/hubs/automations?access_token={tsdk_device_key}
  → {"protocol":"json","version":1}␞
  ← {}␞
  ← CustomEventReceived(envelope)
```

`CustomEventReceived` uses the same canonical envelope as webhooks. The
firmware reads `data.name` and also exposes the complete JSON to your callback.
See [docs/EVENTS.md](docs/EVENTS.md).

## Webhooks and live devices

- Use the live device socket for low-latency effects on hardware that can hold
  an outbound WebSocket connection.
- Use signed webhooks for durable delivery, retries, n8n/Make/Zapier flows, or
  services with a public HTTPS endpoint.
- A named custom event can explicitly fan out to either or both rails. Existing
  webhook subscriptions never receive custom events until the DM opts them in.

## Credits

Structure adapted from
[loji-dev/esp32-websocket-relay](https://github.com/loji-dev/esp32-websocket-relay).

## License

MIT — build cool things at your table.
