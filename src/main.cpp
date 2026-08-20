// Tavern Scribe Live — ESP32 table effects
//
// Subscribes to your campaign's real-time room and fires two relay channels
// on table moments, with zero polling delay:
//   • natural 20  → celebration channel (PIN_RELAY_NAT20) flashes
//   • natural 1   → doom channel (PIN_RELAY_NAT1) flashes
//
// First boot (or after holding BOOT ~5s): the device opens a WiFi access
// point named TavernScribe-Live-XXXX with a captive portal where you enter
// your WiFi plus three Tavern Scribe fields: API host, campaign id, and an
// access token for any member of the campaign. Everything persists in NVS.

#include <Arduino.h>
#include <WiFiManager.h>
#include <Preferences.h>

#include "config.h"
#include "ts_signalr_client.h"

static TsSignalRClient tsClient;
static Preferences prefs;

// ─── Relay effect state (non-blocking; loop()-driven) ────────────────────
struct Effect {
    uint8_t pin;
    unsigned long until = 0;
    unsigned long lastToggle = 0;
    bool on = false;
};
static Effect nat20Effect{PIN_RELAY_NAT20};
static Effect nat1Effect{PIN_RELAY_NAT1};

static void relayWrite(uint8_t pin, bool on) {
#if RELAY_ACTIVE_LOW
    digitalWrite(pin, on ? LOW : HIGH);
#else
    digitalWrite(pin, on ? HIGH : LOW);
#endif
}

static void startEffect(Effect& e) {
    e.until = millis() + EFFECT_DURATION_MS;
    e.lastToggle = 0;  // flash immediately
}

static void runEffect(Effect& e) {
    unsigned long now = millis();
    if (now >= e.until) {
        if (e.on) { e.on = false; relayWrite(e.pin, false); }
        return;
    }
    if (now - e.lastToggle >= EFFECT_FLASH_MS) {
        e.on = !e.on;
        relayWrite(e.pin, e.on);
        e.lastToggle = now;
    }
}

// ─── Config portal ───────────────────────────────────────────────────────
static String cfgHost, cfgCampaignId, cfgToken;

static void loadConfig() {
    prefs.begin("tslive", true);
    cfgHost = prefs.getString("host", DEFAULT_TS_HOST);
    cfgCampaignId = prefs.getString("campaign", "");
    cfgToken = prefs.getString("token", "");
    prefs.end();
}

static void saveConfig() {
    prefs.begin("tslive", false);
    prefs.putString("host", cfgHost);
    prefs.putString("campaign", cfgCampaignId);
    prefs.putString("token", cfgToken);
    prefs.end();
}

static void runPortal(bool forcePortal) {
    WiFiManager wm;

    WiFiManagerParameter pHost("host", "Tavern Scribe API host", cfgHost.c_str(), 64);
    WiFiManagerParameter pCampaign("campaign", "Campaign ID", cfgCampaignId.c_str(), 64);
    // Access tokens are long JWTs — give the field room.
    WiFiManagerParameter pToken("token", "Access token (campaign member)", cfgToken.c_str(), 1600);
    wm.addParameter(&pHost);
    wm.addParameter(&pCampaign);
    wm.addParameter(&pToken);

    String apName = "TavernScribe-Live-" + String((uint32_t)ESP.getEfuseMac(), HEX).substring(0, 4);

    bool connected = forcePortal
        ? wm.startConfigPortal(apName.c_str())
        : wm.autoConnect(apName.c_str());

    if (!connected) {
        DEBUG_PRINTLN("[WiFi] portal timed out — restarting");
        ESP.restart();
    }

    cfgHost = pHost.getValue();
    cfgCampaignId = pCampaign.getValue();
    cfgToken = pToken.getValue();
    saveConfig();
}

static void checkResetButton() {
    static unsigned long heldSince = 0;
    if (digitalRead(PIN_RESET_BUTTON) == LOW) {
        if (heldSince == 0) heldSince = millis();
        if (millis() - heldSince > 5000) {
            DEBUG_PRINTLN("[Reset] wiping WiFi + Tavern Scribe config");
            prefs.begin("tslive", false);
            prefs.clear();
            prefs.end();
            WiFiManager wm;
            wm.resetSettings();
            ESP.restart();
        }
    } else {
        heldSince = 0;
    }
}

// ─── Arduino lifecycle ───────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    DEBUG_PRINTLN("\nTavern Scribe Live — table effects firmware");

    pinMode(PIN_RELAY_NAT20, OUTPUT);
    pinMode(PIN_RELAY_NAT1, OUTPUT);
    pinMode(PIN_STATUS_LED, OUTPUT);
    pinMode(PIN_RESET_BUTTON, INPUT_PULLUP);
    relayWrite(PIN_RELAY_NAT20, false);
    relayWrite(PIN_RELAY_NAT1, false);

    loadConfig();
    runPortal(cfgCampaignId.isEmpty() || cfgToken.isEmpty());

    DEBUG_PRINTF("[WiFi] connected: %s\n", WiFi.localIP().toString().c_str());

    tsClient.onStateChange([](TsConnectionState state) {
        // Status LED: solid = live, off = down (blink handled in loop while connecting)
        digitalWrite(PIN_STATUS_LED, state == TsConnectionState::LIVE ? HIGH : LOW);
    });

    tsClient.onDiceRoll([](const TsDiceRoll& roll) {
        if (roll.isNatural20) startEffect(nat20Effect);
        if (roll.isNatural1) startEffect(nat1Effect);
    });

    tsClient.begin(cfgHost, cfgCampaignId, cfgToken);
    tsClient.connect();
}

void loop() {
    tsClient.loop();
    runEffect(nat20Effect);
    runEffect(nat1Effect);
    checkResetButton();

    // Slow blink while not live so the tinker can see it's trying.
    if (!tsClient.isLive()) {
        digitalWrite(PIN_STATUS_LED, (millis() / 500) % 2 ? HIGH : LOW);
    }
}
