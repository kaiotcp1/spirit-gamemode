---
name: overthrow-persistence
description: Overthrow-specific persistence patterns using EPF (Enfusion Persistence Framework), SaveData classes, EDF, and platform guards for console compatibility
version: 1.0.0
---

# Overthrow Persistence

Quick reference for Overthrow mod save/load patterns. For EnforceScript persistence fundamentals, see `enforcescript-patterns/persistence.md`.

---

## When to Use This Skill

Use this skill when:
- Adding save/load support to Overthrow components (Managers, Controllers)
- Creating `EPF_ComponentSaveData` classes for component state
- Handling console platform restrictions (Xbox/PlayStation)
- Serializing complex data structures (arrays, maps, nested objects)
- Troubleshooting save/load issues

---

## Quick Reference

### Save Data Pattern
Each component gets a `SaveData` class pair:
1. A `*SaveDataClass` extending `EPF_ComponentSaveDataClass` with `[EPF_ComponentSaveDataType]` attribute
2. A `*SaveData` extending `EPF_ComponentSaveData` with `ReadFrom()` and `ApplyTo()` methods

### Console Platform Handling
Arma Reforger provides two platform defines: `PLATFORM_CONSOLE` and `PLATFORM_WINDOWS`. Console (Xbox + PlayStation) does NOT support disk access — all FileIO and EPF calls must be guarded.

### EDF Dependency
EPF depends on EDF (Enfusion Database Framework) for underlying database operations. Both are part of the Arma 4 engine stack.

---

## Architecture

```
EPF_ComponentSaveDataClass (attribute target)
└── OVT_*SaveDataClass (tag class, usually empty)
    └── EPF_ComponentSaveData (base)
        └── OVT_*SaveData
            ├── Members to persist (int, float, string, arrays, maps)
            ├── ReadFrom(component)  → extracts state from component
            └── ApplyTo(component)   → restores state to component

EPF (Enfusion Persistence Framework)
└── EDF (Enfusion Database Framework) — handles DB operations
```

**Framework locations:**
- EPF: `/mnt/n/Projects/Arma 4/EnfusionPersistenceFramework`
- EDF: `/mnt/n/Projects/Arma 4/EnfusionDatabaseFramework`

---

## Save Data Pattern

```cpp
//! Tag class for EPF type registration.
[EPF_ComponentSaveDataType(OVT_ExampleComponent)]
class OVT_ExampleSaveDataClass : EPF_ComponentSaveDataClass {};

//! Save/load data for OVT_ExampleComponent.
class OVT_ExampleSaveData : EPF_ComponentSaveData
{
    int m_iExampleValue;
    string m_sExampleString;
    ref array<ref OVT_ExampleData> m_aExampleArray;

    //! Extract state from component into save data.
    override void ReadFrom(OVT_ExampleComponent component)
    {
        m_iExampleValue = component.GetExampleValue();
        m_sExampleString = component.GetExampleString();
        m_aExampleArray = component.GetExampleArray();
    }

    //! Restore state from save data into component.
    override void ApplyTo(OVT_ExampleComponent component)
    {
        component.SetExampleValue(m_iExampleValue);
        component.SetExampleString(m_sExampleString);
        component.SetExampleArray(m_aExampleArray);
    }
}
```

### Key Rules

- ✅ Use `ref` for Managed class members (arrays, maps, objects)
- ✅ Store `EntityID` instead of `IEntity` (entities may not exist on load)
- ❌ Never store `IEntity`, `BaseWorld`, or other runtime-only references
- ✅ Initialize arrays/maps in `ReadFrom()` before copying from component
- ⚠️ `ReadFrom` and `ApplyTo` are called on save/load — keep them lightweight

---

## Console Platform Handling

```cpp
// Method 1: Guard entire save data section (recommended)
#ifndef PLATFORM_CONSOLE

[EPF_ComponentSaveDataType(OVT_ExampleComponent)]
class OVT_ExampleSaveDataClass : EPF_ComponentSaveDataClass {};

class OVT_ExampleSaveData : EPF_ComponentSaveData
{
    // ...
};

#endif
```

```cpp
// Method 2: Guard save/load calls at runtime
#ifndef PLATFORM_CONSOLE
    EPF_PersistenceManager.GetInstance().Save(component);
#endif
```

```cpp
// Method 3: Use #ifdef for console-specific fallback
#ifdef PLATFORM_CONSOLE
    // Console path: no persistence, use defaults
#else
    // PC path: full EPF persistence
    EPF_PersistenceManager.GetInstance().Save(component);
#endif
```

> **Note:** Arma Reforger only provides `PLATFORM_CONSOLE` and `PLATFORM_WINDOWS`. There is no separate define for Xbox vs PlayStation.

---

## Complex Data Persistence

### Arrays of Managed Classes

```cpp
class OVT_SomeSaveData : EPF_ComponentSaveData
{
    ref array<ref OVT_SomeData> m_aData;

    override void ReadFrom(OVT_SomeComponent component)
    {
        // ALWAYS re-create the array — avoid sharing references
        m_aData = new array<ref OVT_SomeData>();
        component.GetData(m_aData);
    }

    override void ApplyTo(OVT_SomeComponent component)
    {
        component.SetData(m_aData);
    }
}
```

### Maps

```cpp
class OVT_SomeSaveData : EPF_ComponentSaveData
{
    ref map<int, ref OVT_SomeData> m_mDataById;

    override void ReadFrom(OVT_SomeComponent component)
    {
        m_mDataById = new map<int, ref OVT_SomeData>();
        component.GetDataMap(m_mDataById);
    }

    override void ApplyTo(OVT_SomeComponent component)
    {
        component.SetDataMap(m_mDataById);
    }
}
```

### Entity References (EntityID, not IEntity)

```cpp
class OVT_SomeSaveData : EPF_ComponentSaveData
{
    EntityID m_TargetEntityId;  // ✅ Stable across sessions
    // IEntity m_Target;        // ❌ Would crash on load

    override void ReadFrom(OVT_SomeComponent component)
    {
        m_TargetEntityId = component.GetTargetEntityId();
    }

    override void ApplyTo(OVT_SomeComponent component)
    {
        component.SetTargetEntityId(m_TargetEntityId);
        // Component resolves IEntity from EntityID at runtime
    }
}
```

---

## Best Practices

1. **Always wrap persistence calls** in `#ifndef PLATFORM_CONSOLE` guards
2. **Use strong refs** (`ref`) for arrays, maps, and Managed class instances in SaveData
3. **Implement both `ReadFrom` and `ApplyTo`** — partial implementation causes data loss
4. **No IEntity in SaveData** — use `EntityID` and resolve at runtime
5. **Re-create collections in `ReadFrom`** — use `new array<>()` / `new map<>()` before copying
6. **Test persistence** by saving, restarting, and loading — verify all data is intact
7. **Consider console players** — provide alternative defaults when persistence is disabled

---

## Common Pitfalls

1. **Forgetting platform guards** — crashes on Xbox/PlayStation
2. **Storing IEntity** — entity doesn't exist on load, causes null reference
3. **Weak refs in SaveData** — data gets garbage collected between save and load
4. **Sharing array references** — mutations to the component's array affect the saved data
5. **Missing `[EPF_ComponentSaveDataType]` attribute** — EPF can't find the SaveData class
6. **Not initializing collections** — `null` arrays/maps crash on insert

---

## Resource Files

For comprehensive EnforceScript persistence fundamentals:
- `enforcescript-patterns/persistence.md` — Complete EPF save/load guide

**Pattern:** Use this skill for Overthrow-specific conventions; reference `enforcescript-patterns` for engine-level EPF details.
