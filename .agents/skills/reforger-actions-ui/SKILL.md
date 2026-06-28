---
name: reforger-actions-ui
description: Implement and review Arma Reforger EnforceScript interaction actions and gameplay UI. Use for ScriptedUserAction, ActionsManagerComponent, vanilla-prefab proxy actions, layouts, widgets, button/input bindings, modal control locking, timers, RPC validation, replication, and UI lifecycle cleanup.
---

# Reforger Actions and UI

Build interaction flows that work in Workbench, listen-server, dedicated-server, and multiplayer contexts. Treat the action, UI, authoritative state, and cleanup as one lifecycle.

## Read the Relevant Guides

- Read [actions-and-proxies.md](actions-and-proxies.md) when adding an action, discovering world entities, or extending vanilla prefabs.
- Read [ui-and-input.md](ui-and-input.md) when creating layouts, binding buttons or keys, or making a modal screen.
- Read [networking-and-state.md](networking-and-state.md) when the interaction changes gameplay state, uses a timer, or must work in multiplayer.
- Read [lifecycle-and-testing.md](lifecycle-and-testing.md) before finishing implementation or diagnosing Workbench-only behavior.

Read all four for an end-to-end interaction such as MineDefusal.

## Implementation Workflow

1. Inspect the target entity and prefab composition.
2. Decide whether the action belongs on the target prefab or needs a replicated proxy.
3. Define authoritative state transitions and server validation.
4. Keep the `ScriptedUserAction` thin.
5. Open a UI context owned by the local client.
6. Bind widgets explicitly and lock only controls required by the modal.
7. Replicate durable display state and broadcast one-shot results.
8. Unbind callbacks, remove callqueue entries, restore controls, and delete temporary entities.
9. Compile and test in listen-server and client/server scenarios.

## Required Design Rules

- Keep gameplay decisions on the server. Never trust client-provided target, player, index, distance, or result.
- Keep presentation on the client. Do not create UI or lock local controls from server-only code.
- Give replicated interaction entities an `RplComponent`.
- Use `[RplProp()]` for durable client-visible state and `Replication.BumpMe()` after authoritative changes.
- Use reliable RPCs for discrete requests and results; do not use an RPC as the only durable state.
- Put rules in a component or service, not in the user action.
- Preserve movement, view, and weapon-control states before disabling them, then restore those exact values.
- Remove handlers and repeating callqueue callbacks on close, success, failure, cancellation, and deletion.
- Use stable widget names and null-check every widget and component cast.
- Verify widget ranges. Reforger progress bars commonly expect `0..100`, not `0..1`.

## Choosing the Action Architecture

Use a directly authored action when the mod owns the prefab and can add `ActionsManagerComponent`, the custom action, and its gameplay component.

Use a replicated proxy when the target is a vanilla prefab that must remain unmodified or existing world entities need the new action. The server discovers targets, spawns one proxy per target, associates it with the real entity, and cleans stale proxies.

Do not assume script can safely attach a component or action to an instantiated vanilla entity. Prefab composition is the reliable boundary.

## UI Input Decision

Prefer direct widget callbacks for activation:

```c
ButtonActionComponent.GetOnAction(button, true).Insert(OnConfirm);
```

Use registered input actions only when their definitions are known to be loaded by the active game configuration. Missing actions can render every key as one fallback glyph or show a prohibited icon.

Use direct key polling only as a deliberate prototype fallback. Keep labels consistent with actual keys and clear edge-triggered input.

## Completion Checklist

- The action appears once on every intended entity.
- A remote client can start it and the server rejects invalid requests.
- Buttons and displayed keys match actual input.
- Modal controls restore after every exit path.
- Timer text and progress agree with server state.
- Listen-server host receives the result callback.
- Success, failure, and cancellation clean UI, callbacks, queues, proxies, and targets.
- Logs contain no compile errors, null dereferences, missing widgets, or unresolved input actions.

## Reference Implementation

Use MineDefusal as an example, not code to copy blindly:

- `Scripts/Game/GameMode/SPT_MineDefusalManagerComponent.c`
- `Scripts/Game/UserActions/SPT_DefuseMineAction.c`
- `Scripts/Game/Components/SPT_MineDefusalComponent.c`
- `Scripts/Game/UI/SPT_UIContext.c`
- `Scripts/Game/UI/SPT_MineDefusalUIContext.c`
- `Prefabs/MineDefusal/SPT_MineDefusalProxy.et`
- `UI/Layouts/MineDefusal.layout`
