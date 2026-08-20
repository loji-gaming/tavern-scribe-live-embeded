#include "ts_signalr_client.h"
#include "config.h"

// SignalR frames are terminated by the ASCII record separator.
static const char RECORD_SEPARATOR = 0x1e;

void TsSignalRClient::begin(const String& host, const String& campaignId, const String& token) {
    _host = host;
    _campaignId = campaignId;
    _token = token;

    DEBUG_PRINTF("[TS] client ready: host=%s campaign=%s\n", _host.c_str(), _campaignId.c_str());
}

void TsSignalRClient::connect() {
    if (_state == TsConnectionState::CONNECTING || _state == TsConnectionState::LIVE) {
        return;
    }

    setState(TsConnectionState::CONNECTING);
    _handshakeAcked = false;
    _rxBuffer = "";

    // The hub authenticates websockets from the access_token query parameter
    // (the standard ASP.NET Core SignalR pattern the web client uses too).
    String path = "/hubs/users?campaignId=" + _campaignId + "&access_token=" + _token;

    DEBUG_PRINTF("[TS] connecting wss://%s%s\n", _host.c_str(), "/hubs/users?...");
    _ws.beginSSL(_host.c_str(), 443, path.c_str());
    _ws.onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
        this->handleWsEvent(type, payload, length);
    });
    _ws.setReconnectInterval(0);  // we own reconnect pacing
}

void TsSignalRClient::disconnect() {
    _ws.disconnect();
    setState(TsConnectionState::DISCONNECTED);
}

void TsSignalRClient::loop() {
    _ws.loop();

    if (_state == TsConnectionState::RECONNECT_WAIT && millis() >= _reconnectAt) {
        connect();
    }

    if (_state == TsConnectionState::LIVE && millis() - _lastPing >= SIGNALR_PING_INTERVAL_MS) {
        sendPing();
        _lastPing = millis();
    }
}

void TsSignalRClient::handleWsEvent(WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_CONNECTED:
            DEBUG_PRINTLN("[TS] socket open — sending SignalR handshake");
            setState(TsConnectionState::HANDSHAKING);
            sendHandshake();
            break;

        case WStype_TEXT: {
            // A websocket message can carry several \x1e-separated frames, and a
            // frame can theoretically split across messages — buffer + split.
            _rxBuffer.concat((const char*)payload, length);
            int sep;
            while ((sep = _rxBuffer.indexOf(RECORD_SEPARATOR)) >= 0) {
                String frame = _rxBuffer.substring(0, sep);
                _rxBuffer.remove(0, sep + 1);
                if (frame.length() > 0) {
                    handleFrame(frame);
                }
            }
            break;
        }

        case WStype_DISCONNECTED:
            DEBUG_PRINTLN("[TS] socket closed");
            if (_state != TsConnectionState::DISCONNECTED) {
                scheduleReconnect();
            }
            break;

        case WStype_ERROR:
            DEBUG_PRINTLN("[TS] socket error");
            scheduleReconnect();
            break;

        default:
            break;
    }
}

void TsSignalRClient::handleFrame(const String& frame) {
    // First frame after the handshake is the handshake response: {} on success,
    // {"error": "..."} on failure. It has no "type" field.
    if (!_handshakeAcked) {
        _handshakeAcked = true;
        if (frame.indexOf("error") >= 0) {
            DEBUG_PRINTF("[TS] handshake rejected: %s\n", frame.c_str());
            scheduleReconnect();
            return;
        }
        DEBUG_PRINTLN("[TS] handshake accepted — waiting for ReadyForRegistration");
        setState(TsConnectionState::JOINING);
        return;
    }

    // Dice payloads carry per-die arrays; a chatty table message stays < 2KB.
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, frame);
    if (err) {
        DEBUG_PRINTF("[TS] frame parse error: %s\n", err.c_str());
        return;
    }

    int type = doc["type"] | 0;
    switch (type) {
        case 1:  // Invocation (server → client)
            handleInvocation(doc.as<JsonObject>());
            break;
        case 6:  // Ping — reply-in-kind is handled by our own ping cadence.
            break;
        case 7:  // Close
            DEBUG_PRINTF("[TS] server close: %s\n", (const char*)(doc["error"] | ""));
            scheduleReconnect();
            break;
        default:
            break;  // completions/stream items — not used by this device
    }
}

void TsSignalRClient::handleInvocation(JsonObject msg) {
    const char* target = msg["target"] | "";

    if (strcmp(target, "ReadyForRegistration") == 0) {
        // Same two invokes the web app makes before it can hear the table.
        DEBUG_PRINTLN("[TS] registering connection + joining the party chat room");
        invoke1("RegisterUserConnection", _token);
        invoke1("JoinPartyChatGroup", _campaignId);
        setState(TsConnectionState::LIVE);
        _lastPing = millis();
        _reconnectDelay = 0;  // healthy connection resets backoff
        return;
    }

    if (strcmp(target, "PartyChatMessageReceived") == 0) {
        JsonObject message = msg["arguments"][0];
        if (message.isNull()) return;

        // messageType: 0=Text, 1=DiceRoll, 2=System, 3=Whisper, …
        int messageType = message["messageType"] | -1;
        if (messageType != 1) return;

        const char* rollJson = message["diceRollJson"] | "";
        if (rollJson[0] == '\0') return;

        JsonDocument rollDoc;
        if (deserializeJson(rollDoc, rollJson)) return;

        TsDiceRoll roll;
        roll.senderName = (const char*)(message["displayName"] | message["senderName"] | "Someone");
        roll.expression = (const char*)(rollDoc["expression"] | "");
        roll.total = rollDoc["total"] | 0;
        roll.isNatural20 = rollDoc["isNatural20"] | false;
        roll.isNatural1 = rollDoc["isNatural1"] | false;

        DEBUG_PRINTF("[TS] roll: %s %s = %d%s%s\n",
            roll.senderName.c_str(), roll.expression.c_str(), roll.total,
            roll.isNatural20 ? "  NAT 20!" : "", roll.isNatural1 ? "  nat 1…" : "");

        if (_onDiceRoll) _onDiceRoll(roll);
    }
}

void TsSignalRClient::sendHandshake() {
    String handshake = String("{\"protocol\":\"json\",\"version\":1}") + RECORD_SEPARATOR;
    _ws.sendTXT(handshake);
}

void TsSignalRClient::sendPing() {
    String ping = String("{\"type\":6}") + RECORD_SEPARATOR;
    _ws.sendTXT(ping);
}

void TsSignalRClient::invoke1(const String& target, const String& arg) {
    JsonDocument doc;
    doc["type"] = 1;
    doc["invocationId"] = String(++_invocationId);
    doc["target"] = target;
    JsonArray args = doc["arguments"].to<JsonArray>();
    args.add(arg);

    String frame;
    serializeJson(doc, frame);
    frame += RECORD_SEPARATOR;
    _ws.sendTXT(frame);
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
