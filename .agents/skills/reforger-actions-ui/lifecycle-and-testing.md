# Lifecycle and Testing

## Symmetric Cleanup

Every acquisition needs a release:

| Acquire | Release |
|---|---|
| event `Insert` | event `Remove` |
| repeating `CallLater` | callqueue `Remove` |
| disabled controls | restore stored states |
| temporary proxy | server-side deletion |
| UI component reference | set to null |
| interaction ownership | clear owner/state |

Make close idempotent after cancellation, results, deletion, or delayed callbacks.

## Workbench Verification

1. Open the project in Workbench and compile or enter Play Mode.
2. Inspect current console/log errors.
3. Place vanilla and mod-owned targets when both paths exist.
4. Test as listen-server host.
5. Test a separate client against a server.

Automation accesses project files, generated logs, and available commands; it does not inherently see the Workbench GUI. Verify visual state through Play Mode, screenshots, or available GUI automation.

## Test Matrix

- action appears in range, disappears outside it, and is not duplicated;
- mouse and every displayed key select the matching option;
- cancel does not mutate the target;
- success, failure, and timeout happen once at the correct time;
- modal locks movement, view, and weapons;
- prior control states restore on every exit;
- two players cannot complete one interaction concurrently;
- invalid or late requests are rejected;
- target deletion while UI is open causes no null error;
- callbacks, queues, proxy, UI, and target clean up as designed.

Log state transitions with a feature prefix, position, player, state, and timer. Gate verbose logs behind a debug attribute and never expose secret answers to production clients.
