#ifndef TS_SIGNALR_CLIENT_H
#define TS_SIGNALR_CLIENT_H

#include <Arduino.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <functional>

// Minimal SignalR JSON client for Tavern Scribe's receive-only automation hub:
//   wss://{host}/hubs/automations?access_token={tsdk_device_key}
//   -> SignalR handshake
//   <- CustomEventReceived(envelope)

enum class TsConnectionState {
    DISCONNECTED,
    CONNECTING,
    HANDSHAKING,
    LIVE,
    RECONNECT_WAIT,
};

struct TsCustomEvent {
    String id;
    String name;
    String displayName;
    String origin;
    String payloadJson;
};

using CustomEventCallback = std::function<void(const TsCustomEvent& event)>;
using StateCallback = std::function<void(TsConnectionState state)>;

class TsSignalRClient {
public:
    // deviceKey is the tsdk_-prefixed key revealed once in Campaign Settings.
    // It can only receive selected events; it is not a campaign-member login.
    void begin(const String& host, const String& deviceKey);
    void connect();
    void disconnect();
    void loop();

    bool isLive() const { return _state == TsConnectionState::LIVE; }
    TsConnectionState state() const { return _state; }

    void onCustomEvent(CustomEventCallback cb) { _onCustomEvent = cb; }
    void onStateChange(StateCallback cb) { _onState = cb; }

private:
    void handleWsEvent(WStype_t type, uint8_t* payload, size_t length);
    void handleFrame(const String& frame);
    void handleInvocation(JsonObject msg);
    void sendHandshake();
    void sendPing();
    void setState(TsConnectionState next);
    void scheduleReconnect();

    WebSocketsClient _ws;
    String _host;
    String _token;
    String _rxBuffer;
    TsConnectionState _state = TsConnectionState::DISCONNECTED;
    bool _handshakeAcked = false;
    unsigned long _lastPing = 0;
    unsigned long _reconnectAt = 0;
    unsigned long _reconnectDelay = 0;
    CustomEventCallback _onCustomEvent = nullptr;
    StateCallback _onState = nullptr;
};

#endif
