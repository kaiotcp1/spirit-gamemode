# Warfare missions

## Configuring a point

On an `SPT_WarfarePointComponent`, open the `Mission` category:

1. Enable the mission.
2. Select `DESTROY_AMMO_CACHE`.
3. Select a scenario prefab.
4. Tune activation distance, terrain search radius, and maximum slope.

The point's existing unique `PointId` is also the mission identity. Reusing an
ID remains a configuration error.

The included first scenario is
`Prefabs/Warfare/Missions/SPT_DestroyAmmoCacheScenario.et`. It combines the
large USSR ammunition-box stacks with a military HQ tent. Additional
compositions can implement the same contract without changing the manager.

## Scenario prefab contract

The selected root prefab must contain:

- `RplComponent` on the root;
- `SPT_WarfareMissionScenarioComponent` on the root;
- `SPT_WarfareMissionObjectiveComponent` on the root;
- for `DESTROY_AMMO_CACHE`, one `SPT_AmmoCacheDestructionComponent` on the root;
- one `ActionsManagerComponent` with `SPT_DestroyAmmoCacheAction` on the root;
- `SPT_WarfareMissionPlacementComponent` on every child that should be
  individually snapped to the ground.

The objective may also have a placement component. Set its vertical offset to
the amount required by the model pivot. Buildings and tents should normally
leave `Align To Slope` disabled; small crates and clutter may enable it.

The manager rejects malformed prefabs and logs the affected `PointId` instead
of leaving a partially functional mission.

## Multiplayer and persistence

Mission state is server-authoritative. The root prefab is spawned only by the
server and its replication component creates it for clients. The Ammo Cache
timer and terminal state are replicated properties, so join-in-progress clients
cannot start a second timer. Player requests travel through the owned
PlayerController and are revalidated for distance and active mission state on
the server.

Completed IDs are stored by EPF in the `SPT_GAME.Warfare` database. Active
missions are intentionally not persisted as active: after a restart they
return to `INACTIVE`, while completed missions never respawn.

EPF and EDF are required addon dependencies. Disk persistence is invoked only
outside `PLATFORM_CONSOLE`; console clients still receive normal replicated
session state.
