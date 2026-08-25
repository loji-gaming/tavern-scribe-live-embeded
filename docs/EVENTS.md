# Custom Event and Transport Reference

## Live automation socket

Connect to:

```text
wss://api.tavernscribe.com/hubs/automations?access_token=tsdk_...
```

The server accepts only a scoped device key created by a campaign DM. The hub
has no public client methods; it only sends `CustomEventReceived` for event
names and audience levels allowed by that key.

```json
{
  "id": "event-id",
  "type": "custom.event",
  "createdAt": "2026-08-25T20:15:00Z",
  "apiVersion": "2026-08-01",
  "campaign": { "id": "campaign-id" },
  "data": {
    "name": "room.celebration",
    "displayName": "Celebration lights",
    "definitionId": "definition-id",
    "origin": "vtt.zone",
    "schemaVersion": 1,
    "context": { "mapId": "map-id" },
    "attributes": { "color": "gold", "durationSeconds": 4 }
  }
}
```

Table-scoped keys do not receive DM-only events. Their envelope also omits
private zone, trigger, source-event, and token identifiers. DM-scoped keys may
receive either audience and the complete context. Character account IDs never
appear in the external envelope.

The live rail is immediate and best-effort. Reconnect with exponential backoff
and make effects safe to repeat. The stable event `id` can be cached if a
device must suppress duplicates across reconnects.

## Named event rules

- Names are lowercase dotted machine text such as `crypt.trap_sprung`.
- A name is immutable and cannot be reused after archival.
- Definitions independently control manual triggers, VTT-zone triggers,
  webhook routing, live routing, and live audience.
- Attributes are optional scalar values: string, finite number, or boolean.
  A receipt carries at most 16 attributes and 4 KiB of attribute JSON.
- A campaign may emit up to 10 named events per second and 1,000 in a rolling
  day. Webhook deliveries use their own 120-per-hour campaign budget, so a
  noisy custom event cannot starve built-in recap or session deliveries.
- The server attaches trusted context for zone emissions; clients should not
  infer authorization from user-supplied attributes.

## Signed webhooks

A DM opts a subscription into all custom events or selected custom names.
Custom events are never included by the built-in “all eligible events” default,
which protects existing endpoints from a surprise new payload family.

The body matches the live envelope. Custom deliveries add:

```text
X-TavernScribe-Event: custom.event
X-TavernScribe-Custom-Event: room.celebration
X-TavernScribe-Delivery: delivery-id
X-TavernScribe-Signature: t={unix},v1={lowercase-hmac-sha256}
```

Verify HMAC-SHA256 over `{t}.{rawRequestBody}` before parsing. Use `id` as the
logical idempotency key. Webhooks retry on the documented ladder and keep a
delivery log; the live device rail does not replay missed events.

## Choosing a rail

| Need | Use |
|---|---|
| Lights or sound reacting within seconds | Live device socket |
| n8n, Make, Zapier, Home Assistant relay | Signed webhook |
| Delivery retries and operator replay | Signed webhook |
| Local embedded device without inbound networking | Live device socket |
| Both immediate hardware and durable workflow | Enable both on one named event |
