---
name: overthrow-networking
description: Overthrow-specific networking patterns, RplProp, RPC communication, JIP handling, and OVT_OverthrowController client-server architecture
version: 1.0.0
---

# Overthrow Networking

Quick reference for Overthrow mod networking patterns. For full EnforceScript replication fundamentals, see `enforcescript-patterns/networking.md`.

---

## When to Use This Skill

Use this skill when:
- Implementing client→server communication via OVT_OverthrowController
- Setting up RplProp replication for Overthrow components
- Designing RPC patterns between server and clients
- Handling Join-In-Progress (JIP) state synchronization
- Working with Overthrow global events (ScriptInvoker on GameMode)
- Migrating from legacy OVT_PlayerCommsComponent to new architecture

---

## Quick Reference

### OVT_OverthrowController Pattern (Client→Server)
The new standard for client-to-server operations. Each player owns an `OVT_OverthrowController` entity with specialized components. The client finds their controller and calls methods on its components, which handle the RPC internally.

**See:** `overthrow-controller.md` in `overthrow-architecture` skill for complete pattern

### RplProp Replication
Simple types (int, float, bool, short strings) auto-sync via `[RplProp]` attribute. Call `Replication.BumpMe()` after server changes. Client callbacks via `onRplName`.

### RPC Communication
- **Server→Broadcast:** Call `RpcDo_*` directly + `Rpc(RpcDo_*, ...)` for all clients
- **Server→Owner:** Use `RplRcver.Owner` for single-player notifications
- **Client→Server:** Via `OVT_OverthrowController` component with `RplRcver.Server`

### JIP (Join-In-Progress)
Override `RplSave()`/`RplLoad()` for complex state (arrays, maps) that new players need.

### Global Events
Use `ScriptInvoker` on GameMode or `OVT_OverthrowController` for cross-system communication. Progress events use `OVT_ProgressEventHandler`.

---

## Critical Constraints

- ❌ **No EntityID across network** - EntityIDs differ between server/client. Use RplId.
- ✅ **Manager components server-only** - Managers exist only on server, notify clients as needed
- ✅ **Owner-based RPC** - Clients must own the entity to make RPC requests to server
- ❌ **Legacy deprecated** - Do NOT use `OVT_PlayerCommsComponent` or `OVT_Global.GetServer()`
- ✅ **Validate on server** - Always validate client-provided data in `RpcAsk_*` methods
- ⚠️ **RplProp limits** - Only simple types, no arrays/maps/entities. Use JIP or RPC for complex data

---

## OVT_OverthrowController Pattern

### Client→Server Request

```cpp
//! Component on OVT_OverthrowController for feature
class OVT_FeatureComponentClass: OVT_ComponentClass {};

class OVT_FeatureComponent: OVT_Component
{
    void RequestOperation(int param)
    {
        if (Replication.IsServer())
        {
            RpcAsk_Operation(param);
        }
        else
        {
            Rpc(RpcAsk_Operation, param);
        }
    }

    [RplRpc(RplChannel.Reliable, RplRcver.Server)]
    protected void RpcAsk_Operation(int param)
    {
        // ✅ ALWAYS VALIDATE CLIENT DATA
        if (param < 0 || param > 100) return;

        ProcessOperation(param);

        // Send result back to owner
        Rpc(RpcDo_OperationResult, true);
    }

    [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
    protected void RpcDo_OperationResult(bool success)
    {
        // Client receives result
        if (success)
        {
            // Handle success on client
        }
    }

    protected void ProcessOperation(int param)
    {
        // Server-side processing
    }
}

// Access in OVT_Global.c:
static OVT_FeatureComponent GetFeature()
{
    OVT_OverthrowController controller = GetController();
    if (!controller) return null;
    return OVT_FeatureComponent.Cast(
        controller.FindComponent(OVT_FeatureComponent)
    );
}

// Client usage:
OVT_FeatureComponent feature = OVT_Global.GetFeature();
if (!feature) return;
feature.RequestOperation(value);
```

### Server→Specific Client

```cpp
void NotifyPlayer(string playerId, string message)
{
    OVT_OverthrowController controller = OVT_Global.GetPlayers().GetController(playerId);
    if (!controller) return; // Player offline

    OVT_NotificationComponent notif = OVT_NotificationComponent.Cast(
        controller.FindComponent(OVT_NotificationComponent)
    );
    if (!notif) return;

    notif.ShowNotification(message);
}

// In OVT_NotificationComponent:
void ShowNotification(string message)
{
    Rpc(RpcDo_ShowNotification, message);
}

[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
void RpcDo_ShowNotification(string message)
{
    // Shows only on the owning client
}
```

---

## RplProp Replication

```cpp
class OVT_ExampleComponent : OVT_Component
{
    [RplProp(onRplName: "OnExampleParamChanged")]
    protected int m_iExampleParam;

    protected void OnExampleParamChanged()
    {
        // Called on clients when m_iExampleParam changes
    }

    void SetExampleParam(int exampleParam)
    {
        m_iExampleParam = exampleParam;
        Replication.BumpMe(); // Broadcast change to all clients
    }
}
```

**Limitations:**
- Only simple types: int, bool, float, short strings
- ❌ NO arrays, maps, entities, or complex classes
- Avoid frequent updates to prevent network flooding

---

## Server→Client Broadcast

```cpp
void NotifyAllClients(float value)
{
    RpcDo_NotifyAllClients(value); // Direct call for host
    Rpc(RpcDo_NotifyAllClients, value); // RPC to all clients
}

[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
void RpcDo_NotifyAllClients(float value)
{
    Print("Server says: " + value);
}
```

---

## Join-In-Progress (JIP)

```cpp
class OVT_TownManagerComponent : OVT_Component
{
    ref array<ref OVT_TownData> m_aTowns;

    override bool RplSave(ScriptBitWriter writer)
    {
        writer.WriteInt(m_aTowns.Count());
        foreach (OVT_TownData town : m_aTowns)
        {
            writer.WriteString(town.name);
            writer.WriteVector(town.location);
            writer.WriteInt(town.population);
        }
        return true;
    }

    override bool RplLoad(ScriptBitReader reader)
    {
        int count;
        reader.ReadInt(count);

        m_aTowns.Clear();
        for (int i = 0; i < count; i++)
        {
            OVT_TownData town = new OVT_TownData();
            reader.ReadString(town.name);
            reader.ReadVector(town.location);
            reader.ReadInt(town.population);
            m_aTowns.Insert(town);
        }
        return true;
    }
}
```

---

## Global Events

For client-specific events, add `ScriptInvoker` to `OVT_OverthrowController`:

```cpp
class OVT_OverthrowController : GenericEntity
{
    ref OVT_ProgressEventHandler m_OnProgress = new OVT_ProgressEventHandler();
}

// Usage:
controller.m_OnProgress.GetInvoker().Invoke(0.5, "Loading...");
```

---

## Best Practices

### 1. Minimize Network Traffic
- Use RplProp for simple, infrequent updates
- Batch multiple changes into single RPC calls
- Use reliable channel only when necessary

### 2. Validate on Server
```cpp
[RplRpc(RplChannel.Reliable, RplRcver.Server)]
void RpcAsk_ClaimBase(RplId baseId)
{
    IEntity baseEntity = Replication.FindItem(baseId);
    if (!baseEntity) return;

    OVT_BaseControllerComponent base = OVT_BaseControllerComponent.Cast(
        baseEntity.FindComponent(OVT_BaseControllerComponent)
    );
    if (!base) return;
    if (!CanPlayerClaimBase(GetPlayerId(), base)) return;

    base.SetOwner(GetPlayerId());
}
```

### 3. Handle Disconnections
- Always check if entities/players still exist
- Clean up references when players disconnect
- Use weak references where appropriate

### 4. Use RplId for Network Entity References
```cpp
RplId m_TargetId;

IEntity GetTarget()
{
    if (!m_TargetId.IsValid()) return null;
    return RplComponent.Cast(Replication.FindItem(m_TargetId)).GetEntity();
}
```

---

## Common Pitfalls

1. **Using EntityID across network** - Will cause mismatches. Use RplId.
2. **Replicating complex objects** - Use JIP or manual serialization
3. **Too frequent updates** - Causes network congestion
4. **Not checking IsServer()** - Causes unnecessary RPC calls on host
5. **Missing null checks** - Entities can be deleted anytime

---

## Legacy System (Deprecated)

The old `OVT_PlayerCommsComponent` system is deprecated. Do not use `OVT_Global.GetServer()` anymore. All new client-to-server communication should use components on `OVT_OverthrowController`.

---

## Resource Files

For comprehensive EnforceScript networking fundamentals (RplProp details, RPC channel types, JIP serialization, memory considerations), see:
- `enforcescript-patterns/networking.md` - Complete EnforceScript replication guide

**Pattern:** Start here for Overthrow-specific patterns, reference `enforcescript-patterns` for engine-level fundamentals.
