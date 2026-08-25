#include "ts_signalr_client.h"
#include "config.h"

static const char RECORD_SEPARATOR = 0x1e;

void TsSignalRClient::begin(const String& host, const String& deviceKey) {
    _host = host;
    _token = deviceKey;
    DEBUG_PRINTF("[TS] automation client ready: host=%s\n", _host.c_str());
}

void TsSignalRClient::connect() {
    if (_state == TsConnectionState::CONNECTING || _state == TsConnectionState::LIVE) return;
    setState(TsConnectionState::CONNECTING);
    _handshakeAcked = false;
    _rxBuffer = "";
    String path = "/hubs/automations?access_token=" + _token;
    DEBUG_PRINTF("[TS] connecting wss://%s/hubs/automations?...\n", _host.c_str());
    _ws.beginSSL(_host.c_str(), 443, path.c_str());
    _ws.onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
        this->handleWsEvent(type, payload, length);
    });
    _ws.setReconnectInterval(0);
}

void TsSignalRClient::disconnect() {
    _ws.disconnect();
    setState(TsConnectionState::DISCONNECTED);
}

void TsSignalRClient::loop() {
    _ws.loop();
    if (_state == TsConnectionState::RECONNECT_WAIT && millis() >= _reconnectAt) connect();
    if (_state == TsConnectionState::LIVE && millis() - _lastPing >= SIGNALR_PING_INTERVAL_MS) {
        sendPing();
        _lastPing = millis();
    }
}

void TsSignalRClient::handleWsEvent(WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_CONNECTED:
            DEBUG_PRINTLN("[TS] socket open - sending SignalR handshake");
            setState(TsConnectionState::HANDSHAKING);
            sendHandshake();
            break;
        case WStype_TEXT: {
            _rxBuffer.concat((const char*)payload, length);
            int sep;
            while ((sep = _rxBuffer.indexOf(RECORD_SEPARATOR)) >= 0) {
                String frame = _rxBuffer.substring(0, sep);
                _rxBuffer.remove(0, sep + 1);
                if (frame.length() > 0) handleFrame(frame);
            }
            break;
        }
        case WStype_DISCONNECTED:
        case WStype_ERROR:
            DEBUG_PRINTLN("[TS] socket unavailable");
            if (_state != TsConnectionState::DISCONNECTED) scheduleReconnect();
            break;
        default:
            break;
    }
}

void TsSignalRClient::handleFrame(const String& frame) {
    if (!_handshakeAcked) {
        _handshakeAcked = true;
        if (frame.indexOf("error") >= 0) {
            DEBUG_PRINTF("[TS] handshake rejected: %s\n", frame.c_str());
            scheduleReconnect();
            return;
        }
        DEBUG_PRINTLN("[TS] handshake accepted - live");
        setState(TsConnectionState::LIVE);
        _lastPing = millis();
        _reconnectDelay = 0;
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, frame);
    if (err) {
        DEBUG_PRINTF("[TS] frame parse error: %s\n", err.c_str());
        return;
    }
    switch ((int)(doc["type"] | 0)) {
        case 1: handleInvocation(doc.as<JsonObject>()); break;
        case 6: break;
        case 7:
            DEBUG_PRINTF("[TS] server close: %s\n", (const char*)(doc["error"] | ""));
            scheduleReconnect();
            break;
        default: break;
    }
}

void TsSignalRClient::handleInvocation(JsonObject msg) {
    const char* target = msg["target"] | "";
    if (strcmp(target, "CustomEventReceived") != 0) return;
    JsonObject envelope = msg["arguments"][0];
    JsonObject data = envelope["data"];
    if (envelope.isNull() || data.isNull()) return;

    TsCustomEvent event;
    event.id = (const char*)(envelope["id"] | "");
    event.name = (const char*)(data["name"] | "");
    const char* displayName = data["displayName"] | "";
    event.displayName = displayName[0] == '\0' ? event.name : displayName;
    event.origin = (const char*)(data["origin"] | "");
    serializeJson(envelope, event.payloadJson);
    DEBUG_PRINTF("[TS] custom event: %s (%s)\n", event.name.c_str(), event.origin.c_str());
    if (_onCustomEvent) _onCustomEvent(event);
}

void TsSignalRClient::sendHandshake() {
    String handshake = String("{\"protocol\":\"json\",\"version\":1}") + RECORD_SEPARATOR;
    _ws.sendTXT(handshake);
}

void TsSignalRClient::sendPing() {
    String ping = String("{\"type\":6}") + RECORD_SEPARATOR;
    _ws.sendTXT(ping);
}

void TsSignalRClient::setState(TsConnectionState next) {
    if (_state == next) return;
    _state = next;
    if (_onState) _onState(next);
}

void TsSignalRClient::scheduleReconnect() {
    _reconnectDelay = _reconnectDelay == 0
        ? RECONNECT_INITIAL_DELAY_MS
        : min((unsigned long)(_reconnectDelay * 2), (unsigned long)RECONNECT_MAX_DELAY_MS);
    _reconnectAt = millis() + _reconnectDelay;
    DEBUG_PRINTF("[TS] reconnecting in %lums\n", _reconnectDelay);
    setState(TsConnectionState::RECONNECT_WAIT);
}
