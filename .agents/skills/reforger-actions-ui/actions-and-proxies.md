# Actions and Replicated Proxies

## Directly Authored Actions

For a mod-owned prefab, add `ActionsManagerComponent`, the custom `ScriptedUserAction`, its stateful component, and `RplComponent` when clients interact with it.

Keep `CanBeShownScript` and `CanBePerformedScript` cheap and side-effect free. Revalidate meaningful conditions on the server.

## Extending Vanilla Entities

Do not rely on adding components to an already spawned entity. Instead:

1. Scan on the server for a native identifying component or action.
2. Spawn a small replicated proxy prefab at the target.
3. Put the action manager, custom action, gameplay component, and replication component on the proxy.
4. Store the target `EntityID` for server-side lookup.
5. Maintain one proxy per target and remove stale entries.

Detect behavior or native components before matching prefab text. Names are useful fallback and display data, but fragile type tests.

With `QueryEntitiesBySphere`, return `false` to continue scanning; `true` stops the query. Use `EQueryEntitiesFlags.ALL` when relevant objects may not be dynamic or visible. Repeat discovery at a modest interval when targets can spawn after startup.

The server owns proxy creation, association, and deletion. Clients receive the proxy through replication and invoke its action.

## Thin Action Pattern

The action should resolve user context, verify the gameplay component, request the operation through it, and open local UI. It should not choose outcomes, delete targets, or mutate authoritative state directly.
