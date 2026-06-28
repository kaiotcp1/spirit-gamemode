# Networking and State

Clients request, the server validates and mutates, and clients render.

Validate player and target existence, distance, active/terminal state, ownership, option bounds, equipment, and other game rules. Never accept client-computed success.

Use `[RplProp()]` for client-visible state and call `Replication.BumpMe()` after authority changes. Use reliable server RPCs for requests and reliable broadcast RPCs for one-shot results.

If a listen-server host needs a result callback, invoke the result implementation locally before broadcasting; sending an RPC does not necessarily execute its receiver on the host.

Run timer state on the server. Remove its repeating callback on every terminal path. Client interpolation is visual only.

Use `EntityID` for server-local target lookup. Use replicated identity only when clients must resolve an entity. Do not treat `EntityID` and `RplId` as interchangeable.

## Completion and Deletion Order

1. Stop the timer and mark terminal state.
2. Apply the authoritative effect.
3. Notify local/listen-host presentation.
4. Broadcast the result.
5. Delete the target when required.
6. Delay proxy deletion long enough for UI result delivery.

For a mine, disarm before deletion. Use the engine entity helper when children must be removed and guard deleted entities.
