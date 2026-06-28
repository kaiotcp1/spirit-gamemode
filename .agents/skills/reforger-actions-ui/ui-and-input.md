# UI and Input

## Context and Widgets

Gameplay UI belongs to the local client. If the player prefab lacks the expected UI manager, use a standalone context that creates the layout through `WorkspaceWidget`. It may require a repeating callqueue update; remove it on close.

Use stable unique widget names and null-check every lookup. Bind buttons directly:

```c
ButtonActionComponent.GetOnAction(button, true).Insert(OnConfirm);
```

Remove the same handler on close:

```c
ButtonActionComponent.GetOnAction(button).Remove(OnConfirm);
```

Do not register handlers each frame or reopen without unregistering.

## Wrong Glyphs and Prohibited Icons

If every button displays the same key or a prohibited icon, the layout likely references unresolved input actions. A mod action config is insufficient unless merged into the active runtime action manager.

Check the layout names, active game configuration, distinct action per button, and glyph lookup. If registration cannot be guaranteed, show explicit labels and use direct widget callbacks.

For controlled keyboard fallback, poll distinct keys, clear each after handling, and synchronize labels. This is not a portable controller solution.

## Modal Controls

Store movement, view, and weapon disabled states independently, disable required controls, and restore the stored values on close, result, cancellation, and forced teardown. Never blindly enable controls; another system may have disabled them first.

## Timer Display

Display replicated server time, optionally with local smoothing. The server decides expiry. Clamp remaining time and handle zero total.

```c
float progress = (remaining / total) * 100.0;
timerBar.SetCurrent(progress);
```
