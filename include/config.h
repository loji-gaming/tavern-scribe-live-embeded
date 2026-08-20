#ifndef CONFIG_H
#define CONFIG_H

// ─── Debug output ────────────────────────────────────────────────────────
#define DEBUG_ENABLED 1
#if DEBUG_ENABLED
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)
#define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINTF(...)
#endif

// ─── Pins (adjust to your wiring) ────────────────────────────────────────
// Relay modules are usually ACTIVE LOW: LOW = relay closed = light on.
#define PIN_RELAY_NAT20 16   // celebration channel (gold/green light)
#define PIN_RELAY_NAT1 17    // doom channel (red light)
#define PIN_STATUS_LED 2     // built-in LED: connection status
#define PIN_RESET_BUTTON 0   // hold BOOT ~5s to wipe WiFi + campaign config
#define RELAY_ACTIVE_LOW 1

// ─── Effects ─────────────────────────────────────────────────────────────
#define EFFECT_DURATION_MS 4000   // how long a nat-20/nat-1 effect holds
#define EFFECT_FLASH_MS 250       // flash cadence during the effect

// ─── Tavern Scribe connection ────────────────────────────────────────────
// Host only — the firmware builds wss://{host}/hubs/users?campaignId=…&access_token=…
#define DEFAULT_TS_HOST "api.tavernscribe.com"

// ─── SignalR client tuning ───────────────────────────────────────────────
#define RECONNECT_INITIAL_DELAY_MS 2000
#define RECONNECT_MAX_DELAY_MS 60000
#define SIGNALR_PING_INTERVAL_MS 15000  // client → server type-6 keepalive

#endif // CONFIG_H
