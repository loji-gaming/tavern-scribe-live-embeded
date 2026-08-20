#ifndef TS_SIGNALR_CLIENT_H
#define TS_SIGNALR_CLIENT_H

#include <Arduino.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <functional>

// Minimal SignalR (JSON protocol) client for Tavern Scribe's UsersHub.
//
// Connection lifecycle it implements (mirrors the web app's own flow):
//   1. wss://{host}/hubs/users?campaignId={id}&access_token={jwt}
//   2. send handshake  {"protocol":"json","version":1}\x1e
//   3. server invokes  ReadyForRegistration
//   4. we invoke       RegisterUserConnection(token), JoinPartyChatGroup(campaignId)
//   5. server invokes  PartyChatMessageReceived(message) for every table message —
//      dice rolls arrive as messageType == 1 with a diceRollJson payload.
//   6. type-6 pings both ways keep the socket warm.

enum class TsConnectionState {
    DISCONNECTED,
    CONNECTING,
    HANDSHAKING,
    JOINING,
    LIVE,
    RECONNECT_WAIT,
};

// Fired for every dice roll seen in the campaign's party chat.
struct TsDiceRoll {
    String senderName;
    String expression;
    int total = 0;
    bool isNatural20 = false;
    bool isNatural1 = false;
};

using DiceRollCallback = std::function<void(const TsDiceRoll& roll)>;
using StateCallback = std::function<void(TsConnectionState state)>;

class TsSignalRClient {
public:
    // token is the Tavern Scribe access token of a campaign member; the hub
    // authenticates the socket from the access_token query parameter.
    void begin(const String& host, const String& campaignId, const String& token);
    void connect();
    void disconnect();
    void loop();  // call every iteration of loop()

    bool isLive() const { return _state == TsConnectionState::LIVE; }
    TsConnectionState state() const { return _state; }

    void onDiceRoll(DiceRollCallback cb) { _onDiceRoll = cb; }
    void onStateChange(StateCallback cb) { _onState = cb; }

private:
    void handleWsEvent(WStype_t type, uint8_t* payload, size_t length);
    void handleFrame(const String& frame);
    void handleInvocation(JsonObject msg);
    void sendHandshake();
    void sendPing();
    void invoke(const String& target, JsonArray args);
    void invoke1(const String& target, const String& arg);
    void setState(TsConnectionState next);
    void scheduleReconnect();

    WebSocketsClient _ws;
    String _host;
    String _campaignId;
    String _token;
    String _rxBuffer;

    TsConnectionState _state = TsConnectionState::DISCONNECTED;
    bool _handshakeAcked = false;
    unsigned long _lastPing = 0;
    unsigned long _reconnectAt = 0;
    unsigned long _reconnectDelay = 0;
    int _invocationId = 0;

    DiceRollCallback _onDiceRoll = nullptr;
    StateCallback _onState = nullptr;
};

#endif // TS_SIGNALR_CLIENT_H
