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
    vector m_vPosition;                      // Centro real dos sobreviventes no stream-out
    bool m_bIsCQB;                           // true = CQB, false = patrol

    int CaptureFromGroup(SCR_AIGroup group, ...); // Captura prefabs e centro dos vivos
    int GetCachedManpower();                      // Quantidade restaurável
}
```

O wrapper representa somente o estado cacheado. O manager mantém os `EntityID`
dos grupos ativos, executa stream-out/in e preserva entradas que falharem durante
uma restauração para nova tentativa.

**Onde mexer:** `SPT_WorldGarrisonManagerComponent.c` — alterar `SpawnLocation()` e `DespawnLocation()`.

**Arquivos referência JWK:**
- `teste/freedom/AIForce/STREAMEBLE_AI_GROUP.c` (wrapper)
- `teste/freedom/AIForce/STREAMEBLE_AI_FORCE.c:332-393` (DoStreamIn / DoStreamOut)
- `teste/freedom/AIForce/JWK_SCR_AIGroup.c:30-50` (OverrideGroupMembers)
- `teste/freedom/JWK_AIUtils.c:259-278` (GetGroupAliveMembersPrefabs)

| Esforço | Impacto |
|---------|---------|
| ~150 linhas, 1-2h | ⭐⭐⭐⭐⭐ Transforma o jogo |

**Validação manual concluída em 2026-06-28:**
- Cache parcial restaurou exatamente os sobreviventes (`3 grupos / 15 unidades`)
- Oito grupos eliminados permaneceram eliminados após stream out/in
- Local completamente eliminado permaneceu limpo e não gerou uma nova guarnição

---

### FASE 3 — Orçamento por Localização ⭐⭐⭐⭐

**Problema:** O caching preserva baixas, mas cada localização ainda não possui uma
reserva configurável para limitar o total de tropas que poderá mobilizar ao longo
da partida.

**Solução:** Adicionar budget de mobilização por `SPT_GarrisonLocation`. O budget
é consumido quando uma nova unidade é colocada em campo, seguindo o padrão JWK
`ConsumeBudget()`; uma morte não desconta novamente uma unidade que já foi paga.

**Onde mexer:** `Scripts/Game/GameMode/SPT_WorldGarrisonManagerComponent.c` (classe `SPT_GarrisonLocation`)

```cpp
class SPT_GarrisonLocation
{
    // ... membros existentes ...

    int m_iMaxBudget;          // Budget máximo (definido por Attribute)
    int m_iBudgetRemaining;    // Reserva ainda não mobilizada
    int m_iTargetManpower;     // Tamanho desejado da guarnição ativa
    bool m_bCleared;           // Sem manpower e sem reserva

    int GetTotalManpower();    // Derivado: vivos + cached + spawnando
}
```

**Novo Attribute no WorldGarrisonManager:**
```cpp
[Attribute("50", desc: "Budget máximo de tropas por localização. 0 = infinito.")]
protected int m_iLocationBudget;
```

**Regras:**
- Cada localização recebe `m_iLocationBudget` de budget
- Cada personagem mobilizado pela primeira vez custa `1` de budget
- O spawn inicial consome budget até `m_iTargetManpower` ou até a reserva zerar
- Restaurar um sobrevivente do cache não consome budget novamente
- Mortes reduzem o manpower, mas não descontam novamente o budget
- Em um stream-in posterior, a localização pode preencher a diferença até
  `m_iTargetManpower`, consumindo a reserva restante
- `GetTotalManpower()` deve ser calculado, não armazenado, para evitar estado obsoleto
- Quando `GetTotalManpower() == 0` e `m_iBudgetRemaining == 0`, marcar `m_bCleared`
- `m_iLocationBudget == 0` mantém o comportamento ilimitado explicitamente configurado

**Arquivos referência JWK:**
- `teste/freedom/BASER_BATTLE_AI_REGENERATOR.c:60-96` (InitBudget, ConsumeBudget)
- `teste/freedom/JWK_Battle.c:511-556` (AllocateEnemyBudget_S — scaling por jogador)
- `teste/freedom/AIForce/JWK_AIForce.c:86-94` (GetTotalManpower)

| Esforço | Impacto |
|---------|---------|
| ~80 linhas, 1h | ⭐⭐⭐⭐ Torna o jogo finito |

**Implementação:**
- Budget inicializado individualmente para cada `SPT_GarrisonLocation`
- Spawn inicial limitado pelo budget, mantendo o manpower-alvo original
- Sobreviventes do cache restaurados sem novo custo
- Déficit preenchido por reforços enquanto houver reserva
- `GetTotalManpower()` derivado de agentes ativos, filas de grupo e cache
- Local marcado como limpo somente quando manpower e reserva chegam a zero

**Validação manual esperada com budget 50 e alvo 31:**
1. Spawn inicial: `manpower=31/31`, `budgetRestante=19`
2. Após perder 16 e fazer stream-out/in: restaurar 15, mobilizar 16 reforços,
   terminar novamente em `31/31`, `budgetRestante=3`
3. Após eliminar os 31 e retornar: mobilizar somente 3 reforços,
   `budgetRestante=0`
4. Após eliminar os últimos 3 e fazer stream-out: `manpower=0`,
   `budgetRestante=0`, `limpo=1`

**Validação manual concluída em 2026-06-28:**
- Spawn inicial mobilizou `31/31` unidades e preservou `19` de reserva
- Quatro sobreviventes foram restaurados sem custo e os `19` reforços restantes
  foram mobilizados, resultando em `23/31` e `budgetRestante=0`
- Um novo ciclo restaurou as `23` unidades sem criar reforços
- Após a eliminação total, o local registrou `manpower=0`,
  `budgetRestante=0`, `limpo=1` e não voltou a gerar tropas

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

## 📈 Progresso

| Fase | Status | Data |
|------|--------|------|
| 1. Histerese | ✅ Concluído | 2026-06-28 |
| 2. Caching | ✅ Concluído | 2026-06-28 |
| 3. Orçamento | ✅ Concluído | 2026-06-28 |
| 4. Spawn Assíncrono | ⬜ Pendente | — |
| 5. Eventos | ⬜ Pendente | — |
| 6. Persistência | ⬜ Pendente | — |
