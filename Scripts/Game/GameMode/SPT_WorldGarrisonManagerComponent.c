enum SPT_EGarrisonFaction
{
	OPFOR,
	BLUFOR,
	INDEPENDENT
}

class SPT_GarrisonLocation : Managed
{
	vector m_vCenter;
	string m_sName;
	int m_iDescriptorType;
	bool m_bActive;
	bool m_bSpawning;
	bool m_bUnavailable;
	int m_iPendingGroups;
	int m_iSuccessfulGroups;
	int m_iDesiredUnits;
	ref array<EntityID> m_aGroupIds = new array<EntityID>();

	void SPT_GarrisonLocation(vector center, string name, int descriptorType)
	{
		m_vCenter = center;
		m_sName = name;
		m_iDescriptorType = descriptorType;
	}
}

[ComponentEditorProps(category: "SPT/GameMode", description: "Dynamically spawns AI garrisons in settlements and strategic map locations")]
class SPT_WorldGarrisonManagerComponentClass : ScriptComponentClass
{
}

class SPT_WorldGarrisonManagerComponent : ScriptComponent
{
	[Attribute("0", desc: "Enable detailed diagnostic logs with the [SPT_WorldGarrison][DEBUG] prefix")]
	protected bool m_bDebug;

	[Attribute("0", desc: "Faction used by every generated garrison: 0 OPFOR, 1 BLUFOR, 2 INDEPENDENT")]
	protected SPT_EGarrisonFaction m_eFaction;

	[Attribute("800", desc: "Spawn or respawn when a player is within this distance (metres)")]
	protected float m_fSpawnDistance;

	[Attribute("1000", desc: "Despawn after every player is farther than this distance (metres). Must be greater than spawn distance.")]
	protected float m_fDespawnDistance;

	[Attribute("2", desc: "Seconds between player-distance checks")]
	protected float m_fUpdateInterval;

	[Attribute("500", desc: "Game Master AI budget used by the garrison. Increase this when many locations can be active simultaneously. Set to 0 to keep the scenario budget unchanged.")]
	protected int m_iGameMasterAIBudget;

	[Attribute("175", desc: "Radius around each map location used to find buildings")]
	protected float m_fBuildingSearchRadius;

	[Attribute("75", desc: "Ignore duplicate map descriptors closer than this distance")]
	protected float m_fMinimumLocationSpacing;

	[Attribute("1", desc: "Include cities, towns, villages and named settlements")]
	protected bool m_bIncludeSettlements;

	[Attribute("1", desc: "Include bases and other strategic/military markers")]
	protected bool m_bIncludeStrategicSites;

	[Attribute("{4D3BBEC1A955626A}Prefabs/Groups/OPFOR/Spetsnaz/Group_USSR_Spetsnaz_Squad.et", UIWidgets.ResourceNamePicker, desc: "OPFOR group templates. Do not use _NotSpawned ambient patrol prefabs.", params: "et class=SCR_AIGroup")]
	protected ref array<ResourceName> m_aOpforGroupPrefabs = {
		"{4D3BBEC1A955626A}Prefabs/Groups/OPFOR/Spetsnaz/Group_USSR_Spetsnaz_Squad.et"
	};

	[Attribute("{D807C7047E818488}Prefabs/Groups/BLUFOR/Group_US_SniperTeam.et", UIWidgets.ResourceNamePicker, desc: "BLUFOR group templates. Do not use _NotSpawned ambient patrol prefabs.", params: "et class=SCR_AIGroup")]
	protected ref array<ResourceName> m_aBluforGroupPrefabs = {
		"{D807C7047E818488}Prefabs/Groups/BLUFOR/Group_US_SniperTeam.et",
		"{D807C7047E818488}Prefabs/Groups/BLUFOR/Group_US_SniperTeam.et",
		"{D807C7047E818488}Prefabs/Groups/BLUFOR/Group_US_SniperTeam.et",
		"{D807C7047E818488}Prefabs/Groups/BLUFOR/Group_US_SniperTeam.et",
		"{D807C7047E818488}Prefabs/Groups/BLUFOR/Group_US_SniperTeam.et",
		"{D807C7047E818488}Prefabs/Groups/BLUFOR/Group_US_SniperTeam.et",
		"{D807C7047E818488}Prefabs/Groups/BLUFOR/Group_US_SniperTeam.et",
		"{D807C7047E818488}Prefabs/Groups/BLUFOR/Group_US_SniperTeam.et",
		"{D807C7047E818488}Prefabs/Groups/BLUFOR/Group_US_SniperTeam.et"
	};

	[Attribute("{22F33D3EC8F281AB}Prefabs/Groups/INDFOR/Group_FIA_MachineGunTeam.et", UIWidgets.ResourceNamePicker, desc: "INDEPENDENT group templates. Do not use _NotSpawned ambient patrol prefabs.", params: "et class=SCR_AIGroup")]
	protected ref array<ResourceName> m_aIndependentGroupPrefabs = {
		"{22F33D3EC8F281AB}Prefabs/Groups/INDFOR/Group_FIA_MachineGunTeam.et"
	};

	[Attribute("{FFF9518F73279473}PrefabsEditable/Auto/AI/Waypoints/E_AIWaypoint_Move.et", UIWidgets.ResourceNamePicker, desc: "Waypoint used by groups patrolling between buildings", params: "et")]
	protected ResourceName m_sPatrolWaypointPrefab;

	protected ref array<ref SPT_GarrisonLocation> m_aLocations = new array<ref SPT_GarrisonLocation>();
	protected int m_iDebugUpdateCounter;

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

		ValidateSettings();
		DebugLog(string.Format("Inicializacao agendada em 1000 ms | faccao=%1 | prefabsGrupoConfigurados=%2",
			m_eFaction, GetSelectedGroupPrefabCount()));
		if (m_iGameMasterAIBudget > 0)
			GetGame().GetCallqueue().CallLater(ConfigureGameMasterBudget, 1000, false, 10);
		GetGame().GetCallqueue().CallLater(InitializeLocations, 1000, false);
	}

	protected void ValidateSettings()
	{
		if (m_fSpawnDistance < 1)
			m_fSpawnDistance = 800;

		if (m_fDespawnDistance <= m_fSpawnDistance)
			m_fDespawnDistance = m_fSpawnDistance + 200;

		if (m_fUpdateInterval < 0.25)
			m_fUpdateInterval = 0.25;

		if (m_iGameMasterAIBudget < 0)
			m_iGameMasterAIBudget = 0;

		if (m_fBuildingSearchRadius < 25)
			m_fBuildingSearchRadius = 25;

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

		world.QueryEntitiesBySphere(
			vector.Zero,
			99999999,
			CollectLocation,
			FilterLocation,
			EQueryEntitiesFlags.ALL
		);

		Print(string.Format("[SPT_WorldGarrison] %1 locais registrados | faccao %2 | spawn %3m | despawn %4m | grupos completos por local=%5",
			m_aLocations.Count(), m_eFaction, m_fSpawnDistance, m_fDespawnDistance, GetSelectedGroupPrefabCount()));

		if (m_aLocations.IsEmpty())
			Print("[SPT_WorldGarrison] Nenhum local de mapa suportado foi encontrado. Verifique os tipos de descritor e as configuracoes de inclusao.", LogLevel.WARNING);

		UpdateLocations();
		int updateIntervalMs = Math.Round(m_fUpdateInterval * 1000);
		GetGame().GetCallqueue().CallLater(UpdateLocations, updateIntervalMs, true);
		DebugLog(string.Format("Loop de atualizacao de distancia iniciado com intervalo de %1 ms.", updateIntervalMs));
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
		if (IsNearRegisteredLocation(center))
			return true;

		string name = "Map location";
		MapItem item = descriptor.Item();
		if (item)
		{
			string displayName = item.GetDisplayName();
			if (!displayName.IsEmpty())
				name = displayName;
		}

		m_aLocations.Insert(new SPT_GarrisonLocation(center, name, descriptor.GetBaseType()));
		DebugLog(string.Format("Local registrado | nome=%1 | tipo=%2 | posicao=%3",
			name, descriptor.GetBaseType(), center));
		return true;
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

		float spawnDistanceSq = m_fSpawnDistance * m_fSpawnDistance;
		float despawnDistanceSq = m_fDespawnDistance * m_fDespawnDistance;

		foreach (SPT_GarrisonLocation location : m_aLocations)
		{
			if (location.m_bUnavailable)
				continue;
			if (location.m_bSpawning)
				continue;

			float nearestPlayerSq = GetNearestPlayerDistanceSq(location.m_vCenter, playerPositions);
			if (logThisUpdate && nearestPlayerSq <= despawnDistanceSq * 4)
			{
				DebugLog(string.Format("Distancia do local | nome=%1 | distancia=%2m | ativo=%3 | indisponivel=%4",
					location.m_sName, Math.Sqrt(nearestPlayerSq), location.m_bActive, location.m_bUnavailable));
			}

			if (!location.m_bActive && nearestPlayerSq <= spawnDistanceSq)
			{
				DebugLog(string.Format("Limite de spawn atingido para %1.", location.m_sName));
				SpawnLocation(location);
			}
			else if (location.m_bActive && nearestPlayerSq > despawnDistanceSq)
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

	protected void SpawnLocation(notnull SPT_GarrisonLocation location)
	{
		PruneMissingGroupIds(location);
		DebugLog(string.Format("SpawnLocation iniciado | nome=%1 | centro=%2 | prefabsGrupoConfigurados=%3",
			location.m_sName, location.m_vCenter, GetSelectedGroupPrefabCount()));

		if (GetSelectedGroupPrefabCount() < 1)
		{
			Print(string.Format("[SPT_WorldGarrison] Nenhum prefab de grupo configurado para a faccao %1.", m_eFaction), LogLevel.ERROR);
			return;
		}

		array<vector> buildings = {};
		SPT_GarrisonDetector.CollectBuildingCenters(
			GetGame().GetWorld(),
			location.m_vCenter,
			m_fBuildingSearchRadius,
			buildings
		);
		DebugLog(string.Format("Detector de construcoes retornou %1 centros estruturais em %2m.",
			buildings.Count(), m_fBuildingSearchRadius));

		if (buildings.IsEmpty())
		{
			Print(string.Format("[SPT_WorldGarrison] Nenhuma construcao utilizavel ao redor de %1 em %2",
				location.m_sName, location.m_vCenter), LogLevel.WARNING);
			location.m_bUnavailable = true;
			return;
		}

		ShufflePositions(buildings);

		location.m_bSpawning = true;
		location.m_iPendingGroups = 0;
		location.m_iSuccessfulGroups = 0;
		location.m_iDesiredUnits = 0;
		int buildingIndex = 0;

		array<ResourceName> groupPrefabs = GetSelectedGroupPrefabs();
		DebugLog(string.Format("Lista selecionada para spawn | faccao=%1 | entradas=%2",
			m_eFaction, groupPrefabs.Count()));
		foreach (int prefabIndex, ResourceName groupPrefab : groupPrefabs)
		{
			int selectedBuilding = buildingIndex % buildings.Count();
			vector buildingCenter = buildings[selectedBuilding];
			buildingIndex++;

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

			int spawnedUnits;
			DebugLog(string.Format("Spawnando grupo completo da lista | indice=%1 | prefab=%2 | centro=%3",
				prefabIndex, groupPrefab, buildingCenter));
			SCR_AIGroup group = SpawnGroup(groupResource, buildingCenter, spawnedUnits);
			if (!group)
			{
				Print(string.Format("[SPT_WorldGarrison] Falha ao spawnar grupo completo no indice %1: %2",
					prefabIndex, groupPrefab), LogLevel.ERROR);
				continue;
			}

			if (spawnedUnits < 1)
			{
				Print(string.Format("[SPT_WorldGarrison] Grupo no indice %1 nao possui slots de unidades: %2",
					prefabIndex, groupPrefab), LogLevel.ERROR);
				continue;
			}

			location.m_aGroupIds.Insert(group.GetID());
			location.m_iPendingGroups++;
			location.m_iDesiredUnits += spawnedUnits;
			DebugLog(string.Format("Grupo completo criado | id=%1 | membros=%2 | centro=%3",
				group.GetID(), spawnedUnits, buildingCenter));
			GetGame().GetCallqueue().CallLater(
				WaitForGroupMembers,
				250,
				false,
				location,
				group,
				buildingCenter,
				spawnedUnits,
				20
			);
		}

		if (location.m_aGroupIds.IsEmpty())
		{
			location.m_bSpawning = false;
			Print(string.Format("[SPT_WorldGarrison] Falha ao criar grupos para %1.", location.m_sName), LogLevel.ERROR);
			return;
		}

		Print(string.Format("[SPT_WorldGarrison] Lista completa solicitada em %1 | grupos=%2 | unidades=%3; aguardando agentes",
			location.m_sName, location.m_aGroupIds.Count(), location.m_iDesiredUnits));
	}

	protected SCR_AIGroup SpawnGroup(Resource groupResource, vector buildingCenter, out int spawnedUnits)
	{
		spawnedUnits = 0;

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
		DebugLog(string.Format("Template de grupo instanciado | grupo=%1 | capacidadeSlotsUnidade=%2",
			group, capacity));
		if (capacity < 1)
		{
			Print("[SPT_WorldGarrison] Template de grupo nao possui slots de prefab de unidade", LogLevel.ERROR);
			return group;
		}

		group.SetDeleteWhenEmpty(false);
		spawnedUnits = SpawnGroupMembersDirect(group, buildingCenter);
		group.SetDeleteWhenEmpty(true);
		DebugLog(string.Format("Membros diretos finalizados | grupo=%1 | solicitados=%2 | criados=%3 | agentesAtuais=%4",
			group, capacity, spawnedUnits, group.GetAgentsCount()));
		return group;
	}

	protected int SpawnGroupMembersDirect(notnull SCR_AIGroup group, vector center)
	{
		int spawnedCount;
		int slotCount = group.m_aUnitPrefabSlots.Count();
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

			float angle = i * 137.5 * Math.PI / 180.0;
			int radiusStep = i % 3;
			float radius = 1.5 + 0.5 * radiusStep;
			vector memberPosition = center + Vector(Math.Cos(angle) * radius, 0, Math.Sin(angle) * radius);
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

	protected void WaitForGroupMembers(
		SPT_GarrisonLocation location,
		SCR_AIGroup group,
		vector buildingCenter,
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

		DebugLog(string.Format("Enviando grupo populado para guarnicao SPT | grupo=%1 | agentes=%2 | esperado=%3 | centro=%4",
			group, agentCount, expectedMembers, buildingCenter));
		SPT_AIGarrisonHelper.GarrisonGroup(group, buildingCenter, m_fBuildingSearchRadius, m_sPatrolWaypointPrefab);
		CompletePendingGroup(location, true);
	}

	protected void DespawnLocation(notnull SPT_GarrisonLocation location)
	{
		foreach (EntityID groupId : location.m_aGroupIds)
		{
			IEntity groupEntity = GetGame().GetWorld().FindEntityByID(groupId);
			SCR_AIGroup group = SCR_AIGroup.Cast(groupEntity);
			if (group)
				DeleteGroup(group);
		}

		int groupCount = location.m_aGroupIds.Count();
		location.m_aGroupIds.Clear();
		location.m_bActive = false;
		location.m_bSpawning = false;
		location.m_iPendingGroups = 0;
		location.m_iSuccessfulGroups = 0;
		Print(string.Format("[SPT_WorldGarrison] Despawn de %1 (%2 grupos)", location.m_sName, groupCount));
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

		location.m_bSpawning = false;
		location.m_bActive = location.m_iSuccessfulGroups > 0;

		if (location.m_bActive)
		{
			Print(string.Format("[SPT_WorldGarrison] Guarnicao pronta em %1 | gruposBemSucedidos=%2 | gruposRastreados=%3",
				location.m_sName, location.m_iSuccessfulGroups, location.m_aGroupIds.Count()));
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
			location.m_aGroupIds.Remove(index);
	}

	protected void PruneMissingGroupIds(notnull SPT_GarrisonLocation location)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		for (int i = location.m_aGroupIds.Count() - 1; i >= 0; i--)
		{
			if (!world.FindEntityByID(location.m_aGroupIds[i]))
				location.m_aGroupIds.Remove(i);
		}
	}

	protected void DebugLog(string message)
	{
		if (!m_bDebug)
			return;

		Print("[SPT_WorldGarrison][DEBUG] " + message);
	}

	protected void DeleteGroup(notnull SCR_AIGroup group)
	{
		SPT_AIGarrisonHelper.UngarrisonGroup(group);

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
	}

	protected int GetSelectedGroupPrefabCount()
	{
		if (m_eFaction == SPT_EGarrisonFaction.BLUFOR)
		{
			if (!m_aBluforGroupPrefabs)
				return 0;
			return m_aBluforGroupPrefabs.Count();
		}

		if (m_eFaction == SPT_EGarrisonFaction.INDEPENDENT)
		{
			if (!m_aIndependentGroupPrefabs)
				return 0;
			return m_aIndependentGroupPrefabs.Count();
		}

		if (!m_aOpforGroupPrefabs)
			return 0;
		return m_aOpforGroupPrefabs.Count();
	}

	protected array<ResourceName> GetSelectedGroupPrefabs()
	{
		if (m_eFaction == SPT_EGarrisonFaction.BLUFOR)
			return m_aBluforGroupPrefabs;
		if (m_eFaction == SPT_EGarrisonFaction.INDEPENDENT)
			return m_aIndependentGroupPrefabs;
		return m_aOpforGroupPrefabs;
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
