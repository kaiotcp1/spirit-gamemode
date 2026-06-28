# SPT_GAME — Prioridades de Implementação

> Documento gerado a partir da análise do código Freedom Fighters (JWK_) e do estado atual do SPT_GAME.
> Última atualização: 2026-06-28

---

## 🎯 Objetivo Geral

Transformar o SPT_GAME em um modo de jogo **dinâmico e finito**, onde:

1. Tudo é configurável via **Componentes com Attributes** (padrão atual)
2. **Array de group prefabs** para fácil seleção de grupos de mods
3. **Spawn/despawn com histerese** (sem flickering)
4. **Caching de unidades** — grupos despawnados preservam estado (quem morreu continua morto)
5. **Orçamento finito** — cada localização tem um limite de tropas, o jogo tem fim

---

## 📊 Estado Atual vs Desejado

| Aspecto | Atual (SPT) | Desejado | Inspiração JWK |
|---------|-------------|----------|----------------|
| **Spawn** | Síncrono no `UpdateLocations()` | Assíncrono (1 por tick) | `JWK_SpawnQueueSystem.PollQueue_S()` |
| **Despawn** | `DeleteEntityAndChildren()` — perde estado | Stream out: salva membros vivos | `JWK_StreamableAIGroup.DoStreamOut()` |
| **Respawn** | Grupo fresco (todos vivos) | Stream in: recria só os vivos | `JWK_StreamableAIForce.DoStreamIn()` |
| **Histerese** | ❌ Fixo 800/1000m | ✅ Threshold spread (±100m) | `CheckStreamStateConditions()` |
| **Orçamento** | ❌ Infinito | ✅ Budget por localização | `JWK_BaseBattleAIGenerator` + `JWK_Battle` |
| **Eventos** | ❌ Nenhum | Eventos de força (onGroupSpawned, onAgentKilled) | `JWK_AIForceSystem` (16 eventos) |

---

## 🔢 Fases de Implementação

### FASE 1 — Histerese (Anti-Flickering) ⭐

**Problema:** Jogador a exatamente 800m de uma cidade causa spawn/despawn frenético.

**Solução:** Adicionar margem de histerese no `SPT_WorldGarrisonManagerComponent`.

**Onde mexer:** `Scripts/Game/GameMode/SPT_WorldGarrisonManagerComponent.c`

```cpp
// Novo Attribute:
[Attribute("100", desc: "Margem extra antes de despawnar uma localização já ativa.")]
protected float m_fDespawnHysteresis;

// Em UpdateLocations():
float effectiveDespawn = location.m_bActive
    ? m_fDespawnDistance + m_fDespawnHysteresis
    : m_fDespawnDistance;

if (nearestDist < m_fSpawnDistance && !location.m_bActive)
    SpawnLocation(location);
else if (nearestDist > effectiveDespawn && location.m_bActive)
    DespawnLocation(location);
```

**Arquivo referência JWK:** `teste/freedom/AIForce/STREAMEBLE_AI_FORCE.c:301-330`

| Esforço | Impacto |
|---------|---------|
| ~10 linhas, 15 min | ⭐⭐⭐⭐⭐ Imediato |

---

### FASE 2 — Caching de Unidades (Stream Out/In) ⭐⭐⭐⭐⭐

**Problema:** Hoje o jogo é infinito — quando jogador volta pra uma cidade, todos os inimigos respawnam com vida cheia. Não há progresso.

**Solução:** Criar `SPT_StreamableGarrisonGroup` — wrapper que salva estado do grupo ao despawnar.

**Novo arquivo:** `Scripts/Game/AI/Garrison/SPT_StreamableGarrisonGroup.c`

```cpp
class SPT_StreamableGarrisonGroup
{
    ResourceName m_rGroupPrefab;            // Prefab original do grupo
    ref array<ResourceName> m_aAliveMembers; // Prefabs dos membros AINDA VIVOS
    vector m_vPosition;                      // Última posição conhecida
    EntityID m_GroupID;                      // ID quando spawnado, INVALID quando cached
    bool m_bIsCQB;                           // true = CQB, false = patrol

    // Quando jogador sai do raio:
    //   1. Salva m_aAliveMembers = GetGroupAliveMembersPrefabs()
    //   2. Seta m_GroupID = EntityID.INVALID
    //   3. Deleta entidade
    void DoStreamOut();

    // Quando jogador entra no raio:
    //   1. Usa SCR_AIGroup.JWK_OverrideGroupMembers(m_aAliveMembers) pra spawnar só vivos
    //   2. Seta m_GroupID = novo ID
    //   3. Se m_aAliveMembers estiver vazio, não spawna (grupo foi eliminado)
    bool DoStreamIn();
}
```

**Onde mexer:** `SPT_WorldGarrisonManagerComponent.c` — alterar `SpawnLocation()` e `DespawnLocation()`.

**Arquivos referência JWK:**
- `teste/freedom/AIForce/STREAMEBLE_AI_GROUP.c` (wrapper)
- `teste/freedom/AIForce/STREAMEBLE_AI_FORCE.c:332-393` (DoStreamIn / DoStreamOut)
- `teste/freedom/AIForce/JWK_SCR_AIGroup.c:30-50` (OverrideGroupMembers)
- `teste/freedom/JWK_AIUtils.c:259-278` (GetGroupAliveMembersPrefabs)

| Esforço | Impacto |
|---------|---------|
| ~150 linhas, 1-2h | ⭐⭐⭐⭐⭐ Transforma o jogo |

---

### FASE 3 — Orçamento por Localização ⭐⭐⭐⭐

**Problema:** Cada localização spawna um número fixo de grupos. Tropas são infinitas.

**Solução:** Adicionar budget por `SPT_GarrisonLocation`. Quando tropas morrem, o budget é consumido. Quando zera, a localização fica "limpa".

**Onde mexer:** `Scripts/Game/GameMode/SPT_WorldGarrisonManagerComponent.c` (classe `SPT_GarrisonLocation`)

```cpp
class SPT_GarrisonLocation
{
    // ... membros existentes ...

    int m_iMaxBudget;          // Budget máximo (definido por Attribute)
    int m_iBudgetRemaining;    // Budget restante
    int m_iTotalManpower;      // Vivos + cached + spawnando
}
```

**Novo Attribute no WorldGarrisonManager:**
```cpp
[Attribute("50", desc: "Budget máximo de tropas por localização. 0 = infinito.")]
protected int m_iLocationBudget;
```

**Regras:**
- Cada localização recebe `m_iLocationBudget` de budget
- `Cada personagem = 1 de budget`
- Quando morre → `m_iBudgetRemaining -= 1`
- Spawn só acontece se `m_iBudgetRemaining > 0`
- Quando zera → localização marcada como `m_bCleared`

**Arquivos referência JWK:**
- `teste/freedom/BASER_BATTLE_AI_REGENERATOR.c:60-96` (InitBudget, ConsumeBudget)
- `teste/freedom/JWK_Battle.c:511-556` (AllocateEnemyBudget_S — scaling por jogador)
- `teste/freedom/AIForce/JWK_AIForce.c:86-94` (GetTotalManpower)

| Esforço | Impacto |
|---------|---------|
| ~80 linhas, 1h | ⭐⭐⭐⭐ Torna o jogo finito |

---

### FASE 4 — Spawn Assíncrono (Tick-Based) ⭐⭐⭐

**Problema:** `UpdateLocations()` spawna TODOS os grupos de uma localização no mesmo frame. Cidades grandes causam lag spike.

**Solução:** Fila interna que processa 1 spawn a cada N milissegundos via `CallLater`.

**Onde mexer:** `SPT_WorldGarrisonManagerComponent.c`

```cpp
// Novos membros:
protected ref array<ref SPT_GarrisonSpawnRequest> m_aPendingSpawns;
protected bool m_bProcessingSpawnQueue;

// Novo método:
void EnqueueSpawn(SPT_GarrisonLocation location, ResourceName prefab, vector pos, bool isCQB)
{
    SPT_GarrisonSpawnRequest req = new SPT_GarrisonSpawnRequest();
    req.m_Location = location;
    req.m_rPrefab = prefab;
    req.m_vPosition = pos;
    req.m_bIsCQB = isCQB;
    m_aPendingSpawns.Insert(req);

    if (!m_bProcessingSpawnQueue)
    {
        m_bProcessingSpawnQueue = true;
        GetGame().GetCallqueue().CallLater(ProcessSpawnQueue, m_iSpawnIntervalMs);
    }
}

void ProcessSpawnQueue()
{
    if (m_aPendingSpawns.IsEmpty())
    {
        m_bProcessingSpawnQueue = false;
        return;
    }

    SPT_GarrisonSpawnRequest req = m_aPendingSpawns[0];
    m_aPendingSpawns.Remove(0);

    DoSpawnSingle(req);  // Um único spawn

    GetGame().GetCallqueue().CallLater(ProcessSpawnQueue, m_iSpawnIntervalMs);
}
```

**Arquivo referência JWK:** `teste/freedom/JWK_SpawnQueuSystem.c:123-214` (PollQueue_S)

| Esforço | Impacto |
|---------|---------|
| ~60 linhas, 30 min | ⭐⭐⭐ Performance |

---

### FASE 5 — Sistema de Eventos ⭐⭐

**Problema:** Outros sistemas não sabem quando grupos são spawnados/mortos.

**Solução:** Adicionar `ScriptInvoker` para eventos de força.

```cpp
// No SPT_WorldGarrisonManagerComponent:
protected ref ScriptInvoker m_OnGroupSpawned;   // (SPT_GarrisonLocation, SCR_AIGroup)
protected ref ScriptInvoker m_OnGroupDespawned; // (SPT_GarrisonLocation, EntityID)
protected ref ScriptInvoker m_OnAgentKilled;     // (SPT_GarrisonLocation, int remainingBudget)
protected ref ScriptInvoker m_OnLocationCleared; // (SPT_GarrisonLocation)
```

**Arquivo referência JWK:** `teste/freedom/AIForce/JWK_AIForceSystem.c:21-103` (16 eventos)

| Esforço | Impacto |
|---------|---------|
| ~50 linhas, 30 min | ⭐⭐ Infraestrutura |

---

### FASE 6 — Persistência (Save/Load) ⭐

**Problema:** Ao carregar um save, todas as localizações resetam.

**Solução:** Salvar estado dos `SPT_StreamableGarrisonGroup` via EPF.

**Arquivos referência JWK:**
- `teste/freedom/AIForce/JWK_AIForce.c:681-725` (SaveState_S / LoadState_S)
- `teste/freedom/AIForce/JWK_AIForceComponentSaveData.c` (EPF SaveData)

| Esforço | Impacto |
|---------|---------|
| ~100 linhas, 1h | ⭐ Qualidade de vida |

---

## 📁 Arquivos do Projeto

### Existentes (a modificar)

| Arquivo | Fases |
|---------|-------|
| `Scripts/Game/GameMode/SPT_WorldGarrisonManagerComponent.c` | 1, 2, 3, 4, 5, 6 |
| `Scripts/Game/AI/Garrison/SPT_GarrisonManager.c` | 2 (referência) |
| `Scripts/Game/GameMode/SPT_Global.c` | 5 (accessors) |

### Novos (a criar)

| Arquivo | Fase |
|---------|------|
| `Scripts/Game/AI/Garrison/SPT_StreamableGarrisonGroup.c` | 2 |
| `Scripts/Game/GameMode/SPT_GarrisonSpawnRequest.c` | 4 |

---

## 🔗 Referências — Padrões JWK por Feature

| Feature SPT | Classe/Método JWK | Arquivo |
|-------------|-------------------|---------|
| **Histerese** | `CheckStreamStateConditions(streamDistance, out in, out out)` | `AIForce/STREAMEBLE_AI_FORCE.c:301` |
| **Stream Out** | `JWK_StreamableAIGroup.DoStreamOut()` — salva prefabs, deleta entidade | `AIForce/STREAMEBLE_AI_GROUP.c:106` |
| **Stream In** | `JWK_StreamableAIForce.DoStreamIn()` — enfileira spawn requests | `AIForce/STREAMEBLE_AI_FORCE.c:332` |
| **Override membros** | `SCR_AIGroup.JWK_OverrideGroupMembers(array)` | `JWK_SCR_AIGroup.c:30` |
| **Prefabs dos vivos** | `JWK_AIUtils.GetGroupAliveMembersPrefabs(group, out)` | `JWK_AIUtils.c:259` |
| **Contagem de vivos** | `JWK_AIUtils.CountAliveCharactersInGroup(group)` | `JWK_AIUtils.c:219` |
| **Agent está vivo?** | `JWK_AIUtils.IsAgentCharacterAlive(agent)` | `JWK_AIUtils.c:231` |
| **Spawn Queue** | `JWK_SpawnQueueSystem.PollQueue_S()` | `JWK_SpawnQueuSystem.c:138` |
| **Budget** | `JWK_BaseBattleAIGenerator.InitBudget() / ConsumeBudget()` | `BASER_BATTLE_AI_REGENERATOR.c:60` |
| **Budget scaling** | `JWK_Battle.AllocateEnemyBudget_S()` | `JWK_Battle.c:511` |
| **Manpower total** | `JWK_AIForce.GetTotalManpower()` = agents + fila | `AIForce/JWK_AIForce.c:91` |
| **Config por Attributes** | `[BaseContainerProps(configRoot: true)]` + `JWK_AIManagerConfig` | `JWK_AIManagerConfig.c` |
| **Spawn seguro** | `JWK_SpawnUtils.FindSafeCharacterSpawnPositionOutdoor(pos)` | `JWK_SpawnUtils.c:185` |
| **LoS check** | `JWK_SpawnUtils.IsSpawnPositionVisible(ctx, pos)` | `JWK_SpawnUtils.c:336` |

---

## ⚠️ Nota sobre Licença

O código `teste/freedom/` está sob licença **APL-ND (No Derivatives)**:
> "If you remix, transform, or build upon the material, you may not distribute the modified material."

**Isso significa:**
- ✅ Use como **referência de arquitetura e padrões**
- ✅ **Implemente do zero** no padrão `SPT_`
- ❌ **Não copie/cole** trechos de código JWK literalmente
- ❌ **Não distribua** código JWK modificado

---

## 📈 Progresso

| Fase | Status | Data |
|------|--------|------|
| 1. Histerese | ✅ Concluído | 2026-06-28 |
| 2. Caching | ⬜ Pendente | — |
| 3. Orçamento | ⬜ Pendente | — |
| 4. Spawn Assíncrono | ⬜ Pendente | — |
| 5. Eventos | ⬜ Pendente | — |
| 6. Persistência | ⬜ Pendente | — |
