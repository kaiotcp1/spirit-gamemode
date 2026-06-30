enum SPT_EGarrisonFaction
{
	OPFOR,
	BLUFOR,
	INDEPENDENT
}

//! Rastreia metadados de um grupo de guarnicao spawnado para que o stream-out
//! possa salvar o prefab e a posicao corretos de volta no cache.
class SPT_GroupSpawnRecord : Managed
{
	//! Prefab que foi usado para spawnar este grupo (de m_aCQBGroupPrefabs ou m_aPatrolGroupPrefabs).
	ResourceName m_rGroupPrefab;

	//! Posicao de fallback usada somente quando nenhuma posicao de membro sobrevivente esta disponivel.
	vector m_vFallbackPosition;

	//! Verdadeiro se o grupo foi spawnado como grupo interior CQB, falso para grupo de patrulha.
	bool m_bIsCQB;
	bool m_bIsBattleGroup;
	ResourceName m_rVehiclePrefab;

}

//! Item enfileirado para spawn assincrono de um unico grupo de guarnicao.
//! A fila processa um item por tick, distribuindo o custo de spawn ao longo
//! de varios quadros e evitando picos de lag em cidades grandes.
class SPT_GarrisonSpawnRequest : Managed
{
	//! Localizacao dona deste spawn.
	SPT_GarrisonLocation m_Location;

	//! Prefab do grupo (SCR_AIGroup) a ser instanciado.
	ResourceName m_rGroupPrefab;

	//! Centro de spawn no mundo.
	vector m_vSpawnPosition;

	//! Centro da cidade, usado para roteamento de patrulha.
	vector m_vCityCenter;

	//! Verdadeiro se for grupo CQB de interior, falso se for patrulha.
	bool m_bIsCQB;

	//! Verdadeiro se este spawn vem de um cache (stream-in) e usa
	//! SpawnGroupWithMembers em vez de SpawnGroup.
	bool m_bIsFromCache;

	//! Lista de prefabs dos membros vivos (apenas para cache restore).
	ref array<ResourceName> m_aAliveMembers;

	//! Limite de unidades mobilizaveis por budget (-1 = ilimitado).
	int m_iMaxDeployableUnits;

	//! Geracao da fila no momento em que a requisicao foi criada. Requisicoes
	//! antigas sao descartadas depois de stream-out/cancelamento.
	int m_iGeneration;

	//! Quantidade estimada reservada na fila para contabilizar manpower.
	int m_iExpectedUnits;
	bool m_bIsBattleSpawn;
	SPT_BattleWave m_BattleWave;

	void SPT_GarrisonSpawnRequest()
	{
		m_aAliveMembers = new array<ResourceName>();
		m_iMaxDeployableUnits = -1;
	}
}

//! Candidato bruto coletado de um MapDescriptor antes da deduplicacao.
class SPT_GarrisonDescriptorCandidate : Managed
{
	vector m_vCenter;
	string m_sName;
	string m_sLocationId;
	int m_iDescriptorType;
	int m_iPriority;
}

class SPT_GarrisonLocation : Managed
{
	//! Identificador estavel e unico derivado do descritor do mapa.
	//! Usado pelo sistema Warfare para referenciar pontos de forma consistente.
	string m_sLocationId;
	vector m_vCenter;
	string m_sName;
	int m_iDescriptorType;
	bool m_bActive;
	bool m_bSpawning;
	bool m_bUnavailable;
	//! Localizacao protegida por scripts externos; nunca recebe IA hostil.
	bool m_bSafe;
	//! Diferencia "nunca sofreu stream-out" de um cache intencionalmente vazio
	//! apos todos os grupos desta localizacao terem sido eliminados.
	bool m_bHasCachedSnapshot;
	bool m_bCleared;
	//! Verdadeiro quando o Warfare solicitou monitoramento de primeira
	//! baixa nesta localizacao. Desacoplado de m_bActive para evitar que
	//! locais LOCKED disparem eventos de baixa prematuramente.
	bool m_bMonitoringEnabled;
	//! Verdadeiro quando a primeira baixa da guarnicao foi detectada.
	//! Usado pelo Warfare para disparar reforcos uma unica vez.
	bool m_bFirstCasualtyTriggered;
	int m_iPendingGroups;
	int m_iSuccessfulGroups;
	int m_iDesiredUnits;
	int m_iMaxBudget;
	int m_iBudgetRemaining;
	int m_iTargetManpower;
	//! Snapshot do manpower total da guarnicao no momento em que o deploy
	//! inicial termina. Usado para detectar a primeira baixa.
	int m_iInitialGarrisonManpower;
	int m_iSpawnGeneration;
	int m_iQueuedSpawnUnits;
	int m_iQueuedGarrisonUnits;
	ref array<EntityID> m_aGroupIds = new array<EntityID>();

	//! Estado dos grupos salvos durante o stream-out para que apenas sobreviventes
	//! sejam respawnados quando o jogador retorna. Um array vazio junto com
	//! m_bHasCachedSnapshot significa que a localizacao foi completamente eliminada.
	ref array<ref SPT_StreamableGarrisonGroup> m_aCachedGroups = new array<ref SPT_StreamableGarrisonGroup>();

	//! Array paralelo ao m_aGroupIds que rastreia metadados de spawn de cada grupo.
	//! Usado pelo stream-out (caching) para saber qual prefab e posicao salvar.
	ref array<ref SPT_GroupSpawnRecord> m_aGroupRecords = new array<ref SPT_GroupSpawnRecord>();
	ref SPT_LocationBattleState m_Battle = new SPT_LocationBattleState();
	ref SPT_GarrisonLocationConfig m_Config;

	void SPT_GarrisonLocation(vector center, string name, int descriptorType, string locationId = "")
	{
		m_vCenter = center;
		m_sName = name;
		m_iDescriptorType = descriptorType;
		m_sLocationId = locationId;
		if (m_sLocationId.IsEmpty())
			m_sLocationId = GenerateLocationId(name, descriptorType, center);
	}

	//! Gera um ID estavel a partir do nome e tipo do descritor.
	//! Remove caracteres especiais para criar um identificador limpo.
	static string GenerateLocationId(string name, int descriptorType, vector center = vector.Zero)
	{
		string clean = name;
		clean.Replace("#", "");
		clean.Replace("-", "_");
		clean.Replace(".", "");
		clean.Replace(" ", "_");
		if (clean.IsEmpty())
			clean = string.Format("LOC_%1", descriptorType);
		int quantizedX = Math.Round(center[0]);
		int quantizedZ = Math.Round(center[2]);
		return string.Format("%1_%2_%3_%4", clean, descriptorType, quantizedX, quantizedZ);
	}

	//! Captura o manpower atual como referencia inicial para deteccao de
	//! primeira baixa. Chamado pelo monitor de estado da guarnicao.
	void SnapInitialManpower()
	{
		m_iInitialGarrisonManpower = GetGarrisonManpower();
		m_bFirstCasualtyTriggered = false;
	}

	//! Manpower combinado de todos os grupos de guarnicao (CQB + patrulha),
	//! excluindo grupos de batalha e cache pendente.
	int GetGarrisonManpower()
	{
		return GetCachedGarrisonManpower() + GetActiveGarrisonManpower() + m_iQueuedGarrisonUnits;
	}

	//! Verifica se a primeira baixa ocorreu e retorna verdadeiro apenas na
	//! transicao (one-shot). O caller deve agir imediatamente.
	bool CheckFirstCasualty()
	{
		if (m_bFirstCasualtyTriggered)
			return false;

		int current = GetGarrisonManpower();
		if (current < m_iInitialGarrisonManpower)
		{
			m_bFirstCasualtyTriggered = true;
			return true;
		}
		return false;
	}

	//! Total de manpower entre os grupos cacheados (ainda nao respawnados).
	int GetCachedManpower()
	{
		int total;
		foreach (SPT_StreamableGarrisonGroup cached : m_aCachedGroups)
		{
			total = total + cached.GetCachedManpower();
		}
		return total;
	}

	//! Inicializa a reserva finita de reforcos para esta localizacao.
	//! Budget zero significa tropas ilimitadas.
	void InitBudget(int maxBudget)
	{
		m_iMaxBudget = maxBudget;
		m_iBudgetRemaining = maxBudget;
		m_bCleared = false;
	}

	bool HasUnlimitedBudget()
	{
		return m_iMaxBudget == 0;
	}

	bool CanDeployUnits()
	{
		return HasUnlimitedBudget() || m_iBudgetRemaining > 0;
	}

	int GetDeploymentLimit(int requestedUnits)
	{
		if (requestedUnits < 1)
			return 0;

		if (HasUnlimitedBudget())
			return requestedUnits;

		return Math.Min(requestedUnits, m_iBudgetRemaining);
	}

	void ConsumeBudget(int deployedUnits)
	{
		if (deployedUnits < 1 || HasUnlimitedBudget())
			return;

		m_iBudgetRemaining = Math.Max(0, m_iBudgetRemaining - deployedUnits);
	}

	//! Forca total representada por agentes vivos, filas de spawn dos grupos,
	//! requisicoes do manager e cache.
	int GetTotalManpower()
	{
		int total = GetCachedManpower() + m_iQueuedSpawnUnits;
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return total;

		foreach (EntityID groupId : m_aGroupIds)
		{
			SCR_AIGroup group = SCR_AIGroup.Cast(world.FindEntityByID(groupId));
			if (!group)
				continue;

			array<AIAgent> agents = {};
			group.GetAgents(agents);
			foreach (AIAgent agent : agents)
			{
				if (!agent)
					continue;

				IEntity character = agent.GetControlledEntity();
				if (!character)
					continue;

				SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(
					character.FindComponent(SCR_CharacterControllerComponent));
				if (controller && controller.GetLifeState() < ECharacterLifeState.DEAD)
					total++;
			}
			total = total + group.GetSpawnQueueSize();
		}

		return total;
	}

	void RefreshClearedState()
	{
		// Para localizacoes ativas sem cache, verifica a guarnicao ativa diretamente.
		// O budget restante (reserva de batalha) nao impede que a guarnicao local
		// seja considerada completamente eliminada.
		if (m_bHasCachedSnapshot)
		{
			int garrisonManpower = GetCachedGarrisonManpower() + GetActiveGarrisonManpower();
			m_bCleared = garrisonManpower < 1 && GetQueuedGarrisonRequests() < 1;
			return;
		}

		// Localizacao ativa sem cache: verifica apenas grupos ativos
		m_bCleared = GetActiveGarrisonManpower() < 1 && GetQueuedGarrisonRequests() < 1;
	}

	int GetCachedGarrisonManpower()
	{
		int total;
		foreach (SPT_StreamableGarrisonGroup cached : m_aCachedGroups)
		{
			if (!cached.m_bIsBattleGroup)
				total += cached.GetCachedManpower();
		}
		return total;
	}

	int GetActiveGarrisonManpower()
	{
		int total;
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return 0;

		foreach (int i, EntityID groupId : m_aGroupIds)
		{
			if (i < m_aGroupRecords.Count() && m_aGroupRecords[i].m_bIsBattleGroup)
				continue;

			SCR_AIGroup group = SCR_AIGroup.Cast(world.FindEntityByID(groupId));
			if (group)
				total += group.GetAgentsCount() + group.GetSpawnQueueSize();
		}
		return total;
	}

	int GetQueuedGarrisonRequests()
	{
		return m_iQueuedGarrisonUnits;
	}
}

[ComponentEditorProps(category: "SPT/GameMode", description: "Dynamically spawns AI garrisons in settlements and strategic map locations")]
class SPT_WorldGarrisonManagerComponentClass : ScriptComponentClass
{
}

class SPT_WorldGarrisonManagerComponent : ScriptComponent
{
	protected const float PATROL_SPAWN_BUILDING_CLEARANCE_M = 30.0;
	protected const float PATROL_SPAWN_GROUP_MIN_CLEARANCE_M = 40.0;
	protected const float PATROL_SPAWN_GROUP_RADIUS_FACTOR = 0.30;
	protected const float PATROL_MEMBER_MIN_SPACING_M = 3.0;
	protected const float PATROL_MEMBER_MAX_SPACING_M = 5.0;
	protected const float PATROL_MEMBER_RADIUS_FACTOR = 0.01;
	protected const int PATROL_SPAWN_RING_COUNT = 4;
	protected const int PATROL_SPAWN_SAMPLES_PER_RING = 24;
	protected const int BATTLE_UPDATE_INTERVAL_MS = 5000;
	protected const int GARRISON_STATE_CHECK_INTERVAL_MS = 2000;

	protected static SPT_WorldGarrisonManagerComponent s_Instance;

	//-----------------------------------------------------------------------
	// EVENTOS PUBLICOS (Warfare)
	//-----------------------------------------------------------------------

	//! Disparado quando a primeira baixa da guarnicao ocorre.
	//! Parametros: string locationId
	protected ref ScriptInvoker m_OnGarrisonFirstCasualty = new ScriptInvoker();

	//! Disparado quando a guarnicao local e completamente eliminada.
	//! Parametros: string locationId
	protected ref ScriptInvoker m_OnGarrisonCleared = new ScriptInvoker();

	//! Disparado quando uma batalha explicita e iniciada.
	//! Parametros: string locationId
	protected ref ScriptInvoker m_OnBattleStarted = new ScriptInvoker();

	//! Disparado quando uma nova onda de batalha e agendada.
	//! Parametros: string locationId, int waveIndex
	protected ref ScriptInvoker m_OnBattleWaveScheduled = new ScriptInvoker();

	//! Disparado quando uma onda de batalha e materializada (grupos spawnados).
	//! Parametros: string locationId, int waveIndex, int unitCount
	protected ref ScriptInvoker m_OnBattleWaveDeployed = new ScriptInvoker();

	//! Disparado quando uma batalha e encerrada (todas as ondas derrotadas
	//! ou batalha cancelada).
	//! Parametros: string locationId
	protected ref ScriptInvoker m_OnBattleEnded = new ScriptInvoker();

	static SPT_WorldGarrisonManagerComponent GetInstance()
	{
		return s_Instance;
	}

	[Attribute("0", desc: "Enable detailed diagnostic logs with the [SPT_WorldGarrison][DEBUG] prefix")]
	protected bool m_bDebug;

	[Attribute("2", desc: "Seconds between player-distance checks")]
	protected float m_fUpdateInterval;

	[Attribute("500", desc: "Game Master AI budget used by the garrison. Increase this when many locations can be active simultaneously. Set to 0 to keep the scenario budget unchanged.")]
	protected int m_iGameMasterAIBudget;

	[Attribute("$profile:/SPT_RoadNetwork.json", desc: "Caminho do JSON de rede viaria gerado no Workbench.", category: "Batalha")]
	protected string m_sRoadNetworkDatasetPath;

	//! Mantidos apenas para o codigo legado de descritores, que nao e mais executado.
	protected float m_fMinimumLocationSpacing;
	protected bool m_bIncludeSettlements;
	protected bool m_bIncludeStrategicSites;

	protected ref array<ref SPT_GarrisonLocation> m_aLocations = new array<ref SPT_GarrisonLocation>();
	protected ref array<ref SPT_GarrisonDescriptorCandidate> m_aDescriptorCandidates = new array<ref SPT_GarrisonDescriptorCandidate>();
	protected ref array<ref SPT_GarrisonLocation> m_aDiscardedDescriptorLocations = new array<ref SPT_GarrisonLocation>();
	protected int m_iDebugUpdateCounter;
	protected bool m_bBuildingEditorPreview;

	//! Fila de spawns pendentes processados um por tick.
	protected ref array<ref SPT_GarrisonSpawnRequest> m_aPendingSpawns = new array<ref SPT_GarrisonSpawnRequest>();

	//! Verdadeiro enquanto o CallLater de ProcessSpawnQueue esta ativo.
	protected bool m_bProcessingSpawnQueue;

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		bool editMode = SCR_Global.IsEditMode();
		bool isServer = Replication.IsServer();
		Print(string.Format("[SPT_WorldGarrison] OnPostInit | dono=%1 | modoEdicao=%2 | servidor=%3 | debug=%4",
			owner, editMode, isServer, m_bDebug));

		if (editMode)
		{
			DebugLog("Inicializacao ignorada porque o mundo esta em modo de edicao.");
			return;
		}

		if (!isServer)
		{
			DebugLog("Inicializacao ignorada no cliente; o servidor controla o spawn da guarnicao.");
			return;
		}

		s_Instance = this;
		if (!m_sRoadNetworkDatasetPath.IsEmpty())
			SPT_RoadNetworkService.Load(m_sRoadNetworkDatasetPath);

		ValidateSettings();
		DebugLog("Inicializacao agendada em 1000 ms; configuracoes de area serao lidas exclusivamente dos SPT_WarfarePoint.");
		if (m_iGameMasterAIBudget > 0)
			GetGame().GetCallqueue().CallLater(ConfigureGameMasterBudget, 1000, false, 10);
		GetGame().GetCallqueue().CallLater(InitializeLocations, 1000, false);
		GetGame().GetCallqueue().CallLater(UpdateBattles, BATTLE_UPDATE_INTERVAL_MS, true);
		GetGame().GetCallqueue().CallLater(MonitorGarrisonState, GARRISON_STATE_CHECK_INTERVAL_MS, true);
	}

	protected void ValidateSettings()
	{
		if (m_fUpdateInterval < 0.25)
			m_fUpdateInterval = 0.25;

		if (m_iGameMasterAIBudget < 0)
			m_iGameMasterAIBudget = 0;

		if (m_fMinimumLocationSpacing < 0)
			m_fMinimumLocationSpacing = 0;
	}

	protected void ConfigureGameMasterBudget(int retriesLeft)
	{
		array<Managed> components = {};
		int componentCount = SCR_BaseEditorComponent.GetAllInstances(SCR_BudgetEditorComponent, components);
		if (componentCount < 1)
		{
			if (retriesLeft > 0)
			{
				DebugLog(string.Format("Componente de orcamento Game Master nao esta pronto; tentando novamente (%1 tentativas restantes).", retriesLeft));
				GetGame().GetCallqueue().CallLater(ConfigureGameMasterBudget, 1000, false, retriesLeft - 1);
				return;
			}

			Print("[SPT_WorldGarrison] SCR_BudgetEditorComponent nao encontrado; orcamento de IA do Game Master nao foi alterado.", LogLevel.WARNING);
			return;
		}

		int configuredComponents;
		foreach (Managed component : components)
		{
			SCR_BudgetEditorComponent budgetComponent = SCR_BudgetEditorComponent.Cast(component);
			if (!budgetComponent)
				continue;

			int previousAI;
			int previousServerAI;
			budgetComponent.GetMaxBudgetValue(EEditableEntityBudget.AI, previousAI);
			budgetComponent.GetMaxBudgetValue(EEditableEntityBudget.AI_SERVER, previousServerAI);
			budgetComponent.SetMaxBudgetValue(EEditableEntityBudget.AI, m_iGameMasterAIBudget);
			budgetComponent.SetMaxBudgetValue(EEditableEntityBudget.AI_SERVER, m_iGameMasterAIBudget);
			configuredComponents++;

			DebugLog(string.Format("Orcamento Game Master atualizado | componente=%1 | IA=%2->%3 | IA_SERVIDOR=%4->%5",
				budgetComponent, previousAI, m_iGameMasterAIBudget, previousServerAI, m_iGameMasterAIBudget));
		}

		Print(string.Format("[SPT_WorldGarrison] Orcamento de IA do Game Master definido para %1 em %2 componente(s).",
			m_iGameMasterAIBudget, configuredComponents));
	}

	protected void ConfigureAIWorldLimits()
	{
		if (m_iGameMasterAIBudget <= 0)
			return;

		AIWorld aiWorld = GetGame().GetAIWorld();
		if (!aiWorld)
		{
			Print("[SPT_WorldGarrison] AIWorld nao esta disponivel; limites reais de IA nao foram alterados.", LogLevel.WARNING);
			return;
		}

		int previousAILimit = aiWorld.GetAILimit();
		int previousActiveLimit = aiWorld.GetLimitOfActiveAIs();
		int newAILimit = previousAILimit;
		if (newAILimit < m_iGameMasterAIBudget)
			newAILimit = m_iGameMasterAIBudget;
		int newActiveLimit = previousActiveLimit;
		if (newActiveLimit < m_iGameMasterAIBudget)
			newActiveLimit = m_iGameMasterAIBudget;
		aiWorld.SetAILimit(newAILimit);
		aiWorld.SetLimitOfActiveAIs(newActiveLimit);
		Print(string.Format("[SPT_WorldGarrison] Limites AIWorld atualizados | total=%1->%2 | ativos=%3->%4 | atuais=%5 | ativosAtuais=%6",
			previousAILimit,
			newAILimit,
			previousActiveLimit,
			newActiveLimit,
			aiWorld.GetCurrentAmountOfLimitedAIs(),
			aiWorld.GetCurrentNumOfActiveAIs()));
	}

	protected bool HasValidResourceList(array<ResourceName> prefabs)
	{
		if (!prefabs)
			return false;

		foreach (ResourceName prefab : prefabs)
		{
			if (!prefab.IsEmpty())
				return true;
		}

		return false;
	}

	protected void CopyValidResources(array<ResourceName> source, notnull array<ResourceName> target)
	{
		if (!source)
			return;

		foreach (ResourceName prefab : source)
		{
			if (!prefab.IsEmpty())
				target.Insert(prefab);
		}
	}

	protected float GetLocationSpawnDistance(notnull SPT_GarrisonLocation location)
	{
		return Math.Max(1.0, location.m_Config.m_fSpawnDistance);
	}

	protected float GetLocationDespawnDistance(notnull SPT_GarrisonLocation location)
	{
		float spawnDistance = GetLocationSpawnDistance(location);
		if (location.m_Config.m_fDespawnDistance > spawnDistance)
			return location.m_Config.m_fDespawnDistance;
		return spawnDistance + 1.0;
	}

	protected float GetLocationDespawnHysteresis(notnull SPT_GarrisonLocation location)
	{
		return Math.Max(0.0, location.m_Config.m_fDespawnHysteresis);
	}

	protected float GetLocationBuildingSearchRadius(notnull SPT_GarrisonLocation location)
	{
		return Math.Max(25.0, location.m_Config.m_fBuildingSearchRadius);
	}

	protected float GetLocationPatrolRadius(notnull SPT_GarrisonLocation location)
	{
		return Math.Max(25.0, location.m_Config.m_fPatrolRadius);
	}

	protected int GetLocationBudget(notnull SPT_GarrisonLocation location)
	{
		return Math.Max(0, location.m_Config.m_iLocationBudget);
	}

	protected bool IsLocationCachingEnabled(notnull SPT_GarrisonLocation location)
	{
		return location.m_Config.m_bEnableCaching;
	}

	protected int GetLocationSpawnIntervalMs(notnull SPT_GarrisonLocation location)
	{
		return Math.Max(0, location.m_Config.m_iSpawnIntervalMs);
	}

	protected bool IsLocationBattleEnabled(notnull SPT_GarrisonLocation location)
	{
		return location.m_Config.m_bBattleEnabled;
	}

	protected int GetLocationBattleInitialDelayMinMs(notnull SPT_GarrisonLocation location)
	{
		return Math.Max(0, location.m_Config.m_iBattleInitialDelayMinMs);
	}

	protected int GetLocationBattleInitialDelayMaxMs(notnull SPT_GarrisonLocation location)
	{
		return Math.Max(GetLocationBattleInitialDelayMinMs(location), location.m_Config.m_iBattleInitialDelayMaxMs);
	}

	protected int GetLocationBattleWaveDelayMinMs(notnull SPT_GarrisonLocation location)
	{
		return Math.Max(0, location.m_Config.m_iBattleWaveDelayMinMs);
	}

	protected int GetLocationBattleWaveDelayMaxMs(notnull SPT_GarrisonLocation location)
	{
		return Math.Max(GetLocationBattleWaveDelayMinMs(location), location.m_Config.m_iBattleWaveDelayMaxMs);
	}

	protected int GetLocationBattleSpawnIntervalMs(notnull SPT_GarrisonLocation location)
	{
		return Math.Max(0, location.m_Config.m_iBattleSpawnIntervalMs);
	}

	protected int GetLocationBattleWaveUnitsMin(notnull SPT_GarrisonLocation location)
	{
		return Math.Max(1, location.m_Config.m_iBattleWaveUnitsMin);
	}

	protected int GetLocationBattleWaveUnitsMax(notnull SPT_GarrisonLocation location)
	{
		return Math.Max(GetLocationBattleWaveUnitsMin(location), location.m_Config.m_iBattleWaveUnitsMax);
	}

	protected float GetLocationBattleWaveAliveThreshold(notnull SPT_GarrisonLocation location)
	{
		return Math.Clamp(location.m_Config.m_fBattleWaveAliveThreshold, 0.0, 1.0);
	}

	protected float GetLocationBattleSpawnDistanceMin(notnull SPT_GarrisonLocation location)
	{
		return Math.Max(100.0, location.m_Config.m_fBattleSpawnDistanceMin);
	}

	protected float GetLocationBattleSpawnDistanceMax(notnull SPT_GarrisonLocation location)
	{
		return Math.Max(GetLocationBattleSpawnDistanceMin(location), location.m_Config.m_fBattleSpawnDistanceMax);
	}

	protected float GetLocationBattleConcentratedWeight(notnull SPT_GarrisonLocation location)
	{
		return Math.Max(0.0, location.m_Config.m_fBattleConcentratedWeight);
	}

	protected float GetLocationBattleSpreadedWeight(notnull SPT_GarrisonLocation location)
	{
		return Math.Max(0.0, location.m_Config.m_fBattleSpreadedWeight);
	}

	protected float GetLocationBattleConvoyWeight(notnull SPT_GarrisonLocation location)
	{
		return Math.Max(0.0, location.m_Config.m_fBattleConvoyWeight);
	}

	protected ResourceName GetLocationPatrolWaypointPrefab(notnull SPT_GarrisonLocation location)
	{
		return location.m_Config.m_sPatrolWaypointPrefab;
	}

	protected SCR_EAIGroupFormation GetLocationPatrolFormation(notnull SPT_GarrisonLocation location)
	{
		return location.m_Config.m_ePatrolFormation;
	}

	protected EMovementType GetLocationPatrolMovementType(notnull SPT_GarrisonLocation location)
	{
		return location.m_Config.m_ePatrolMovementType;
	}

	protected EMovementType GetLocationBattleMovementType(notnull SPT_GarrisonLocation location)
	{
		return location.m_Config.m_eBattleMovementType;
	}

	protected void GetLocationCQBPrefabs(notnull SPT_GarrisonLocation location, notnull array<ResourceName> outPrefabs)
	{
		outPrefabs.Clear();
		CopyValidResources(location.m_Config.m_aCQBGroupPrefabs, outPrefabs);
	}

	protected void GetLocationPatrolPrefabs(notnull SPT_GarrisonLocation location, notnull array<ResourceName> outPrefabs)
	{
		outPrefabs.Clear();
		CopyValidResources(location.m_Config.m_aPatrolGroupPrefabs, outPrefabs);
	}

	protected void GetLocationBattlePrefabs(notnull SPT_GarrisonLocation location, notnull array<ResourceName> outPrefabs)
	{
		outPrefabs.Clear();
		if (HasValidResourceList(location.m_Config.m_aBattleGroupPrefabs))
		{
			CopyValidResources(location.m_Config.m_aBattleGroupPrefabs, outPrefabs);
			return;
		}

		if (HasValidResourceList(location.m_Config.m_aPatrolGroupPrefabs))
		{
			CopyValidResources(location.m_Config.m_aPatrolGroupPrefabs, outPrefabs);
			return;
		}

	}

	protected int GetEffectiveCQBPrefabCount(notnull SPT_GarrisonLocation location)
	{
		array<ResourceName> prefabs = {};
		GetLocationCQBPrefabs(location, prefabs);
		return prefabs.Count();
	}

	protected int GetEffectivePatrolPrefabCount(notnull SPT_GarrisonLocation location)
	{
		array<ResourceName> prefabs = {};
		GetLocationPatrolPrefabs(location, prefabs);
		return prefabs.Count();
	}

	protected int GetEffectiveBattlePrefabCount(notnull SPT_GarrisonLocation location)
	{
		array<ResourceName> prefabs = {};
		GetLocationBattlePrefabs(location, prefabs);
		return prefabs.Count();
	}

	protected void InitializeLocations()
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
		{
			Print("[SPT_WorldGarrison] Falha na inicializacao: mundo do jogo e nulo.", LogLevel.ERROR);
			return;
		}

		ConfigureAIWorldLimits();
		DebugLog("Verificando descritores do mapa em busca de assentamentos e locais estrategicos...");
		m_aLocations.Clear();
		m_aDescriptorCandidates.Clear();
		m_aDiscardedDescriptorLocations.Clear();

		Print("[SPT_WorldGarrison] Descoberta automatica desativada; aguardando configuracoes dos SPT_WarfarePoint.");

		if (m_aLocations.IsEmpty())
			Print("[SPT_WorldGarrison] Nenhum local de mapa suportado foi encontrado. Verifique os tipos de descritor e as configuracoes de inclusao.", LogLevel.WARNING);

		// O Warfare registra os objetivos inimigos depois deste componente.
		// Atrasamos o loop para que as localizacoes manuais existam antes do stream-in.
		GetGame().GetCallqueue().CallLater(StartLocationUpdateLoop, 1500, false);
		DebugLog("Primeiro ciclo de distancia agendado apos o registro dos objetivos Warfare.");
	}

	protected void StartLocationUpdateLoop()
	{
		UpdateLocations();
		int updateIntervalMs = Math.Round(m_fUpdateInterval * 1000);
		GetGame().GetCallqueue().CallLater(UpdateLocations, updateIntervalMs, true);
		DebugLog(string.Format("Loop de atualizacao de distancia iniciado com intervalo de %1 ms.",
			updateIntervalMs));
	}

	protected bool FilterLocation(IEntity entity)
	{
		if (!entity)
			return false;

		MapDescriptorComponent descriptor = MapDescriptorComponent.Cast(entity.FindComponent(MapDescriptorComponent));
		if (!descriptor)
			return false;

		return IsSupportedDescriptor(descriptor.GetBaseType());
	}

	protected bool CollectLocation(IEntity entity)
	{
		MapDescriptorComponent descriptor = MapDescriptorComponent.Cast(entity.FindComponent(MapDescriptorComponent));
		if (!descriptor)
			return true;

		vector center = entity.GetOrigin();

		string name = "Map location";
		MapItem item = descriptor.Item();
		if (item)
		{
			string displayName = item.GetDisplayName();
			if (!displayName.IsEmpty())
				name = displayName;
		}

		SPT_GarrisonDescriptorCandidate candidate = new SPT_GarrisonDescriptorCandidate();
		candidate.m_vCenter = center;
		candidate.m_sName = name;
		candidate.m_iDescriptorType = descriptor.GetBaseType();
		candidate.m_sLocationId = SPT_GarrisonLocation.GenerateLocationId(
			name,
			candidate.m_iDescriptorType,
			center);
		candidate.m_iPriority = GetDescriptorPriority(candidate.m_iDescriptorType);
		m_aDescriptorCandidates.Insert(candidate);
		return true;
	}

	protected void ResolveDescriptorCandidates()
	{
		ref array<ref SPT_GarrisonDescriptorCandidate> pending = new array<ref SPT_GarrisonDescriptorCandidate>();
		foreach (SPT_GarrisonDescriptorCandidate candidate : m_aDescriptorCandidates)
			pending.Insert(candidate);

		float mergeDistanceSq = m_fMinimumLocationSpacing * m_fMinimumLocationSpacing;
		while (!pending.IsEmpty())
		{
			int bestIndex;
			for (int i = 1; i < pending.Count(); i++)
			{
				if (IsDescriptorCandidateBetter(pending[i], pending[bestIndex]))
					bestIndex = i;
			}

			SPT_GarrisonDescriptorCandidate winner = pending[bestIndex];
			pending.Remove(bestIndex);

			SPT_GarrisonLocation duplicateOf;
			float duplicateDistanceSq;
			foreach (SPT_GarrisonLocation accepted : m_aLocations)
			{
				float distanceSq = HorizontalDistanceSq(winner.m_vCenter, accepted.m_vCenter);
				if (distanceSq <= mergeDistanceSq)
				{
					duplicateOf = accepted;
					duplicateDistanceSq = distanceSq;
					break;
				}
			}

			if (duplicateOf)
			{
				m_aDiscardedDescriptorLocations.Insert(new SPT_GarrisonLocation(
					winner.m_vCenter,
					winner.m_sName,
					winner.m_iDescriptorType,
					winner.m_sLocationId));
				if (!m_bBuildingEditorPreview)
				{
					Print(string.Format("[SPT_WorldGarrison] Descritor duplicado descartado | descartado=%1 (%2) | vencedor=%3 (%4) | distancia=%5m",
						winner.m_sName,
						winner.m_iDescriptorType,
						duplicateOf.m_sName,
						duplicateOf.m_iDescriptorType,
						Math.Sqrt(duplicateDistanceSq)), LogLevel.WARNING);
				}
				continue;
			}

			m_aLocations.Insert(new SPT_GarrisonLocation(
				winner.m_vCenter,
				winner.m_sName,
				winner.m_iDescriptorType,
				winner.m_sLocationId));
			DebugLog(string.Format("Descritor canonico | id=%1 | nome=%2 | tipo=%3 | prioridade=%4 | posicao=%5",
				winner.m_sLocationId,
				winner.m_sName,
				winner.m_iDescriptorType,
				winner.m_iPriority,
				winner.m_vCenter));
		}
	}

	protected bool IsDescriptorCandidateBetter(
		notnull SPT_GarrisonDescriptorCandidate candidate,
		notnull SPT_GarrisonDescriptorCandidate current)
	{
		if (candidate.m_iPriority != current.m_iPriority)
			return candidate.m_iPriority > current.m_iPriority;

		bool candidateNamed = candidate.m_sName != "Map location";
		bool currentNamed = current.m_sName != "Map location";
		if (candidateNamed != currentNamed)
			return candidateNamed;

		int idCompare = candidate.m_sLocationId.Compare(current.m_sLocationId);
		if (idCompare != 0)
			return idCompare < 0;

		if (candidate.m_vCenter[0] != current.m_vCenter[0])
			return candidate.m_vCenter[0] < current.m_vCenter[0];
		return candidate.m_vCenter[2] < current.m_vCenter[2];
	}

	protected void ValidateCanonicalLocations()
	{
		float minimumDistanceSq = m_fMinimumLocationSpacing * m_fMinimumLocationSpacing;
		for (int i = 0; i < m_aLocations.Count(); i++)
		{
			for (int j = i + 1; j < m_aLocations.Count(); j++)
			{
				SPT_GarrisonLocation locationA = m_aLocations[i];
				SPT_GarrisonLocation locationB = m_aLocations[j];
				if (locationA.m_sLocationId == locationB.m_sLocationId)
				{
					Print(string.Format("[SPT_WorldGarrison] ERRO: ID canonico duplicado: %1",
						locationA.m_sLocationId), LogLevel.ERROR);
				}

				float distanceSq = HorizontalDistanceSq(locationA.m_vCenter, locationB.m_vCenter);
				if (distanceSq <= minimumDistanceSq)
				{
					Print(string.Format("[SPT_WorldGarrison] ERRO: Areas canonicas sobrepostas apos deduplicacao | %1 | %2 | distancia=%3m",
						locationA.m_sLocationId,
						locationB.m_sLocationId,
						Math.Sqrt(distanceSq)), LogLevel.ERROR);
				}
			}
		}
	}

	protected int GetDescriptorPriority(int descriptorType)
	{
		if (descriptorType == EMapDescriptorType.MDT_AIRPORT) return 100;
		if (descriptorType == EMapDescriptorType.MDT_BASE) return 95;
		if (descriptorType == EMapDescriptorType.MDT_PORT) return 90;
		if (descriptorType == EMapDescriptorType.MDT_RADIO) return 85;
		if (descriptorType == EMapDescriptorType.MDT_POLICE) return 80;
		if (descriptorType == EMapDescriptorType.MDT_FIREDEP) return 75;
		if (descriptorType == EMapDescriptorType.MDT_CONSTRUCTION_SITE) return 70;
		if (descriptorType == EMapDescriptorType.MDT_LANDMARK) return 65;
		if (descriptorType == EMapDescriptorType.MDT_NAME_CITY) return 50;
		if (descriptorType == EMapDescriptorType.MDT_NAME_TOWN) return 40;
		if (descriptorType == EMapDescriptorType.MDT_NAME_VILLAGE) return 30;
		if (descriptorType == EMapDescriptorType.MDT_NAME_SETTLEMENT) return 20;
		return 0;
	}

	//! Resolve descritores no World Editor usando exatamente a mesma regra do runtime.
	void BuildEditorPreviewLocations(notnull array<ref SPT_GarrisonLocation> outLocations)
	{
		outLocations.Clear();
		if (!SCR_Global.IsEditMode())
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		m_bBuildingEditorPreview = true;
		m_aLocations.Clear();
		m_aDescriptorCandidates.Clear();
		m_aDiscardedDescriptorLocations.Clear();
		world.QueryEntitiesBySphere(
			vector.Zero,
			99999999,
			CollectLocation,
			FilterLocation,
			EQueryEntitiesFlags.ALL);
		ResolveDescriptorCandidates();
		foreach (SPT_GarrisonLocation location : m_aLocations)
			outLocations.Insert(location);
		m_bBuildingEditorPreview = false;
	}

	//! Retorna os descritores descartados pela deduplicacao para destaque
	//! magenta no preview. Eles nunca entram nas localizacoes do runtime.
	void GetEditorPreviewDuplicateLocations(notnull array<ref SPT_GarrisonLocation> outLocations)
	{
		outLocations.Clear();
		if (!SCR_Global.IsEditMode())
			return;
		foreach (SPT_GarrisonLocation location : m_aDiscardedDescriptorLocations)
			outLocations.Insert(location);
	}

	protected bool IsSupportedDescriptor(int descriptorType)
	{
		if (m_bIncludeSettlements)
		{
			if (descriptorType == EMapDescriptorType.MDT_NAME_CITY)
				return true;
			if (descriptorType == EMapDescriptorType.MDT_NAME_TOWN)
				return true;
			if (descriptorType == EMapDescriptorType.MDT_NAME_VILLAGE)
				return true;
			if (descriptorType == EMapDescriptorType.MDT_NAME_SETTLEMENT)
				return true;
		}

		if (m_bIncludeStrategicSites)
		{
			if (descriptorType == EMapDescriptorType.MDT_BASE)
				return true;
			if (descriptorType == EMapDescriptorType.MDT_AIRPORT)
				return true;
			if (descriptorType == EMapDescriptorType.MDT_PORT)
				return true;
			if (descriptorType == EMapDescriptorType.MDT_RADIO)
				return true;
			if (descriptorType == EMapDescriptorType.MDT_POLICE)
				return true;
			if (descriptorType == EMapDescriptorType.MDT_FIREDEP)
				return true;
			if (descriptorType == EMapDescriptorType.MDT_CONSTRUCTION_SITE)
				return true;
			if (descriptorType == EMapDescriptorType.MDT_LANDMARK)
				return true;
		}

		return false;
	}

	protected bool IsNearRegisteredLocation(vector position)
	{
		float minimumDistanceSq = m_fMinimumLocationSpacing * m_fMinimumLocationSpacing;
		foreach (SPT_GarrisonLocation location : m_aLocations)
		{
			if (HorizontalDistanceSq(position, location.m_vCenter) < minimumDistanceSq)
				return true;
		}

		return false;
	}

	protected void UpdateLocations()
	{
		m_iDebugUpdateCounter++;
		bool logThisUpdate = false;
		if (m_bDebug && m_iDebugUpdateCounter % 5 == 0)
			logThisUpdate = true;

		array<vector> playerPositions = {};
		CollectPlayerPositions(playerPositions);
		if (logThisUpdate)
			DebugLog(string.Format("Atualizacao de distancia | jogadoresComPosicao=%1 | locais=%2",
				playerPositions.Count(), m_aLocations.Count()));

		if (playerPositions.IsEmpty())
		{
			if (logThisUpdate)
				DebugLog("Nenhuma entidade controlada ou principal valida foi encontrada.");

			foreach (SPT_GarrisonLocation emptyServerLocation : m_aLocations)
			{
				if (emptyServerLocation.m_bActive)
					DespawnLocation(emptyServerLocation);
			}
			return;
		}

		foreach (SPT_GarrisonLocation location : m_aLocations)
		{
			if (location.m_bSafe)
				continue;
			if (location.m_bUnavailable)
				continue;
			if (location.m_bCleared && !location.m_Battle.m_bActive)
				continue;
			if (location.m_bSpawning)
				continue;

			float nearestPlayerSq = GetNearestPlayerDistanceSq(location.m_vCenter, playerPositions);

			float spawnDistance = GetLocationSpawnDistance(location);
			float spawnDistanceSq = spawnDistance * spawnDistance;
			float effectiveDespawnDistance = GetLocationDespawnDistance(location);
			if (location.m_bActive)
				effectiveDespawnDistance = effectiveDespawnDistance + GetLocationDespawnHysteresis(location);
			float effectiveDespawnSq = effectiveDespawnDistance * effectiveDespawnDistance;

			if (logThisUpdate && nearestPlayerSq <= effectiveDespawnSq * 4)
			{
				DebugLog(string.Format("Distancia do local | nome=%1 | distancia=%2m | ativo=%3 | indisponivel=%4 | spawn=%5m | despawnEfetivo=%6m",
					location.m_sName, Math.Sqrt(nearestPlayerSq), location.m_bActive, location.m_bUnavailable, spawnDistance, Math.Sqrt(effectiveDespawnSq)));
			}

			if (!location.m_bActive && nearestPlayerSq <= spawnDistanceSq)
			{
				// Um snapshot vazio tem significado: esta localizacao foi
				// completamente eliminada e nao deve receber uma guarnicao nova.
				if (IsLocationCachingEnabled(location) && location.m_bHasCachedSnapshot && location.m_aCachedGroups.IsEmpty())
				{
					location.RefreshClearedState();
					if (logThisUpdate)
						DebugLog(string.Format("Guarnicao local eliminada | nome=%1 | reservaBatalha=%2.",
							location.m_sName, location.m_iBudgetRemaining));
					continue;
				}

				DebugLog(string.Format("Limite de spawn atingido para %1.", location.m_sName));
				SpawnLocation(location);
			}
			else if (location.m_bActive && nearestPlayerSq > effectiveDespawnSq)
			{
				DebugLog(string.Format("Limite de despawn atingido para %1.", location.m_sName));
				DespawnLocation(location);
			}
		}
	}

	protected void CollectPlayerPositions(out array<vector> positions)
	{
		positions.Clear();

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
		{
			DebugLog("PlayerManager e nulo.");
			return;
		}

		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);
		if (m_bDebug && m_iDebugUpdateCounter % 5 == 0)
			DebugLog(string.Format("PlayerManager retornou %1 jogadores conectados.", playerIds.Count()));

		foreach (int playerId : playerIds)
		{
			IEntity playerEntity = playerManager.GetPlayerControlledEntity(playerId);
			string source = "controlled";
			if (!playerEntity)
			{
				playerEntity = SCR_PossessingManagerComponent.GetPlayerMainEntity(playerId);
				source = "main";
			}
			if (playerEntity)
			{
				positions.Insert(playerEntity.GetOrigin());
				if (m_bDebug && m_iDebugUpdateCounter % 5 == 0)
				{
					DebugLog(string.Format("Posicao do jogador | id=%1 | origem=%2 | classe=%3 | posicao=%4",
						playerId, source, playerEntity.ClassName(), playerEntity.GetOrigin()));
				}
			}
			else
			{
				DebugLog(string.Format("Nenhuma entidade espacial encontrada para o jogador id %1.", playerId));
			}
		}
	}

	protected float GetNearestPlayerDistanceSq(vector location, notnull array<vector> playerPositions)
	{
		float nearestSq = float.MAX;
		foreach (vector playerPosition : playerPositions)
		{
			float distanceSq = HorizontalDistanceSq(location, playerPosition);
			if (distanceSq < nearestSq)
				nearestSq = distanceSq;
		}

		return nearestSq;
	}

	//! Inicia uma batalha explicita na localizacao registrada mais proxima.
	//! O stream-in comum nunca chama este metodo.
	bool StartLocationBattle(vector position)
	{
		if (!Replication.IsServer())
			return false;

		SPT_GarrisonLocation location = FindNearestLocation(position);
		if (!location || location.m_bSafe || location.m_Battle.m_bActive || !location.CanDeployUnits())
			return false;

		if (!IsLocationBattleEnabled(location))
			return false;

		location.m_Battle.Reset();
		location.m_Battle.m_bActive = true;
		int initialDelayMin = GetLocationBattleInitialDelayMinMs(location);
		int initialDelayMax = GetLocationBattleInitialDelayMaxMs(location);
		if (initialDelayMax < initialDelayMin)
			initialDelayMax = initialDelayMin;
		location.m_Battle.m_iTimerMs = RandomRangeInt(initialDelayMin, initialDelayMax);
		BuildBattleWaves(location);

		if (location.m_Battle.m_aWaves.IsEmpty())
		{
			location.m_Battle.Reset();
			return false;
		}

		Print(string.Format("[SPT_WorldGarrison] Batalha iniciada | local=%1 | ondas=%2 | reserva=%3 | atrasoInicialMs=%4",
			location.m_sName,
			location.m_Battle.m_aWaves.Count(),
			location.m_iBudgetRemaining,
			location.m_Battle.m_iTimerMs));
		m_OnBattleStarted.Invoke(location.m_sLocationId);
		return true;
	}

	bool CancelLocationBattle(vector position)
	{
		if (!Replication.IsServer())
			return false;

		SPT_GarrisonLocation location = FindNearestLocation(position);
		if (!location || !location.m_Battle.m_bActive)
			return false;

		location.m_Battle.m_bCancelled = true;
		location.m_Battle.m_bActive = false;
		CancelPendingBattleSpawns(location);
		RemoveBattleForces(location);
		Print(string.Format("[SPT_WorldGarrison] Batalha cancelada | local=%1 | reserva=%2",
			location.m_sName, location.m_iBudgetRemaining));
		m_OnBattleEnded.Invoke(location.m_sLocationId);
		return true;
	}

	protected void RemoveBattleForces(notnull SPT_GarrisonLocation location)
	{
		BaseWorld world = GetGame().GetWorld();
		for (int i = location.m_aGroupIds.Count() - 1; i >= 0; i--)
		{
			if (i >= location.m_aGroupRecords.Count() || !location.m_aGroupRecords[i].m_bIsBattleGroup)
				continue;
			SCR_AIGroup group = SCR_AIGroup.Cast(world.FindEntityByID(location.m_aGroupIds[i]));
			if (group)
				DeleteGroup(group);
			location.m_aGroupIds.Remove(i);
			location.m_aGroupRecords.Remove(i);
		}

		for (int cacheIndex = location.m_aCachedGroups.Count() - 1; cacheIndex >= 0; cacheIndex--)
		{
			if (location.m_aCachedGroups[cacheIndex].m_bIsBattleGroup)
				location.m_aCachedGroups.Remove(cacheIndex);
		}

		foreach (EntityID vehicleId : location.m_Battle.m_aVehicleIds)
		{
			IEntity vehicle = world.FindEntityByID(vehicleId);
			if (vehicle)
				SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
		}
		location.m_Battle.m_aVehicleIds.Clear();
	}

	bool IsLocationBattleActive(vector position)
	{
		SPT_GarrisonLocation location = FindNearestLocation(position);
		return location && !location.m_bSafe && location.m_Battle.m_bActive;
	}

	//-----------------------------------------------------------------------
	// GETTERS DE EVENTOS PUBLICOS (Warfare)
	//-----------------------------------------------------------------------

	ScriptInvoker GetOnGarrisonFirstCasualty() { return m_OnGarrisonFirstCasualty; }
	ScriptInvoker GetOnGarrisonCleared() { return m_OnGarrisonCleared; }
	ScriptInvoker GetOnBattleStarted() { return m_OnBattleStarted; }
	ScriptInvoker GetOnBattleWaveScheduled() { return m_OnBattleWaveScheduled; }
	ScriptInvoker GetOnBattleWaveDeployed() { return m_OnBattleWaveDeployed; }
	ScriptInvoker GetOnBattleEnded() { return m_OnBattleEnded; }

	//-----------------------------------------------------------------------
	// CONSULTAS PUBLICAS (Warfare)
	//-----------------------------------------------------------------------

	//! Encontra uma localizacao pelo ID estavel.
	//! @param locationId O identificador estavel da localizacao
	//! @return A localizacao ou null se nao encontrada
	SPT_GarrisonLocation FindLocationById(string locationId)
	{
		if (locationId.IsEmpty())
			return null;

		foreach (SPT_GarrisonLocation location : m_aLocations)
		{
			if (location.m_sLocationId == locationId)
				return location;
		}
		return null;
	}

	//! Retorna o numero total de localizacoes registradas.
	int GetLocationCount()
	{
		return m_aLocations.Count();
	}

	//! Substitui descritores automaticos por localizacoes definidas pelos
	//! SPT_WarfarePoint. Deve ser chamado antes do primeiro ciclo de streaming.
	void ResetLocationsForManualWarfare()
	{
		if (!Replication.IsServer())
			return;

		foreach (SPT_GarrisonLocation location : m_aLocations)
		{
			location.m_bSafe = true;
			ClearLocationForSafe(location);
		}
		m_aLocations.Clear();
		m_aDescriptorCandidates.Clear();
		m_aDiscardedDescriptorLocations.Clear();
	}

	bool RegisterManualWarfareLocation(string locationId, string displayName, vector center, SPT_GarrisonLocationConfig config)
	{
		if (!Replication.IsServer() || locationId.IsEmpty() || FindLocationById(locationId))
			return false;
		if (!config)
		{
			Print(string.Format("[SPT_WorldGarrison] Ponto %1 rejeitado: configuracao local obrigatoria ausente.", locationId), LogLevel.ERROR);
			return false;
		}

		SPT_GarrisonLocation location = new SPT_GarrisonLocation(
			center,
			displayName,
			EMapDescriptorType.MDT_BASE,
			locationId);
		location.m_Config = config;
		location.InitBudget(GetLocationBudget(location));
		m_aLocations.Insert(location);
		Print(string.Format("[SPT_WorldGarrison] Localizacao Warfare manual registrada | id=%1 | nome=%2 | posicao=%3",
			locationId, displayName, center));
		Print(string.Format("[SPT_WorldGarrison] Config local | id=%1 | spawn=%2 | despawn=%3 | cache=%4 | budget=%5 | CQB=%6 | patrol=%7 | battle=%8",
			locationId,
			GetLocationSpawnDistance(location),
			GetLocationDespawnDistance(location),
			IsLocationCachingEnabled(location),
			location.m_iMaxBudget,
			GetEffectiveCQBPrefabCount(location),
			GetEffectivePatrolPrefabCount(location),
			IsLocationBattleEnabled(location)));
		Print(string.Format("[SPT_WorldGarrison] Config de reforcos | id=%1 | battlePrefabs=%2 | convoyVehicles=%3 | spawnIntervalMs=%4",
			locationId,
			GetEffectiveBattlePrefabCount(location),
			location.m_Config.m_aBattleVehicles.Count(),
			GetLocationBattleSpawnIntervalMs(location)));
		return true;
	}

	//! Ativa/desativa a protecao de uma localizacao. Ao ativar, remove toda
	//! presenca hostil e invalida callbacks de spawn pendentes.
	bool SetLocationSafe(string locationId, bool safe)
	{
		if (!Replication.IsServer())
			return false;

		SPT_GarrisonLocation location = FindLocationById(locationId);
		if (!location)
			return false;

		if (location.m_bSafe == safe)
			return true;

		location.m_bSafe = safe;
		if (!safe)
		{
			location.m_bCleared = false;
			Print(string.Format("[SPT_WorldGarrison] Localizacao deixou de ser SAFE | id=%1", locationId));
			return true;
		}

		ClearLocationForSafe(location);
		Print(string.Format("[SPT_WorldGarrison] Localizacao marcada como SAFE | id=%1 | nome=%2",
			locationId, location.m_sName));
		return true;
	}

	bool IsLocationSafe(string locationId)
	{
		SPT_GarrisonLocation location = FindLocationById(locationId);
		return location && location.m_bSafe;
	}

	protected void ClearLocationForSafe(notnull SPT_GarrisonLocation location)
	{
		CancelPendingSpawns(location);
		CancelPendingBattleSpawns(location);
		location.m_Battle.m_bCancelled = true;
		location.m_Battle.m_bActive = false;
		RemoveBattleForces(location);

		BaseWorld world = GetGame().GetWorld();
		for (int i = location.m_aGroupIds.Count() - 1; i >= 0; i--)
		{
			SCR_AIGroup group = SCR_AIGroup.Cast(world.FindEntityByID(location.m_aGroupIds[i]));
			if (group)
				DeleteGroup(group);
		}

		location.m_aGroupIds.Clear();
		location.m_aGroupRecords.Clear();
		location.m_aCachedGroups.Clear();
		location.m_bHasCachedSnapshot = true;
		location.m_bActive = false;
		location.m_bSpawning = false;
		location.m_bUnavailable = false;
		location.m_bCleared = true;
		location.m_bFirstCasualtyTriggered = true;
		location.m_iInitialGarrisonManpower = 0;
		location.m_iPendingGroups = 0;
		location.m_iQueuedSpawnUnits = 0;
		location.m_iQueuedGarrisonUnits = 0;
	}

	//! Preenche um array com todos os IDs de localizacao registrados.
	void GetAllLocationIds(notnull array<string> outIds)
	{
		outIds.Clear();
		foreach (SPT_GarrisonLocation location : m_aLocations)
			outIds.Insert(location.m_sLocationId);
	}

	//! Retorna um snapshot publico do estado da guarnicao em uma localizacao.
	//! @param locationId O identificador estavel da localizacao
	//! @return true se a localizacao foi encontrada e os parametros out foram preenchidos
	bool GetLocationState(string locationId, out vector outCenter, out string outName, out int outDescriptorType,
		out int outGarrisonManpower, out int outTargetManpower, out int outPendingSpawns,
		out bool outCleared, out int outBudgetRemaining, out bool outBattleActive,
		out int outBattleWaveIndex, out int outBattleWaveCount)
	{
		outCenter = vector.Zero;
		outName = "";
		outDescriptorType = 0;
		outGarrisonManpower = 0;
		outTargetManpower = 0;
		outPendingSpawns = 0;
		outCleared = false;
		outBudgetRemaining = 0;
		outBattleActive = false;
		outBattleWaveIndex = -1;
		outBattleWaveCount = 0;

		SPT_GarrisonLocation location = FindLocationById(locationId);
		if (!location)
			return false;

		outCenter = location.m_vCenter;
		outName = location.m_sName;
		outDescriptorType = location.m_iDescriptorType;
		outGarrisonManpower = location.GetGarrisonManpower();
		outTargetManpower = location.m_iTargetManpower;
		outPendingSpawns = location.m_iQueuedSpawnUnits;
		outCleared = location.m_bCleared;
		outBudgetRemaining = location.m_iBudgetRemaining;
		outBattleActive = location.m_Battle.m_bActive;
		outBattleWaveIndex = location.m_Battle.m_iCurrentWave;
		outBattleWaveCount = location.m_Battle.m_aWaves.Count();
		return true;
	}

	//! Inicia o monitoramento de primeira baixa para uma localizacao.
	//! O snapshot do manpower inicial e adiado ate que todos os grupos
	//! terminem de spawnar, para nao capturar um valor parcial.
	//! Chamado pelo Warfare quando um ponto entra na frente de batalha.
	bool StartMonitoringLocation(string locationId)
	{
		SPT_GarrisonLocation location = FindLocationById(locationId);
		if (!location || location.m_bSafe)
			return false;

		location.m_bMonitoringEnabled = true;

		// Se o spawn ja terminou, captura o manpower agora.
		// Caso contrario, o snapshot sera feito em CompletePendingGroup
		// ou no proximo ciclo de MonitorGarrisonState.
		if (!location.m_bSpawning)
		{
			location.SnapInitialManpower();
			DebugLog(string.Format("Monitoramento iniciado | id=%1 | manpowerInicial=%2",
				locationId, location.m_iInitialGarrisonManpower));
		}
		else
		{
			DebugLog(string.Format("Monitoramento agendado (spawn em andamento) | id=%1",
				locationId));
		}
		return true;
	}

	//! Verifica todas as localizacoes com monitoramento ativo por eventos
	//! de estado da guarnicao (primeira baixa e guarnicao eliminada).
	//! Chamado periodicamente via CallLater.
	protected void MonitorGarrisonState()
	{
		foreach (SPT_GarrisonLocation location : m_aLocations)
		{
			if (location.m_bSafe)
				continue;
			if (!location.m_bActive)
				continue;

			// Apenas localizacoes com monitoramento explicito (via
			// StartMonitoringLocation) disparam eventos de baixa.
			// Isso impede que pontos LOCKED ou nao-inicializados
			// disparem OnGarrisonFirstCasualty prematuramente.
			if (!location.m_bMonitoringEnabled)
				continue;

			// Se o monitoramento foi ativado mas o snapshot ainda nao
			// foi capturado (spawn estava em andamento), captura agora
			// que a guarnicao esta ativa e estavel.
			if (location.m_iInitialGarrisonManpower <= 0)
			{
				int currentGarrison = location.GetGarrisonManpower();
				if (currentGarrison > 0 && !location.m_bSpawning)
				{
					location.SnapInitialManpower();
					Print(string.Format("[SPT_WorldGarrison] Monitoramento re-capturado | id=%1 | nome=%2 | manpowerInicial=%3",
						location.m_sLocationId,
						location.m_sName,
						location.m_iInitialGarrisonManpower));
				}
				continue;
			}

			// Verifica primeira baixa
			if (location.CheckFirstCasualty())
			{
				Print(string.Format("[SPT_WorldGarrison] Primeira baixa detectada | id=%1 | nome=%2 | manpowerAtual=%3/%4",
					location.m_sLocationId,
					location.m_sName,
					location.GetGarrisonManpower(),
					location.m_iInitialGarrisonManpower));
				m_OnGarrisonFirstCasualty.Invoke(location.m_sLocationId);
			}

			// Verifica guarnicao eliminada
			location.RefreshClearedState();
			
			// Log de diagnostico a cada ciclo para localizacoes monitoradas
			if (m_bDebug)
			{
				Print(string.Format("[SPT_WorldGarrison][DEBUG] MonitorGarrisonState | id=%1 | ativo=%2 | cached=%3 | ativoManpower=%4 | filaGarrison=%5 | limpo=%6 | inicialManpower=%7",
					location.m_sLocationId,
					location.m_bActive,
					location.m_bHasCachedSnapshot,
					location.GetActiveGarrisonManpower(),
					location.GetQueuedGarrisonRequests(),
					location.m_bCleared,
					location.m_iInitialGarrisonManpower));
			}

			if (location.m_bCleared)
			{
				Print(string.Format("[SPT_WorldGarrison] Guarnicao eliminada | id=%1 | nome=%2",
					location.m_sLocationId, location.m_sName));
				m_OnGarrisonCleared.Invoke(location.m_sLocationId);

				// Reseta o monitoramento para evitar disparos repetidos
				location.m_iInitialGarrisonManpower = 0;
				location.m_bFirstCasualtyTriggered = false;
			}
		}
	}

	protected SPT_GarrisonLocation FindNearestLocation(vector position)
	{
		SPT_GarrisonLocation best;
		float bestSq = float.MAX;
		foreach (SPT_GarrisonLocation location : m_aLocations)
		{
			float distanceSq = HorizontalDistanceSq(position, location.m_vCenter);
			if (distanceSq > bestSq)
				continue;
			bestSq = distanceSq;
			best = location;
		}
		return best;
	}

	protected void BuildBattleWaves(notnull SPT_GarrisonLocation location)
	{
		int available = location.m_iBudgetRemaining;
		if (location.HasUnlimitedBudget())
			available = GetLocationBattleWaveUnitsMax(location) * 3;

		int waveUnitsMin = GetLocationBattleWaveUnitsMin(location);
		int waveUnitsMax = GetLocationBattleWaveUnitsMax(location);
		if (waveUnitsMax < waveUnitsMin)
			waveUnitsMax = waveUnitsMin;

		static const array<ref array<float>> patterns = {
			{0.2, 0.3, 0.5},
			{0.3, 0.3, 0.4},
			{0.4, 0.4, 0.2},
			{0.5, 0.3, 0.2}
		};
		int patternIndex = Math.RandomInt(0, patterns.Count());
		int original = available;
		int waveIndex;

		while (available > 0)
		{
			float factor = 1.0;
			if (waveIndex < 3)
				factor = patterns[patternIndex][waveIndex];

			int waveUnits = Math.ClampInt(
				original * factor,
				waveUnitsMin,
				waveUnitsMax);
			waveUnits = Math.Min(waveUnits, available);
			if (available - waveUnits < waveUnitsMin)
				waveUnits = available;

			SPT_BattleWave wave = new SPT_BattleWave();
			wave.m_iUnitBudget = waveUnits;
			wave.m_eStrategy = SelectBattleStrategy(location);
			location.m_Battle.m_aWaves.Insert(wave);
			available -= waveUnits;
			waveIndex++;
		}
	}

	protected SPT_EDeploymentStrategy SelectBattleStrategy(notnull SPT_GarrisonLocation location)
	{
		float concentrated = Math.Max(0.0, GetLocationBattleConcentratedWeight(location));
		float spreaded = Math.Max(0.0, GetLocationBattleSpreadedWeight(location));
		float convoy = Math.Max(0.0, GetLocationBattleConvoyWeight(location));
		float total = concentrated + spreaded + convoy;
		if (total <= 0)
			return SPT_EDeploymentStrategy.SPREADED;

		float roll = Math.RandomFloat01() * total;
		if (roll < concentrated)
			return SPT_EDeploymentStrategy.CONCENTRATED;
		if (roll < concentrated + spreaded)
			return SPT_EDeploymentStrategy.SPREADED;
		return SPT_EDeploymentStrategy.CONVOY;
	}

	protected void UpdateBattles()
	{
		array<vector> playerPositions = {};
		CollectPlayerPositions(playerPositions);

		foreach (SPT_GarrisonLocation location : m_aLocations)
		{
			if (location.m_bSafe)
				continue;
			SPT_LocationBattleState battle = location.m_Battle;
			if (!battle.m_bActive)
				continue;

			if (battle.m_iTimerMs > 0)
				battle.m_iTimerMs = Math.Max(0, battle.m_iTimerMs - BATTLE_UPDATE_INTERVAL_MS);

			SPT_BattleWave currentWave;
			if (battle.m_iCurrentWave >= 0 && battle.m_iCurrentWave < battle.m_aWaves.Count())
				currentWave = battle.m_aWaves[battle.m_iCurrentWave];

			if (currentWave && !currentWave.m_bDeploymentFinished)
				continue;

			int aliveBattle = CountAliveBattleUnits(location);
			bool hasNext = battle.m_iCurrentWave + 1 < battle.m_aWaves.Count();
			if (!hasNext)
			{
				if (aliveBattle < 1 && CountQueuedBattleRequests(location) < 1)
				FinishLocationBattle(location);
				continue;
			}

			bool deploy = battle.m_iCurrentWave < 0 && battle.m_iTimerMs == 0;
			if (!deploy && battle.m_iTimerMs == 0)
				deploy = true;
			if (!deploy && currentWave && currentWave.m_iDeployedUnits > 0)
				deploy = aliveBattle < currentWave.m_iDeployedUnits * GetLocationBattleWaveAliveThreshold(location);
			if (!deploy || playerPositions.IsEmpty())
				continue;

			float activationDistance = GetLocationSpawnDistance(location);
			float activationSq = activationDistance * activationDistance;
			if (GetNearestPlayerDistanceSq(location.m_vCenter, playerPositions) > activationSq)
				continue;

			DeployNextBattleWave(location);
		}
	}

	protected void FinishLocationBattle(notnull SPT_GarrisonLocation location)
	{
		location.m_Battle.m_bActive = false;
		Print(string.Format("[SPT_WorldGarrison] Batalha encerrada | local=%1 | ondas=%2 | reserva=%3",
			location.m_sName,
			location.m_Battle.m_aWaves.Count(),
			location.m_iBudgetRemaining));
		m_OnBattleEnded.Invoke(location.m_sLocationId);
	}

	protected int CountAliveBattleUnits(notnull SPT_GarrisonLocation location)
	{
		int total;
		BaseWorld world = GetGame().GetWorld();
		foreach (int i, EntityID groupId : location.m_aGroupIds)
		{
			if (i >= location.m_aGroupRecords.Count() || !location.m_aGroupRecords[i].m_bIsBattleGroup)
				continue;
			SCR_AIGroup group = SCR_AIGroup.Cast(world.FindEntityByID(groupId));
			if (group)
				total += group.GetAgentsCount() + group.GetSpawnQueueSize();
		}
		return total;
	}

	protected int CountQueuedBattleRequests(notnull SPT_GarrisonLocation location)
	{
		int total;
		foreach (SPT_GarrisonSpawnRequest req : m_aPendingSpawns)
		{
			if (req && req.m_Location == location && req.m_bIsBattleSpawn)
				total++;
		}
		return total;
	}

	protected void DeployNextBattleWave(notnull SPT_GarrisonLocation location)
	{
		SPT_LocationBattleState battle = location.m_Battle;
		int nextIndex = battle.m_iCurrentWave + 1;
		if (nextIndex >= battle.m_aWaves.Count())
			return;

		SPT_BattleWave wave = battle.m_aWaves[nextIndex];
		battle.m_iCurrentWave = nextIndex;
		battle.m_iTimerMs = -1;

		if (wave.m_eStrategy == SPT_EDeploymentStrategy.CONVOY && !CanDeployConvoy(location))
		{
			Print(string.Format("[SPT_WorldGarrison] CONVOY indisponivel; usando SPREADED | local=%1",
				location.m_sName), LogLevel.WARNING);
			wave.m_eStrategy = SPT_EDeploymentStrategy.SPREADED;
		}

		array<vector> positions = {};
		int estimatedGroups = Math.Max(1, (wave.m_iUnitBudget + 7) / 8);
		BuildBattleSpawnPositions(location, wave.m_eStrategy, estimatedGroups, positions);
		if (positions.IsEmpty())
		{
			wave.m_bDeploymentFinished = true;
			ScheduleNextBattleWave(location);
			return;
		}

		if (wave.m_eStrategy == SPT_EDeploymentStrategy.CONVOY)
			SpawnConvoyVehicles(location, wave, positions);

		array<ResourceName> prefabs = {};
		GetLocationBattlePrefabs(location, prefabs);
		if (prefabs.IsEmpty())
		{
			Print(string.Format("[SPT_WorldGarrison] Nenhum prefab de reforco configurado para %1.", location.m_sName), LogLevel.WARNING);
			wave.m_bDeploymentFinished = true;
			ScheduleNextBattleWave(location);
			return;
		}

		array<ref SPT_GarrisonSpawnRequest> requests = {};
		int plannedUnits = wave.m_iDeployedUnits;
		for (int i = 0; i < positions.Count() && plannedUnits < wave.m_iUnitBudget; i++)
		{
			int requestUnits = Math.Min(8, wave.m_iUnitBudget - plannedUnits);
			SPT_GarrisonSpawnRequest req = new SPT_GarrisonSpawnRequest();
			req.m_Location = location;
			req.m_rGroupPrefab = prefabs[i % prefabs.Count()];
			req.m_vSpawnPosition = positions[i];
			req.m_vCityCenter = location.m_vCenter;
			req.m_bIsCQB = false;
			req.m_bIsBattleSpawn = true;
			req.m_BattleWave = wave;
			req.m_iMaxDeployableUnits = requestUnits;
			req.m_iExpectedUnits = 1;
			requests.Insert(req);
			plannedUnits += requestUnits;
		}

		wave.m_iPendingRequests = requests.Count();
		if (requests.IsEmpty())
		{
			wave.m_bDeploymentFinished = true;
			ScheduleNextBattleWave(location);
			return;
		}

		EnqueueSpawns(requests);
		Print(string.Format("[SPT_WorldGarrison] Onda enfileirada | local=%1 | indice=%2 | estrategia=%3 | unidadesPlanejadas=%4 | grupos=%5",
			location.m_sName,
			nextIndex,
			SCR_Enum.GetEnumName(SPT_EDeploymentStrategy, wave.m_eStrategy),
			wave.m_iUnitBudget,
			requests.Count()));
		m_OnBattleWaveDeployed.Invoke(location.m_sLocationId, nextIndex, wave.m_iUnitBudget);
	}

	protected void SpawnConvoyVehicles(
		notnull SPT_GarrisonLocation location,
		notnull SPT_BattleWave wave,
		notnull array<vector> positions)
	{
		if (!location.m_Config.m_aBattleVehicles || location.m_Config.m_aBattleVehicles.IsEmpty())
			return;

		int vehicleLimit = Math.Min(location.m_Config.m_aBattleVehicles.Count(), positions.Count());
		for (int i = vehicleLimit - 1; i >= 0; i--)
		{
			int waveRemaining = wave.m_iUnitBudget - wave.m_iDeployedUnits;
			int budgetLimit = location.GetDeploymentLimit(waveRemaining);
			if (budgetLimit < 1)
				break;

			SPT_BattleVehicleConfig config = location.m_Config.m_aBattleVehicles[i % location.m_Config.m_aBattleVehicles.Count()];
			float roadYaw = (location.m_vCenter - positions[i]).ToYaw();
			if (positions.Count() > 1)
			{
				int adjacent = i + 1;
				if (adjacent >= positions.Count())
					adjacent = i - 1;
				roadYaw = (positions[i] - positions[adjacent]).ToYaw();
			}
			int crewSpawned;
			IEntity vehicle = SpawnCrewedBattleVehicle(
				location,
				config,
				positions[i],
				roadYaw,
				budgetLimit,
				crewSpawned);
			if (!vehicle)
				continue;

			location.ConsumeBudget(crewSpawned);
			wave.m_iDeployedUnits += crewSpawned;
			location.m_Battle.m_aVehicleIds.Insert(vehicle.GetID());
			positions.Remove(i);
		}
	}

	protected IEntity SpawnCrewedBattleVehicle(
		notnull SPT_GarrisonLocation location,
		notnull SPT_BattleVehicleConfig config,
		vector position,
		float yaw,
		int maxCrew,
		out int crewSpawned)
	{
		crewSpawned = 0;
		if (config.m_rVehiclePrefab.IsEmpty() || config.m_rCrewGroupPrefab.IsEmpty())
			return null;

		Resource vehicleResource = Resource.Load(config.m_rVehiclePrefab);
		Resource crewGroupResource = Resource.Load(config.m_rCrewGroupPrefab);
		if (!vehicleResource || !crewGroupResource)
			return null;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		Math3D.AnglesToMatrix(Vector(yaw, 0, 0), params.Transform);
		params.Transform[3] = position;
		IEntity vehicleEntity = GetGame().SpawnEntityPrefab(
			vehicleResource,
			GetGame().GetWorld(),
			params);
		Vehicle vehicle = Vehicle.Cast(vehicleEntity);
		if (!vehicle)
		{
			if (vehicleEntity)
				SCR_EntityHelper.DeleteEntityAndChildren(vehicleEntity);
			return null;
		}

		BaseCompartmentManagerComponent compartmentManager = BaseCompartmentManagerComponent.Cast(
			vehicle.FindComponent(BaseCompartmentManagerComponent));
		if (!compartmentManager)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
			return null;
		}

		array<ResourceName> crewPrefabs = {};
		if (!GetGroupMemberPrefabs(crewGroupResource, crewPrefabs))
		{
			SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
			return null;
		}

		array<BaseCompartmentSlot> compartments = {};
		compartmentManager.GetCompartments(compartments);
		AIGroup crewGroup;
		int crewLimit = Math.Min(Math.Min(compartments.Count(), crewPrefabs.Count()), maxCrew);
		for (int i = 0; i < crewLimit; i++)
		{
			if (!compartments[i])
				continue;
			IEntity character = compartments[i].SpawnCharacterInCompartment(
				crewPrefabs[i],
				crewGroup,
				config.m_rCrewGroupPrefab);
			if (character)
				crewSpawned++;
		}

		SCR_AIGroup group = SCR_AIGroup.Cast(crewGroup);
		if (crewSpawned < 1 || !group)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
			return null;
		}

		location.m_aGroupIds.Insert(group.GetID());
		SPT_GroupSpawnRecord record = new SPT_GroupSpawnRecord();
		record.m_rGroupPrefab = config.m_rCrewGroupPrefab;
		record.m_vFallbackPosition = position;
		record.m_bIsCQB = false;
		record.m_bIsBattleGroup = true;
		record.m_rVehiclePrefab = config.m_rVehiclePrefab;
		location.m_aGroupRecords.Insert(record);
		location.m_bActive = true;

		SPT_AIGarrisonHelper.PatrolGroup(
			group,
			location.m_vCenter,
			GetLocationPatrolRadius(location),
			GetLocationPatrolWaypointPrefab(location),
			GetLocationPatrolFormation(location),
			GetLocationBattleMovementType(location));
		return vehicle;
	}

	protected SCR_AIGroup SpawnCachedBattleVehicle(
		notnull SPT_GarrisonLocation location,
		notnull SPT_StreamableGarrisonGroup cached,
		out int crewSpawned)
	{
		crewSpawned = 0;
		Resource vehicleResource = Resource.Load(cached.m_rVehiclePrefab);
		if (!vehicleResource || cached.m_aAliveMembers.IsEmpty())
			return null;

		vector direction = location.m_vCenter - cached.m_vPosition;
		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		Math3D.AnglesToMatrix(Vector(direction.ToYaw(), 0, 0), params.Transform);
		params.Transform[3] = cached.m_vPosition;
		IEntity vehicleEntity = GetGame().SpawnEntityPrefab(
			vehicleResource,
			GetGame().GetWorld(),
			params);
		Vehicle vehicle = Vehicle.Cast(vehicleEntity);
		if (!vehicle)
		{
			if (vehicleEntity)
				SCR_EntityHelper.DeleteEntityAndChildren(vehicleEntity);
			return null;
		}

		BaseCompartmentManagerComponent manager = BaseCompartmentManagerComponent.Cast(
			vehicle.FindComponent(BaseCompartmentManagerComponent));
		if (!manager)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
			return null;
		}

		array<BaseCompartmentSlot> compartments = {};
		manager.GetCompartments(compartments);
		AIGroup crewGroup;
		int limit = Math.Min(compartments.Count(), cached.m_aAliveMembers.Count());
		for (int i = 0; i < limit; i++)
		{
			if (!compartments[i])
				continue;
			IEntity character = compartments[i].SpawnCharacterInCompartment(
				cached.m_aAliveMembers[i],
				crewGroup,
				cached.m_rGroupPrefab);
			if (character)
				crewSpawned++;
		}

		SCR_AIGroup group = SCR_AIGroup.Cast(crewGroup);
		if (!group || crewSpawned != cached.m_aAliveMembers.Count())
		{
			SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
			return null;
		}

		location.m_Battle.m_aVehicleIds.Insert(vehicle.GetID());
		SPT_AIGarrisonHelper.PatrolGroup(
			group,
			location.m_vCenter,
			GetLocationPatrolRadius(location),
			GetLocationPatrolWaypointPrefab(location),
			GetLocationPatrolFormation(location),
			GetLocationBattleMovementType(location));
		return group;
	}

	protected bool GetGroupMemberPrefabs(Resource groupResource, out array<ResourceName> members)
	{
		members.Clear();
		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		Math3D.MatrixIdentity4(params.Transform);
		params.Transform[3] = Vector(0, -1000, 0);

		SCR_AIGroup.IgnoreSpawning(true);
		IEntity entity = GetGame().SpawnEntityPrefab(groupResource, GetGame().GetWorld(), params);
		SCR_AIGroup.IgnoreSpawning(false);
		SCR_AIGroup group = SCR_AIGroup.Cast(entity);
		if (!group)
		{
			if (entity)
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
			return false;
		}

		foreach (ResourceName prefab : group.m_aUnitPrefabSlots)
			if (!prefab.IsEmpty())
				members.Insert(prefab);
		SCR_EntityHelper.DeleteEntityAndChildren(group);
		return !members.IsEmpty();
	}

	protected void BuildBattleSpawnPositions(
		notnull SPT_GarrisonLocation location,
		SPT_EDeploymentStrategy strategy,
		int count,
		out array<vector> positions)
	{
		positions.Clear();
		float minDistance = GetLocationBattleSpawnDistanceMin(location);
		float maxDistance = GetLocationBattleSpawnDistanceMax(location);
		if (maxDistance < minDistance)
			maxDistance = minDistance;

		if (strategy == SPT_EDeploymentStrategy.CONVOY)
		{
			if (SPT_RoadNetworkService.FindConvoyPositions(
				location.m_vCenter,
				minDistance,
				maxDistance,
				count,
				20.0,
				positions))
				return;
			strategy = SPT_EDeploymentStrategy.SPREADED;
		}

		float baseAngle = Math.RandomFloat01() * Math.PI * 2.0;
		float concentratedAngle = baseAngle;
		for (int i = 0; i < count; i++)
		{
			float angle = concentratedAngle;
			if (strategy == SPT_EDeploymentStrategy.SPREADED)
				angle = baseAngle + i * Math.PI * 2.0 / count;
			float distance = minDistance
				+ Math.RandomFloat01() * (maxDistance - minDistance);
			vector position = location.m_vCenter + Vector(
				Math.Cos(angle) * distance,
				0,
				Math.Sin(angle) * distance);
			position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]) + 0.2;
			positions.Insert(position);
		}
	}

	protected bool CanDeployConvoy(notnull SPT_GarrisonLocation location)
	{
		return location.m_Config.m_aBattleVehicles
			&& !location.m_Config.m_aBattleVehicles.IsEmpty()
			&& SPT_RoadNetworkService.IsAvailable();
	}

	protected void ScheduleNextBattleWave(notnull SPT_GarrisonLocation location)
	{
		int delayMin = GetLocationBattleWaveDelayMinMs(location);
		int delayMax = GetLocationBattleWaveDelayMaxMs(location);
		if (delayMax < delayMin)
			delayMax = delayMin;
		location.m_Battle.m_iTimerMs = RandomRangeInt(delayMin, delayMax);
		int nextIndex = location.m_Battle.m_iCurrentWave + 1;
		m_OnBattleWaveScheduled.Invoke(location.m_sLocationId, nextIndex);
	}

	protected int RandomRangeInt(int minimum, int maximum)
	{
		if (maximum <= minimum)
			return minimum;
		return Math.RandomInt(minimum, maximum + 1);
	}

	protected void SpawnLocation(notnull SPT_GarrisonLocation location)
	{
		if (location.m_bSafe)
			return;

		PruneMissingGroupIds(location);

		// -----------------------------------------------------------------
		// RESTAURACAO DO CACHE: Se o caching estiver ativo e esta
		// localizacao tiver sobreviventes salvos de um stream-out anterior,
		// respawna apenas esses sobreviventes em vez de gerar grupos novos.
		// -----------------------------------------------------------------
		if (IsLocationCachingEnabled(location) && location.m_bHasCachedSnapshot)
		{
			if (location.m_aCachedGroups.IsEmpty())
			{
				location.RefreshClearedState();
				DebugLog(string.Format("Spawn de guarnicao ignorado para %1: snapshot completamente eliminado.",
					location.m_sName));
				return;
			}

			DebugLog(string.Format("Restaurando do cache | nome=%1 | gruposCacheados=%2 | budgetRestante=%3",
				location.m_sName, location.m_aCachedGroups.Count(), location.m_iBudgetRemaining));
			SpawnLocationFromCache(location);
			return;
		}

		array<ResourceName> cqbPrefabs = {};
		array<ResourceName> patrolPrefabs = {};
		GetLocationCQBPrefabs(location, cqbPrefabs);
		GetLocationPatrolPrefabs(location, patrolPrefabs);

		DebugLog(string.Format("SpawnLocation iniciado | nome=%1 | centro=%2 | gruposCQB=%3 | gruposPatrulha=%4",
			location.m_sName, location.m_vCenter, cqbPrefabs.Count(), patrolPrefabs.Count()));

		if (cqbPrefabs.Count() + patrolPrefabs.Count() < 1)
		{
			Print(string.Format("[SPT_WorldGarrison] Nenhum grupo CQB ou de patrulha configurado para %1.", location.m_sName), LogLevel.ERROR);
			return;
		}

		array<vector> buildings = {};
		float buildingSearchRadius = GetLocationBuildingSearchRadius(location);
		SPT_GarrisonDetector.CollectBuildingCenters(
			GetGame().GetWorld(),
			location.m_vCenter,
			buildingSearchRadius,
			buildings
		);
		DebugLog(string.Format("Detector de construcoes retornou %1 centros estruturais em %2m.",
			buildings.Count(), buildingSearchRadius));

		// Coleta construcoes pontuadas para distribuicao CQB de qualidade.
		// Construcoes maiores, com varios andares, janelas e torres recebem
		// prioridade; toneis, props e garagens minusculas sao filtrados.
		ref array<ref SPT_BuildingCQBRecord> cqbBuildings = new array<ref SPT_BuildingCQBRecord>();
		SPT_GarrisonDetector.CollectCQBBuildingCenters(
			GetGame().GetWorld(),
			location.m_vCenter,
			buildingSearchRadius,
			cqbBuildings
		);
		DebugLog(string.Format("Detector CQB retornou %1 construcoes pontuadas (de %2 total).",
			cqbBuildings.Count(), buildings.Count()));

		if (buildings.IsEmpty())
		{
			Print(string.Format("[SPT_WorldGarrison] Nenhuma construcao utilizavel ao redor de %1 em %2",
				location.m_sName, location.m_vCenter), LogLevel.WARNING);
			if (patrolPrefabs.Count() < 1)
			{
				location.m_bUnavailable = true;
				return;
			}
		}

		ShufflePositions(buildings);

		location.m_bSpawning = true;
		location.m_iPendingGroups = 0;
		location.m_iSuccessfulGroups = 0;
		location.m_iDesiredUnits = 0;
		location.m_iTargetManpower = 0;
		int buildingIndex = 0;
		int cqbBuildingIndex = 0;
		int patrolSpawnIndex = 0;
		array<vector> patrolSpawnPositions = {};

		array<ResourceName> groupPrefabs = {};
		array<bool> patrolFlags = {};
		foreach (ResourceName cqbPrefab : cqbPrefabs)
		{
			groupPrefabs.Insert(cqbPrefab);
			patrolFlags.Insert(false);
		}
		foreach (ResourceName patrolPrefab : patrolPrefabs)
		{
			groupPrefabs.Insert(patrolPrefab);
			patrolFlags.Insert(true);
		}
		DebugLog(string.Format("Listas preparadas para spawn | CQB=%1 | patrulha=%2 | total=%3",
			cqbPrefabs.Count(), patrolPrefabs.Count(), groupPrefabs.Count()));

		// Prepara requisicoes de spawn e enfileira para processamento assincrono.
		// O spawn real acontece em ProcessSpawnQueue, um grupo por tick.
		ref array<ref SPT_GarrisonSpawnRequest> requests = new array<ref SPT_GarrisonSpawnRequest>();

		foreach (int prefabIndex, ResourceName groupPrefab : groupPrefabs)
		{
			bool patrolGroup = patrolFlags[prefabIndex];
			vector spawnCenter;
			if (patrolGroup)
			{
				if (!FindOutdoorPatrolSpawn(
					location.m_vCenter,
					GetLocationPatrolRadius(location),
					buildings,
					patrolSpawnPositions,
					patrolSpawnIndex,
					spawnCenter))
				{
					Print(string.Format("[SPT_WorldGarrison] Nenhum ponto externo encontrado para grupo de patrulha | indice=%1 | centroCidade=%2",
						prefabIndex, location.m_vCenter), LogLevel.ERROR);
					continue;
				}
				patrolSpawnPositions.Insert(spawnCenter);
				patrolSpawnIndex++;
			}
			else if (!cqbBuildings.IsEmpty())
			{
				// Distribui grupos CQB priorizando as melhores construcoes:
				// o primeiro grupo vai para a melhor construcao, o segundo
				// para a segunda melhor, etc. Quando todas as construcoes
				// receberam um grupo, recomeca pelas melhores (round-robin).
				int selectedBuilding = cqbBuildingIndex % cqbBuildings.Count();
				spawnCenter = cqbBuildings[selectedBuilding].m_vCenter;
				cqbBuildingIndex++;
			}
			else if (!buildings.IsEmpty())
			{
				// Fallback: se nao houver construcoes pontuadas para CQB
				// (raro), usa as construcoes normais embaralhadas
				int selectedBuilding = buildingIndex % buildings.Count();
				spawnCenter = buildings[selectedBuilding];
				buildingIndex++;
			}
			else
			{
				Print(string.Format("[SPT_WorldGarrison] Grupo CQB ignorado por falta de construcao | indice=%1 | prefab=%2",
					prefabIndex, groupPrefab), LogLevel.WARNING);
				continue;
			}

			if (groupPrefab.IsEmpty())
			{
				Print(string.Format("[SPT_WorldGarrison] Entrada vazia na lista de grupos no indice %1.", prefabIndex), LogLevel.WARNING);
				continue;
			}

			Resource groupResource = Resource.Load(groupPrefab);
			if (!groupResource)
			{
				Print(string.Format("[SPT_WorldGarrison] Nao foi possivel carregar o grupo no indice %1: %2",
					prefabIndex, groupPrefab), LogLevel.ERROR);
				continue;
			}

			int maxDeployableUnits = -1;
			if (!location.HasUnlimitedBudget())
				maxDeployableUnits = location.m_iBudgetRemaining;

			SPT_GarrisonSpawnRequest req = new SPT_GarrisonSpawnRequest();
			req.m_Location = location;
			req.m_rGroupPrefab = groupPrefab;
			req.m_vSpawnPosition = spawnCenter;
			req.m_vCityCenter = location.m_vCenter;
			req.m_bIsCQB = !patrolGroup;
			req.m_bIsFromCache = false;
			req.m_iMaxDeployableUnits = maxDeployableUnits;
			req.m_iGeneration = location.m_iSpawnGeneration;
			req.m_iExpectedUnits = 1;
			requests.Insert(req);

			DebugLog(string.Format("Spawn enfileirado | tipoPatrulha=%1 | indice=%2 | prefab=%3 | centroSpawn=%4 | limiteBudget=%5",
				patrolGroup, prefabIndex, groupPrefab, spawnCenter, maxDeployableUnits));
		}

		if (requests.IsEmpty())
		{
			location.m_bSpawning = false;
			location.RefreshClearedState();
			Print(string.Format("[SPT_WorldGarrison] Falha ao criar grupos para %1.", location.m_sName), LogLevel.ERROR);
			return;
		}

		EnqueueSpawns(requests);
		Print(string.Format("[SPT_WorldGarrison] Guarnicao inicial enfileirada em %1 | grupos=%2 | intervaloMs=%3; aguardando agentes",
			location.m_sName, requests.Count(), GetLocationSpawnIntervalMs(location)));
	}

	//! Restaura grupos cacheados (que sofreram stream-out) em vez de spawnar
	//! grupos novos. Apenas os sobreviventes que estavam vivos no momento do
	//! stream-out sao respawnados. Grupos totalmente eliminados sao ignorados.
	protected void SpawnLocationFromCache(notnull SPT_GarrisonLocation location)
	{
		location.m_bSpawning = true;
		location.m_iPendingGroups = 0;
		location.m_iSuccessfulGroups = 0;
		location.m_iDesiredUnits = 0;
		array<ref SPT_StreamableGarrisonGroup> failedCachedGroups = {};
		int restoredGroups;

		foreach (int cacheIndex, SPT_StreamableGarrisonGroup cached : location.m_aCachedGroups)
		{
			if (cached.GetCachedManpower() < 1)
			{
				DebugLog(string.Format("Grupo cacheado ignorado (eliminado) | indice=%1", cacheIndex));
				continue;
			}

			Resource groupResource = Resource.Load(cached.m_rGroupPrefab);
			if (!groupResource)
			{
				Print(string.Format("[SPT_WorldGarrison] Nao foi possivel carregar grupo cacheado no indice %1: %2",
					cacheIndex, cached.m_rGroupPrefab), LogLevel.ERROR);
				failedCachedGroups.Insert(cached);
				continue;
			}

			int spawnedUnits;
			bool isCQB = cached.m_bIsCQB;
			DebugLog(string.Format("Spawnando grupo cacheado | indice=%1 | prefab=%2 | isCQB=%3 | vivos=%4 | posicao=%5",
				cacheIndex, cached.m_rGroupPrefab, isCQB, cached.GetCachedManpower(), cached.m_vPosition));

			SCR_AIGroup group;
			if (!cached.m_rVehiclePrefab.IsEmpty())
				group = SpawnCachedBattleVehicle(location, cached, spawnedUnits);
			else
			{
				float cachedMemberSpacing;
				if (!isCQB)
					cachedMemberSpacing = GetPatrolMemberSpacing(GetLocationPatrolRadius(location));
				group = SpawnGroupWithMembers(
					groupResource,
					cached.m_vPosition,
					cached.m_aAliveMembers,
					cachedMemberSpacing,
					spawnedUnits);
			}
			if (!group)
			{
				Print(string.Format("[SPT_WorldGarrison] Falha ao spawnar grupo cacheado indice %1: %2",
					cacheIndex, cached.m_rGroupPrefab), LogLevel.ERROR);
				failedCachedGroups.Insert(cached);
				continue;
			}

			int cachedManpower = cached.GetCachedManpower();
			if (spawnedUnits != cachedManpower)
			{
				Print(string.Format("[SPT_WorldGarrison] Restauracao parcial recusada | indice=%1 | esperado=%2 | criado=%3 | prefab=%4",
					cacheIndex, cachedManpower, spawnedUnits, cached.m_rGroupPrefab), LogLevel.ERROR);

				if (DeleteGroup(group))
				{
					failedCachedGroups.Insert(cached);
					continue;
				}

				if (spawnedUnits < 1)
				{
					// Preserva o snapshot original mesmo que o ciclo de vida
					// editavel recuse temporariamente a delecao de um grupo vazio.
					failedCachedGroups.Insert(cached);
					GetGame().GetCallqueue().CallLater(
						RetryDeleteOrphanGroup,
						250,
						false,
						group,
						20
					);
					continue;
				}

				Print(string.Format("[SPT_WorldGarrison] Rollback recusado; mantendo grupo parcialmente restaurado para evitar duplicacao | indice=%1 | membros=%2",
					cacheIndex, spawnedUnits), LogLevel.WARNING);
			}

			location.m_aGroupIds.Insert(group.GetID());

			SPT_GroupSpawnRecord record = new SPT_GroupSpawnRecord();
			record.m_rGroupPrefab = cached.m_rGroupPrefab;
			record.m_vFallbackPosition = cached.m_vPosition;
			record.m_bIsCQB = cached.m_bIsCQB;
			record.m_bIsBattleGroup = cached.m_bIsBattleGroup;
			record.m_rVehiclePrefab = cached.m_rVehiclePrefab;
			location.m_aGroupRecords.Insert(record);

			restoredGroups++;
			location.m_iPendingGroups++;
			location.m_iDesiredUnits += spawnedUnits;
			DebugLog(string.Format("Grupo cacheado criado | id=%1 | membros=%2 | centro=%3",
				group.GetID(), spawnedUnits, cached.m_vPosition));

			GetGame().GetCallqueue().CallLater(
				WaitForGroupMembers,
				250,
				false,
				location,
				group,
				cached.m_vPosition,
				location.m_vCenter,
				!cached.m_bIsCQB,
				cached.m_bIsBattleGroup,
				spawnedUnits,
				20
			);
		}

		location.m_aCachedGroups.Clear();
		foreach (SPT_StreamableGarrisonGroup failedCached : failedCachedGroups)
			location.m_aCachedGroups.Insert(failedCached);
		location.m_bHasCachedSnapshot = !location.m_aCachedGroups.IsEmpty();

		if (location.m_aGroupIds.IsEmpty())
		{
			location.m_bSpawning = false;
			location.RefreshClearedState();
			if (location.m_bCleared)
			{
				Print(string.Format("[SPT_WorldGarrison] Local limpo: %1 | manpower=0 | budgetRestante=0",
					location.m_sName));
			}
			else
			{
				Print(string.Format("[SPT_WorldGarrison] Falha ao ativar %1 | cachePreservado=%2 | budgetRestante=%3",
					location.m_sName, location.m_aCachedGroups.Count(), location.m_iBudgetRemaining), LogLevel.ERROR);
			}
			return;
		}

		Print(string.Format("[SPT_WorldGarrison] Stream-in concluido em %1 | gruposRestaurados=%2 | unidadesAtivas=%3/%4 | reservaBatalha=%5 | cachePreservado=%6",
			location.m_sName,
			restoredGroups,
			location.GetTotalManpower(),
			location.m_iTargetManpower,
			location.m_iBudgetRemaining,
			location.m_aCachedGroups.Count()));
	}

	//-----------------------------------------------------------------------
	// FILA DE SPAWN ASSINCRONO (FASE 4)
	//-----------------------------------------------------------------------

	//! Adiciona uma lista de requisicoes de spawn a fila e inicia o
	//! processamento tick-a-tick se ainda nao estiver rodando.
	void EnqueueSpawns(notnull array<ref SPT_GarrisonSpawnRequest> requests)
	{
		if (!requests || requests.IsEmpty())
			return;

		foreach (int i, SPT_GarrisonSpawnRequest req : requests)
		{
			if (!req)
				continue;
			req.m_iGeneration = req.m_Location.m_iSpawnGeneration;
			req.m_Location.m_iQueuedSpawnUnits += req.m_iExpectedUnits;
			if (!req.m_bIsBattleSpawn)
				req.m_Location.m_iQueuedGarrisonUnits += req.m_iExpectedUnits;
			m_aPendingSpawns.Insert(req);
		}

		if (!m_bProcessingSpawnQueue)
		{
			m_bProcessingSpawnQueue = true;
			int intervalMs = 1;
			if (!m_aPendingSpawns.IsEmpty())
				intervalMs = GetSpawnRequestIntervalMs(m_aPendingSpawns[0]);
			if (intervalMs < 1)
				intervalMs = 1;
			GetGame().GetCallqueue().CallLater(ProcessSpawnQueue, intervalMs, false);
		}
	}

	protected int GetSpawnRequestIntervalMs(SPT_GarrisonSpawnRequest request)
	{
		if (!request || !request.m_Location)
			return 1;
		if (request.m_bIsBattleSpawn)
			return GetLocationBattleSpawnIntervalMs(request.m_Location);
		return GetLocationSpawnIntervalMs(request.m_Location);
	}

	//! Processa um unico spawn da fila por invocacao. Reagenda-se via
	//! CallLater enquanto houver itens pendentes.
	protected void ProcessSpawnQueue()
	{
		if (!m_aPendingSpawns || m_aPendingSpawns.IsEmpty())
		{
			m_bProcessingSpawnQueue = false;
			return;
		}

		SPT_GarrisonSpawnRequest req = m_aPendingSpawns[0];
		m_aPendingSpawns.Remove(0);

		if (req)
			DoSpawnSingle(req);

		if (!m_aPendingSpawns || m_aPendingSpawns.IsEmpty())
		{
			m_bProcessingSpawnQueue = false;
			return;
		}

		int intervalMs = 1;
		if (!m_aPendingSpawns.IsEmpty())
			intervalMs = GetSpawnRequestIntervalMs(m_aPendingSpawns[0]);
		if (intervalMs < 1)
			intervalMs = 1;
		GetGame().GetCallqueue().CallLater(ProcessSpawnQueue, intervalMs, false);
	}

	//! Invalida e remove todos os spawns ainda nao materializados desta
	//! localizacao. A geracao tambem protege contra callbacks ja retirados da
	//! fila no mesmo frame.
	protected void CancelPendingSpawns(notnull SPT_GarrisonLocation location)
	{
		location.m_iSpawnGeneration++;

		for (int i = m_aPendingSpawns.Count() - 1; i >= 0; i--)
		{
			SPT_GarrisonSpawnRequest req = m_aPendingSpawns[i];
			if (!req || req.m_Location != location)
				continue;

			location.m_iQueuedSpawnUnits = Math.Max(
				0,
				location.m_iQueuedSpawnUnits - req.m_iExpectedUnits);
			if (!req.m_bIsBattleSpawn)
			{
				location.m_iQueuedGarrisonUnits = Math.Max(
					0,
					location.m_iQueuedGarrisonUnits - req.m_iExpectedUnits);
			}
			else if (req.m_BattleWave)
			{
				req.m_BattleWave.m_iPendingRequests = Math.Max(
					0,
					req.m_BattleWave.m_iPendingRequests - 1);
				if (req.m_BattleWave.m_iPendingRequests == 0)
					req.m_BattleWave.m_bDeploymentFinished = true;
			}
			m_aPendingSpawns.Remove(i);
		}

		location.RefreshClearedState();
	}

	protected void CancelPendingBattleSpawns(notnull SPT_GarrisonLocation location)
	{
		for (int i = m_aPendingSpawns.Count() - 1; i >= 0; i--)
		{
			SPT_GarrisonSpawnRequest req = m_aPendingSpawns[i];
			if (!req || req.m_Location != location || !req.m_bIsBattleSpawn)
				continue;

			location.m_iQueuedSpawnUnits = Math.Max(
				0,
				location.m_iQueuedSpawnUnits - req.m_iExpectedUnits);
			if (req.m_BattleWave)
				req.m_BattleWave.m_iPendingRequests = Math.Max(0, req.m_BattleWave.m_iPendingRequests - 1);
			m_aPendingSpawns.Remove(i);
		}
	}

	//! Executa o spawn de um unico grupo a partir de uma requisicao
	//! enfileirada. Consome budget, registra metadados e agenda o callback
	//! WaitForGroupMembers.
	protected void DoSpawnSingle(notnull SPT_GarrisonSpawnRequest req)
	{
		SPT_GarrisonLocation location = req.m_Location;
		if (!location)
			return;

		location.m_iQueuedSpawnUnits = Math.Max(
			0,
			location.m_iQueuedSpawnUnits - req.m_iExpectedUnits);
		if (!req.m_bIsBattleSpawn)
		{
			location.m_iQueuedGarrisonUnits = Math.Max(
				0,
				location.m_iQueuedGarrisonUnits - req.m_iExpectedUnits);
		}

		if (location.m_bSafe)
		{
			location.RefreshClearedState();
			return;
		}

		if (req.m_iGeneration != location.m_iSpawnGeneration)
		{
			DebugLog(string.Format("Spawn obsoleto descartado | local=%1 | geracao=%2/%3",
				location.m_sName, req.m_iGeneration, location.m_iSpawnGeneration));
			location.RefreshClearedState();
			return;
		}

		if (req.m_bIsBattleSpawn && req.m_BattleWave)
		{
			req.m_BattleWave.m_iPendingRequests = Math.Max(
				0,
				req.m_BattleWave.m_iPendingRequests - 1);
			if (req.m_BattleWave.m_iPendingRequests == 0)
			{
				req.m_BattleWave.m_bDeploymentFinished = true;
				ScheduleNextBattleWave(location);
			}
		}

		Resource groupResource = Resource.Load(req.m_rGroupPrefab);
		if (!groupResource)
		{
			Print(string.Format("[SPT_WorldGarrison] DoSpawnSingle: nao foi possivel carregar recurso %1",
				req.m_rGroupPrefab), LogLevel.ERROR);
			return;
		}

		SCR_AIGroup group;
		int spawnedUnits;
		int groupCapacity;
		float patrolMemberSpacing;
		if (!req.m_bIsCQB)
			patrolMemberSpacing = GetPatrolMemberSpacing(GetLocationPatrolRadius(location));

		if (req.m_bIsFromCache && req.m_aAliveMembers)
		{
			group = SpawnGroupWithMembers(
				groupResource,
				req.m_vSpawnPosition,
				req.m_aAliveMembers,
				patrolMemberSpacing,
				spawnedUnits);
			groupCapacity = spawnedUnits;
		}
		else
		{
			int maxUnits = req.m_iMaxDeployableUnits;
			if (req.m_bIsBattleSpawn && req.m_BattleWave)
			{
				int waveRemaining = Math.Max(
					0,
					req.m_BattleWave.m_iUnitBudget - req.m_BattleWave.m_iDeployedUnits);
				maxUnits = Math.Min(maxUnits, waveRemaining);
			}
			if (!location.HasUnlimitedBudget())
				maxUnits = Math.Min(maxUnits, location.m_iBudgetRemaining);
			group = SpawnGroup(
				groupResource,
				req.m_vSpawnPosition,
				patrolMemberSpacing,
				spawnedUnits,
				groupCapacity,
				maxUnits);
		}

		if (!group)
		{
			Print(string.Format("[SPT_WorldGarrison] DoSpawnSingle: falha ao spawnar grupo | prefab=%1 | posicao=%2",
				req.m_rGroupPrefab, req.m_vSpawnPosition), LogLevel.ERROR);
			return;
		}

		if (!req.m_bIsFromCache && !req.m_bIsBattleSpawn)
			location.m_iTargetManpower = location.m_iTargetManpower + groupCapacity;

		if (spawnedUnits < 1)
		{
			if (groupCapacity < 1)
			{
				Print(string.Format("[SPT_WorldGarrison] DoSpawnSingle: grupo sem slots de unidade | prefab=%1",
					req.m_rGroupPrefab), LogLevel.ERROR);
			}
			else
			{
				DebugLog(string.Format("DoSpawnSingle: grupo nao mobilizado (sem budget) | prefab=%1 | capacidade=%2",
					req.m_rGroupPrefab, groupCapacity));
			}

			if (!DeleteGroup(group))
			{
				GetGame().GetCallqueue().CallLater(
					RetryDeleteOrphanGroup,
					250, false, group, 20);
			}
			return;
		}

		// Cache restore nao consome budget, apenas spawns frescos
		if (!req.m_bIsFromCache)
			location.ConsumeBudget(spawnedUnits);
		if (req.m_bIsBattleSpawn && req.m_BattleWave)
			req.m_BattleWave.m_iDeployedUnits += spawnedUnits;

		location.m_aGroupIds.Insert(group.GetID());

		// Rastreia metadados do grupo para futuro stream-out caching
		SPT_GroupSpawnRecord record = new SPT_GroupSpawnRecord();
		record.m_rGroupPrefab = req.m_rGroupPrefab;
		record.m_vFallbackPosition = req.m_vSpawnPosition;
		record.m_bIsCQB = req.m_bIsCQB;
		record.m_bIsBattleGroup = req.m_bIsBattleSpawn;
		location.m_aGroupRecords.Insert(record);

		location.m_iPendingGroups++;
		location.m_iDesiredUnits += spawnedUnits;

		DebugLog(string.Format("DoSpawnSingle concluido | local=%1 | prefab=%2 | membros=%3/%4 | isCQB=%5 | isCache=%6 | budgetRestante=%7",
			location.m_sName, req.m_rGroupPrefab, spawnedUnits, groupCapacity, req.m_bIsCQB, req.m_bIsFromCache, location.m_iBudgetRemaining));

		// Garante que o loop de HoldPoll do SPT_GarrisonManager esta ativo
		// antes de agendar o setup de patrulha/CQB. No modo assincrono, o
		// timing entre spawns pode deixar o CallLater do HoldPoll cancelado
		// com a flag m_bHoldPollRunning ainda true, impedindo waypoints.
		SPT_GarrisonManager.Get().ForceRestartHoldPoll();

		GetGame().GetCallqueue().CallLater(
			WaitForGroupMembers,
			250, false,
			location, group, req.m_vSpawnPosition, req.m_vCityCenter,
			!req.m_bIsCQB,
			req.m_bIsBattleSpawn,
			spawnedUnits,
			20);
	}

	protected SCR_AIGroup SpawnGroup(
		Resource groupResource,
		vector buildingCenter,
		float memberSpacing,
		out int spawnedUnits,
		out int groupCapacity,
		int maxUnits)
	{
		spawnedUnits = 0;
		groupCapacity = 0;

		EntitySpawnParams spawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		Math3D.MatrixIdentity4(spawnParams.Transform);
		buildingCenter[1] = GetGame().GetWorld().GetSurfaceY(buildingCenter[0], buildingCenter[2]) + 0.2;
		spawnParams.Transform[3] = buildingCenter;

		// Impede o EOnInit do prefab de tentar gerar membros antes de definirmos
		// o tamanho completo. A fila e preparada manualmente logo abaixo.
		SCR_AIGroup.IgnoreSpawning(true);
		IEntity spawnedEntity = GetGame().SpawnEntityPrefab(groupResource, GetGame().GetWorld(), spawnParams);
		SCR_AIGroup.IgnoreSpawning(false);

		SCR_AIGroup group = SCR_AIGroup.Cast(spawnedEntity);
		if (!group)
		{
			DebugLog(string.Format("SpawnEntityPrefab nao retornou SCR_AIGroup | entidade=%1", spawnedEntity));
			if (spawnedEntity)
				SCR_EntityHelper.DeleteEntityAndChildren(spawnedEntity);
			return null;
		}

		int capacity = group.m_aUnitPrefabSlots.Count();
		groupCapacity = capacity;
		DebugLog(string.Format("Template de grupo instanciado | grupo=%1 | capacidadeSlotsUnidade=%2",
			group, capacity));
		if (capacity < 1)
		{
			Print("[SPT_WorldGarrison] Template de grupo nao possui slots de prefab de unidade", LogLevel.ERROR);
			return group;
		}

		group.SetDeleteWhenEmpty(false);
		spawnedUnits = SpawnGroupMembersDirect(group, buildingCenter, memberSpacing, maxUnits);
		if (spawnedUnits > 0)
			group.SetDeleteWhenEmpty(true);
		DebugLog(string.Format("Membros diretos finalizados | grupo=%1 | solicitados=%2 | criados=%3 | agentesAtuais=%4",
			group, capacity, spawnedUnits, group.GetAgentsCount()));
		return group;
	}

	//! Spawna um grupo usando uma lista pre-definida de prefabs de membros em vez
	//! dos slots do template. Usado ao restaurar um grupo cacheado (streamed-out)
	//! para que apenas os membros que ainda estavam vivos reaparecam.
	protected SCR_AIGroup SpawnGroupWithMembers(
		Resource groupResource,
		vector spawnCenter,
		notnull array<ResourceName> memberPrefabs,
		float memberSpacing,
		out int spawnedUnits)
	{
		spawnedUnits = 0;

		if (!memberPrefabs || memberPrefabs.IsEmpty())
		{
			DebugLog("SpawnGroupWithMembers ignorado: lista de membros vazia.");
			return null;
		}

		EntitySpawnParams spawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		Math3D.MatrixIdentity4(spawnParams.Transform);
		spawnCenter[1] = GetGame().GetWorld().GetSurfaceY(spawnCenter[0], spawnCenter[2]) + 0.2;
		spawnParams.Transform[3] = spawnCenter;

		SCR_AIGroup.IgnoreSpawning(true);
		IEntity spawnedEntity = GetGame().SpawnEntityPrefab(groupResource, GetGame().GetWorld(), spawnParams);
		SCR_AIGroup.IgnoreSpawning(false);

		SCR_AIGroup group = SCR_AIGroup.Cast(spawnedEntity);
		if (!group)
		{
			DebugLog(string.Format("SpawnGroupWithMembers: SpawnEntityPrefab nao retornou SCR_AIGroup | entidade=%1", spawnedEntity));
			if (spawnedEntity)
				SCR_EntityHelper.DeleteEntityAndChildren(spawnedEntity);
			return null;
		}

		group.SetDeleteWhenEmpty(false);
		spawnedUnits = SpawnGroupMembersFromList(group, spawnCenter, memberPrefabs, memberSpacing);
		if (spawnedUnits > 0)
			group.SetDeleteWhenEmpty(true);

		DebugLog(string.Format("SpawnGroupWithMembers finalizado | grupo=%1 | membrosSolicitados=%2 | criados=%3 | agentesAtuais=%4",
			group, memberPrefabs.Count(), spawnedUnits, group.GetAgentsCount()));
		return group;
	}

	protected int SpawnGroupMembersFromList(
		notnull SCR_AIGroup group,
		vector center,
		notnull array<ResourceName> memberPrefabs,
		float memberSpacing)
	{
		int spawnedCount;
		int count = memberPrefabs.Count();
		for (int i = 0; i < count; i++)
		{
			ResourceName memberPrefab = memberPrefabs[i];
			Resource memberResource = Resource.Load(memberPrefab);
			if (!memberResource)
			{
				Print(string.Format("[SPT_WorldGarrison] SpawnGroupMembersFromList: nao foi possivel carregar membro %1: %2",
					i, memberPrefab), LogLevel.ERROR);
				continue;
			}

			vector memberPosition = GetMemberSpawnPosition(center, i, memberSpacing);
			memberPosition[1] = GetGame().GetWorld().GetSurfaceY(memberPosition[0], memberPosition[2]) + 0.2;

			EntitySpawnParams memberParams();
			memberParams.TransformMode = ETransformMode.WORLD;
			Math3D.MatrixIdentity4(memberParams.Transform);
			memberParams.Transform[3] = memberPosition;

			IEntity member = GetGame().SpawnEntityPrefab(memberResource, GetGame().GetWorld(), memberParams);
			if (!member)
			{
				Print(string.Format("[SPT_WorldGarrison] SpawnGroupMembersFromList: spawn direto falhou para membro %1: %2",
					i, memberPrefab), LogLevel.ERROR);
				continue;
			}

			if (!group.AddAIEntityToGroup(member))
			{
				Print(string.Format("[SPT_WorldGarrison] SpawnGroupMembersFromList: nao foi possivel associar membro %1 ao grupo: %2",
					i, memberPrefab), LogLevel.ERROR);
				SCR_EntityHelper.DeleteEntityAndChildren(member);
				continue;
			}

			FactionAffiliationComponent faction = FactionAffiliationComponent.Cast(
				member.FindComponent(FactionAffiliationComponent));
			if (faction)
				faction.SetAffiliatedFactionByKey(group.m_faction);

			spawnedCount++;
		}

		return spawnedCount;
	}

	protected int SpawnGroupMembersDirect(
		notnull SCR_AIGroup group,
		vector center,
		float memberSpacing,
		int maxUnits)
	{
		int spawnedCount;
		int slotCount = group.m_aUnitPrefabSlots.Count();
		if (maxUnits >= 0 && slotCount > maxUnits)
			slotCount = maxUnits;
		for (int i = 0; i < slotCount; i++)
		{
			ResourceName memberPrefab = group.m_aUnitPrefabSlots[i];
			Resource memberResource = Resource.Load(memberPrefab);
			if (!memberResource)
			{
				Print(string.Format("[SPT_WorldGarrison] Nao foi possivel carregar membro %1 do grupo: %2",
					i, memberPrefab), LogLevel.ERROR);
				continue;
			}

			vector memberPosition = GetMemberSpawnPosition(center, i, memberSpacing);
			memberPosition[1] = GetGame().GetWorld().GetSurfaceY(memberPosition[0], memberPosition[2]) + 0.2;

			EntitySpawnParams memberParams();
			memberParams.TransformMode = ETransformMode.WORLD;
			Math3D.MatrixIdentity4(memberParams.Transform);
			memberParams.Transform[3] = memberPosition;

			IEntity member = GetGame().SpawnEntityPrefab(memberResource, GetGame().GetWorld(), memberParams);
			if (!member)
			{
				Print(string.Format("[SPT_WorldGarrison] Spawn direto falhou para membro %1: %2",
					i, memberPrefab), LogLevel.ERROR);
				continue;
			}

			if (!group.AddAIEntityToGroup(member))
			{
				Print(string.Format("[SPT_WorldGarrison] Nao foi possivel associar membro %1 ao grupo: %2",
					i, memberPrefab), LogLevel.ERROR);
				SCR_EntityHelper.DeleteEntityAndChildren(member);
				continue;
			}

			FactionAffiliationComponent faction = FactionAffiliationComponent.Cast(
				member.FindComponent(FactionAffiliationComponent));
			if (faction)
				faction.SetAffiliatedFactionByKey(group.m_faction);

			spawnedCount++;
		}

		return spawnedCount;
	}

	protected vector GetMemberSpawnPosition(vector center, int index, float patrolSpacing)
	{
		float angle = index * 137.5 * Math.PI / 180.0;
		float radius;
		if (patrolSpacing > 0)
		{
			radius = patrolSpacing * Math.Sqrt(index + 1);
		}
		else
		{
			int radiusStep = index % 3;
			radius = 1.5 + 0.5 * radiusStep;
		}

		return center + Vector(Math.Cos(angle) * radius, 0, Math.Sin(angle) * radius);
	}

	protected float GetPatrolMemberSpacing(float patrolRadius)
	{
		return Math.Clamp(
			patrolRadius * PATROL_MEMBER_RADIUS_FACTOR,
			PATROL_MEMBER_MIN_SPACING_M,
			PATROL_MEMBER_MAX_SPACING_M);
	}

	protected void WaitForGroupMembers(
		SPT_GarrisonLocation location,
		SCR_AIGroup group,
		vector buildingCenter,
		vector patrolCenter,
		bool patrolGroup,
		bool battleGroup,
		int expectedMembers,
		int pollsLeft)
	{
		if (!group)
		{
			DebugLog(string.Format("Callback de guarnicao ignorado: grupo nao existe mais no centro %1.", buildingCenter));
			if (location)
				PruneMissingGroupIds(location);
			CompletePendingGroup(location, false);
			return;
		}

		int agentCount = group.GetAgentsCount();
		if (agentCount < expectedMembers && pollsLeft > 0)
		{
			DebugLog(string.Format("Aguardando grupo completo | grupo=%1 | agentes=%2/%3 | fila=%4 | inicializando=%5 | tentativasRestantes=%6",
				group, agentCount, expectedMembers, group.GetSpawnQueueSize(), group.IsInitializing(), pollsLeft));
			GetGame().GetCallqueue().CallLater(
				WaitForGroupMembers,
				250,
				false,
				location,
				group,
				buildingCenter,
				patrolCenter,
				patrolGroup,
				battleGroup,
				expectedMembers,
				pollsLeft - 1
			);
			return;
		}

		if (agentCount < 1)
		{
			Print(string.Format("[SPT_WorldGarrison] Grupo falhou ao criar membros apos 5 segundos | grupo=%1 | centro=%2",
				group, buildingCenter), LogLevel.ERROR);
			RemoveGroupId(location, group.GetID());
			DeleteGroup(group);
			CompletePendingGroup(location, false);
			return;
		}

		if (agentCount < expectedMembers)
			Print(string.Format("[SPT_WorldGarrison] Grupo permaneceu parcial apos 5 segundos | grupo=%1 | agentes=%2/%3",
				group, agentCount, expectedMembers), LogLevel.WARNING);

		// Seguranca: garante que o HoldPoll esta rodando antes do setup
		// de patrulha/CQB. No spawn assincrono, atrasos entre grupos
		// podem cancelar o CallLater do HoldPoll com a flag ainda true.
		SPT_GarrisonManager.Get().ForceRestartHoldPoll();

		if (patrolGroup)
		{
			EMovementType movementType = GetLocationPatrolMovementType(location);
			if (battleGroup)
				movementType = GetLocationBattleMovementType(location);
			DebugLog(string.Format("Enviando grupo para patrulha | grupo=%1 | agentes=%2 | centroCidade=%3 | raio=%4",
				group, agentCount, patrolCenter, GetLocationPatrolRadius(location)));
			SPT_AIGarrisonHelper.PatrolGroup(
				group,
				patrolCenter,
				GetLocationPatrolRadius(location),
				GetLocationPatrolWaypointPrefab(location),
				GetLocationPatrolFormation(location),
				movementType);
		}
		else
		{
			DebugLog(string.Format("Enviando grupo para CQB estatico | grupo=%1 | agentes=%2 | centroConstrucao=%3",
				group, agentCount, buildingCenter));
			SPT_AIGarrisonHelper.GarrisonGroup(group, buildingCenter, GetLocationBuildingSearchRadius(location));
		}
		CompletePendingGroup(location, true);
	}

	protected void DespawnLocation(notnull SPT_GarrisonLocation location)
	{
		CancelPendingSpawns(location);

		if (IsLocationCachingEnabled(location))
		{
			// =============================================================
			// CAMINHO DE CACHING: Salva os membros vivos de cada grupo
			// antes de deletar a entidade. Quando o jogador retorna,
			// apenas os sobreviventes sao respawnados em vez de uma
			// guarnicao nova.
			// =============================================================
			// Inicia um novo snapshot apenas para uma populacao recem-ativa.
			// Se um stream-out anterior reteve grupos, as tentativas
			// subsequentes adicionam ao mesmo snapshot em vez de apagar
			// sobreviventes ja capturados.
			if (!location.m_bHasCachedSnapshot)
			{
				location.m_aCachedGroups.Clear();
				location.m_bHasCachedSnapshot = true;
			}
			int cachedCount = 0;
			int eliminatedCount = 0;
			int retainedCount = 0;

			for (int i = location.m_aGroupIds.Count() - 1; i >= 0; i--)
			{
				EntityID groupId = location.m_aGroupIds[i];
				IEntity groupEntity = GetGame().GetWorld().FindEntityByID(groupId);
				SCR_AIGroup group = SCR_AIGroup.Cast(groupEntity);

				SPT_GroupSpawnRecord record;
				if (i < location.m_aGroupRecords.Count())
					record = location.m_aGroupRecords[i];

				if (!group)
				{
				// Um grupo rastreado que nao existe mais foi eliminado
				// antes do stream-out. Sua ausencia faz parte do snapshot.
				if (record)
						eliminatedCount++;
					location.m_aGroupIds.Remove(i);
					if (i < location.m_aGroupRecords.Count())
						location.m_aGroupRecords.Remove(i);
					continue;
				}

				if (record)
				{
				// Captura membros vivos ANTES de deletar a entidade
				SPT_StreamableGarrisonGroup cached = new SPT_StreamableGarrisonGroup();
					int aliveCount = cached.CaptureFromGroup(
						group,
						record.m_rGroupPrefab,
						record.m_vFallbackPosition,
						record.m_bIsCQB);
					cached.m_bIsBattleGroup = record.m_bIsBattleGroup;
					cached.m_rVehiclePrefab = record.m_rVehiclePrefab;
					DebugLog(string.Format("Estado capturado para cache | grupo=%1 | vivos=%2 | centroSobreviventes=%3 | isCQB=%4",
						group, aliveCount, cached.m_vPosition, record.m_bIsCQB));

					if (DeleteGroup(group))
					{
						if (aliveCount > 0)
						{
							location.m_aCachedGroups.Insert(cached);
							cachedCount++;
						}
						else
						{
							eliminatedCount++;
						}
						location.m_aGroupIds.Remove(i);
						if (i < location.m_aGroupRecords.Count())
							location.m_aGroupRecords.Remove(i);
					}
					else
					{
						retainedCount++;
					}
				}
				else
				{
					// Sem registro de metadados – deleta sem cachear
				if (DeleteGroup(group))
						location.m_aGroupIds.Remove(i);
					else
						retainedCount++;
				}
			}

			foreach (EntityID vehicleId : location.m_Battle.m_aVehicleIds)
			{
				IEntity vehicle = GetGame().GetWorld().FindEntityByID(vehicleId);
				if (vehicle)
					SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
			}
			location.m_Battle.m_aVehicleIds.Clear();

			// Dados cacheados sao estado de stream-out, nao uma entidade
			// ativa no mundo. Mantem ativo apenas enquanto um grupo nao
			// pode ser removido.
			location.m_bActive = !location.m_aGroupIds.IsEmpty();
			location.m_bSpawning = false;
			location.m_iPendingGroups = 0;
			location.m_iSuccessfulGroups = 0;
			location.RefreshClearedState();

			Print(string.Format("[SPT_WorldGarrison] Stream-out de %1 | novosCacheados=%2 | totalCacheados=%3 | unidadesCacheadas=%4 | eliminados=%5 | retidos=%6 | ativo=%7 | snapshot=%8",
				location.m_sName,
				cachedCount,
				location.m_aCachedGroups.Count(),
				location.GetCachedManpower(),
				eliminatedCount,
				retainedCount,
				location.m_bActive,
				location.m_bHasCachedSnapshot));
			Print(string.Format("[SPT_WorldGarrison] Estado de forca em %1 | manpowerTotal=%2/%3 | budgetRestante=%4 | limpo=%5",
				location.m_sName,
				location.GetTotalManpower(),
				location.m_iTargetManpower,
				location.m_iBudgetRemaining,
				location.m_bCleared));
		}
		else
		{
			// =============================================================
			// CAMINHO ORIGINAL (sem caching): Deleta todos os grupos
			// imediatamente.
			// =============================================================
			int groupCount = location.m_aGroupIds.Count();
			array<EntityID> pendingGroupIds = {};
			foreach (EntityID groupId : location.m_aGroupIds)
			{
				IEntity groupEntity = GetGame().GetWorld().FindEntityByID(groupId);
				SCR_AIGroup group = SCR_AIGroup.Cast(groupEntity);
				if (group && !DeleteGroup(group))
					pendingGroupIds.Insert(groupId);
			}

			location.m_aGroupIds.Clear();
			foreach (EntityID pendingGroupId : pendingGroupIds)
				location.m_aGroupIds.Insert(pendingGroupId);

			location.m_bActive = !pendingGroupIds.IsEmpty();
			location.m_bSpawning = false;
			location.m_iPendingGroups = 0;
			location.m_iSuccessfulGroups = 0;
			location.RefreshClearedState();
			Print(string.Format("[SPT_WorldGarrison] Despawn de %1 | removidos=%2 | pendentes=%3 | manpowerTotal=%4 | budgetRestante=%5 | limpo=%6",
				location.m_sName,
				groupCount - pendingGroupIds.Count(),
				pendingGroupIds.Count(),
				location.GetTotalManpower(),
				location.m_iBudgetRemaining,
				location.m_bCleared));
		}
	}

	protected void CompletePendingGroup(SPT_GarrisonLocation location, bool successful)
	{
		if (!location)
			return;

		if (successful)
			location.m_iSuccessfulGroups++;

		if (location.m_iPendingGroups > 0)
			location.m_iPendingGroups--;

		if (location.m_iPendingGroups > 0)
			return;

		// So finaliza o estado de spawn quando TODOS os grupos da fila
		// foram processados pelo DoSpawnSingle. Sem esta guarda,
		// CompletePendingGroup do primeiro grupo zera m_iPendingGroups
		// enquanto os demais ainda estao na fila, causando snapshot
		// prematuro do manpower.
		if (location.m_iQueuedSpawnUnits > 0)
			return;

		location.m_bSpawning = false;
		location.m_bActive = location.m_iSuccessfulGroups > 0;

		// Se o Warfare pediu monitoramento enquanto o spawn ainda estava
		// em andamento, captura o manpower inicial agora que esta estavel.
		if (location.m_bMonitoringEnabled && location.m_iInitialGarrisonManpower <= 0)
		{
			location.SnapInitialManpower();
			Print(string.Format("[SPT_WorldGarrison] Monitoramento ativado pos-spawn | id=%1 | nome=%2 | manpowerInicial=%3",
				location.m_sLocationId,
				location.m_sName,
				location.m_iInitialGarrisonManpower));
		}

		location.RefreshClearedState();

		if (location.m_bActive)
		{
			Print(string.Format("[SPT_WorldGarrison] Guarnicao pronta em %1 | gruposBemSucedidos=%2 | gruposRastreados=%3 | manpower=%4/%5 | budgetRestante=%6",
				location.m_sName,
				location.m_iSuccessfulGroups,
				location.m_aGroupIds.Count(),
				location.GetTotalManpower(),
				location.m_iTargetManpower,
				location.m_iBudgetRemaining));
		}
		else if (location.m_bCleared)
		{
			Print(string.Format("[SPT_WorldGarrison] Local limpo: %1 | manpower=0 | budgetRestante=0",
				location.m_sName));
		}
		else
		{
			Print(string.Format("[SPT_WorldGarrison] Guarnicao falhou em %1; local tentara novamente enquanto houver jogador por perto.",
				location.m_sName), LogLevel.ERROR);
		}
	}

	protected void RemoveGroupId(SPT_GarrisonLocation location, EntityID groupId)
	{
		if (!location)
			return;

		int index = location.m_aGroupIds.Find(groupId);
		if (index >= 0)
		{
			location.m_aGroupIds.Remove(index);
			if (index < location.m_aGroupRecords.Count())
				location.m_aGroupRecords.Remove(index);
		}
	}

	protected void PruneMissingGroupIds(notnull SPT_GarrisonLocation location)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		for (int i = location.m_aGroupIds.Count() - 1; i >= 0; i--)
		{
			if (!world.FindEntityByID(location.m_aGroupIds[i]))
			{
				location.m_aGroupIds.Remove(i);
				if (i < location.m_aGroupRecords.Count())
					location.m_aGroupRecords.Remove(i);
			}
		}
	}

	protected void DebugLog(string message)
	{
		if (!m_bDebug)
			return;

		Print("[SPT_WorldGarrison][DEBUG] " + message);
	}

	protected bool DeleteGroup(notnull SCR_AIGroup group)
	{
		// O grupo, seus membros e waypoints formam uma hierarquia editavel.
		// Deixa o ciclo de vida do editor remover essa hierarquia em uma operacao
		// em vez de deletar cada entidade registrada por detras do SCR_EditableEntityCore.
		SPT_AIGarrisonHelper.UngarrisonGroup(group, false);

		SCR_EditableEntityComponent editableGroup =
			SCR_EditableEntityComponent.GetEditableEntity(group);
		if (editableGroup)
		{
			if (editableGroup.Delete(false, false))
				return true;

			Print(string.Format("[SPT_WorldGarrison] Exclusao recusada pelo ciclo de vida editavel; grupo mantido para nova tentativa | grupo=%1",
				group), LogLevel.WARNING);
			return false;
		}

		// Non-editable group prefabs do not participate in the editor hierarchy.
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			IEntity character = agent.GetControlledEntity();
			if (character)
				SCR_EntityHelper.DeleteEntityAndChildren(character);
		}

		SCR_EntityHelper.DeleteEntityAndChildren(group);
		return true;
	}

	//! Tenta novamente a limpeza quando o ciclo de vida de entidade editavel
	//! recusa temporariamente a delecao de um grupo vazio criado por uma
	//! restauracao de cache que falhou.
	protected void RetryDeleteOrphanGroup(SCR_AIGroup group, int retriesLeft)
	{
		if (!group)
			return;

		if (DeleteGroup(group))
			return;

		if (retriesLeft > 0)
		{
			GetGame().GetCallqueue().CallLater(
				RetryDeleteOrphanGroup,
				250,
				false,
				group,
				retriesLeft - 1
			);
			return;
		}

		Print(string.Format("[SPT_WorldGarrison] Nao foi possivel remover grupo vazio apos restauracao falha | id=%1",
			group.GetID()), LogLevel.ERROR);
	}

	protected bool FindOutdoorPatrolSpawn(
		vector cityCenter,
		float patrolRadius,
		notnull array<vector> buildingCenters,
		notnull array<vector> occupiedPatrolSpawns,
		int sequence,
		out vector outPosition)
	{
		float buildingClearanceSq = PATROL_SPAWN_BUILDING_CLEARANCE_M * PATROL_SPAWN_BUILDING_CLEARANCE_M;
		float targetGroupClearance = Math.Min(
			patrolRadius,
			Math.Max(
				PATROL_SPAWN_GROUP_MIN_CLEARANCE_M,
				patrolRadius * PATROL_SPAWN_GROUP_RADIUS_FACTOR));
		float targetGroupClearanceSq = targetGroupClearance * targetGroupClearance;
		float bestPreferredScore = -1;
		float bestPreferredBuildingDistanceSq;
		vector bestPreferredPosition;
		float bestFallbackScore = -1;
		float bestFallbackBuildingDistanceSq;
		vector bestFallbackPosition;

		for (int ring = 1; ring <= PATROL_SPAWN_RING_COUNT; ring++)
		{
			float distance = patrolRadius * ring / PATROL_SPAWN_RING_COUNT;
			for (int sample = 0; sample < PATROL_SPAWN_SAMPLES_PER_RING; sample++)
			{
				float angle = (sample + sequence * 0.37) * 2.0 * Math.PI / PATROL_SPAWN_SAMPLES_PER_RING;
				vector candidate = cityCenter + Vector(
					Math.Cos(angle) * distance,
					0,
					Math.Sin(angle) * distance);

				float nearestBuildingSq = 1000000000.0;
				foreach (vector buildingCenter : buildingCenters)
				{
					float buildingDistanceSq = HorizontalDistanceSq(candidate, buildingCenter);
					if (buildingDistanceSq < nearestBuildingSq)
						nearestBuildingSq = buildingDistanceSq;
				}
				if (buildingCenters.IsEmpty())
					nearestBuildingSq = HorizontalDistanceSq(candidate, cityCenter);
				if (nearestBuildingSq < buildingClearanceSq)
					continue;

				float nearestPatrolSq = 1000000000.0;
				foreach (vector occupiedSpawn : occupiedPatrolSpawns)
				{
					float patrolDistanceSq = HorizontalDistanceSq(candidate, occupiedSpawn);
					if (patrolDistanceSq < nearestPatrolSq)
						nearestPatrolSq = patrolDistanceSq;
				}

				float score = nearestBuildingSq;
				if (!occupiedPatrolSpawns.IsEmpty())
					score = nearestPatrolSq;

				if (score > bestFallbackScore ||
					(score == bestFallbackScore && nearestBuildingSq > bestFallbackBuildingDistanceSq))
				{
					bestFallbackScore = score;
					bestFallbackBuildingDistanceSq = nearestBuildingSq;
					bestFallbackPosition = candidate;
				}

				if (!occupiedPatrolSpawns.IsEmpty() && nearestPatrolSq < targetGroupClearanceSq)
					continue;

				if (score > bestPreferredScore ||
					(score == bestPreferredScore && nearestBuildingSq > bestPreferredBuildingDistanceSq))
				{
					bestPreferredScore = score;
					bestPreferredBuildingDistanceSq = nearestBuildingSq;
					bestPreferredPosition = candidate;
				}
			}
		}

		bool usedFallback;
		float selectedBuildingDistanceSq;
		float selectedGroupDistanceSq;
		if (bestPreferredScore >= 0)
		{
			outPosition = bestPreferredPosition;
			selectedBuildingDistanceSq = bestPreferredBuildingDistanceSq;
			selectedGroupDistanceSq = bestPreferredScore;
		}
		else if (bestFallbackScore >= 0)
		{
			usedFallback = true;
			outPosition = bestFallbackPosition;
			selectedBuildingDistanceSq = bestFallbackBuildingDistanceSq;
			selectedGroupDistanceSq = bestFallbackScore;
		}
		else
		{
			return false;
		}

		outPosition[1] = GetGame().GetWorld().GetSurfaceY(outPosition[0], outPosition[2]) + 0.2;
		if (usedFallback && !occupiedPatrolSpawns.IsEmpty())
		{
			Print(string.Format("[SPT_WorldGarrison] Espaco limitado para patrulha; usando maior separacao disponivel | posicao=%1 | separacao=%2m | alvo=%3m",
				outPosition, Math.Sqrt(selectedGroupDistanceSq), targetGroupClearance), LogLevel.WARNING);
		}
		DebugLog(string.Format("Spawn externo de patrulha selecionado | posicao=%1 | distanciaConstrucao=%2m | distanciaGrupo=%3m | alvoGrupo=%4m | fallback=%5 | sequencia=%6",
			outPosition,
			Math.Sqrt(selectedBuildingDistanceSq),
			Math.Sqrt(selectedGroupDistanceSq),
			targetGroupClearance,
			usedFallback,
			sequence));
		return true;
	}

	protected void ShufflePositions(notnull array<vector> positions)
	{
		for (int i = positions.Count() - 1; i > 0; i--)
		{
			int swapIndex = Math.RandomInt(0, i + 1);
			vector temporary = positions[i];
			positions[i] = positions[swapIndex];
			positions[swapIndex] = temporary;
		}
	}

	protected float HorizontalDistanceSq(vector a, vector b)
	{
		float deltaX = a[0] - b[0];
		float deltaZ = a[2] - b[2];
		return deltaX * deltaX + deltaZ * deltaZ;
	}
}
