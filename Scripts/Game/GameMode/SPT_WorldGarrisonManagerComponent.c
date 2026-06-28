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

	[Attribute("15", desc: "Minimum units generated per active location")]
	protected int m_iMinimumUnits;

	[Attribute("15", desc: "Maximum units generated per active location. Set above minimum to randomize, for example 15 to 30.")]
	protected int m_iMaximumUnits;

	[Attribute("500", desc: "Game Master AI budget used by the garrison. Increase this when many locations can be active simultaneously. Set to 0 to keep the scenario budget unchanged.")]
	protected int m_iGameMasterAIBudget;

	[Attribute("3", desc: "Maximum soldiers assigned to one building/group")]
	protected int m_iUnitsPerBuilding;

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
		"{D807C7047E818488}Prefabs/Groups/BLUFOR/Group_US_SniperTeam.et"
	};

	[Attribute("{22F33D3EC8F281AB}Prefabs/Groups/INDFOR/Group_FIA_MachineGunTeam.et", UIWidgets.ResourceNamePicker, desc: "INDEPENDENT group templates. Do not use _NotSpawned ambient patrol prefabs.", params: "et class=SCR_AIGroup")]
	protected ref array<ResourceName> m_aIndependentGroupPrefabs = {
		"{22F33D3EC8F281AB}Prefabs/Groups/INDFOR/Group_FIA_MachineGunTeam.et"
	};

	protected ref array<ref SPT_GarrisonLocation> m_aLocations = new array<ref SPT_GarrisonLocation>();
	protected int m_iDebugUpdateCounter;

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		bool editMode = SCR_Global.IsEditMode();
		bool isServer = Replication.IsServer();
		Print(string.Format("[SPT_WorldGarrison] OnPostInit | owner=%1 | editMode=%2 | server=%3 | debug=%4",
			owner, editMode, isServer, m_bDebug));

		if (editMode)
		{
			DebugLog("Initialization skipped because the world is in edit mode.");
			return;
		}

		if (!isServer)
		{
			DebugLog("Initialization skipped on client; the server owns garrison spawning.");
			return;
		}

		ValidateSettings();
		DebugLog(string.Format("Initialization scheduled in 1000 ms | faction=%1 | configuredGroupPrefabs=%2",
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

		if (m_iMinimumUnits < 1)
			m_iMinimumUnits = 1;

		if (m_iMaximumUnits < m_iMinimumUnits)
			m_iMaximumUnits = m_iMinimumUnits;

		if (m_iGameMasterAIBudget < 0)
			m_iGameMasterAIBudget = 0;

		if (m_iUnitsPerBuilding < 1)
			m_iUnitsPerBuilding = 1;

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
				DebugLog(string.Format("Game Master budget component not ready; retrying (%1 attempts left).", retriesLeft));
				GetGame().GetCallqueue().CallLater(ConfigureGameMasterBudget, 1000, false, retriesLeft - 1);
				return;
			}

			Print("[SPT_WorldGarrison] Could not find SCR_BudgetEditorComponent; Game Master AI budget was not changed.", LogLevel.WARNING);
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

			DebugLog(string.Format("Game Master budget updated | component=%1 | AI=%2->%3 | AI_SERVER=%4->%5",
				budgetComponent, previousAI, m_iGameMasterAIBudget, previousServerAI, m_iGameMasterAIBudget));
		}

		Print(string.Format("[SPT_WorldGarrison] Game Master AI budget set to %1 on %2 component(s).",
			m_iGameMasterAIBudget, configuredComponents));
	}

	protected void InitializeLocations()
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
		{
			Print("[SPT_WorldGarrison] Initialization failed: game world is null.", LogLevel.ERROR);
			return;
		}

		DebugLog("Scanning map descriptors for settlements and strategic sites...");

		world.QueryEntitiesBySphere(
			vector.Zero,
			99999999,
			CollectLocation,
			FilterLocation,
			EQueryEntitiesFlags.ALL
		);

		Print(string.Format("[SPT_WorldGarrison] Registered %1 locations | faction %2 | spawn %3m | despawn %4m | units %5-%6",
			m_aLocations.Count(), m_eFaction, m_fSpawnDistance, m_fDespawnDistance, m_iMinimumUnits, m_iMaximumUnits));

		if (m_aLocations.IsEmpty())
			Print("[SPT_WorldGarrison] No supported map locations were found. Check descriptor types and include settings.", LogLevel.WARNING);

		UpdateLocations();
		int updateIntervalMs = Math.Round(m_fUpdateInterval * 1000);
		GetGame().GetCallqueue().CallLater(UpdateLocations, updateIntervalMs, true);
		DebugLog(string.Format("Distance update loop started with interval %1 ms.", updateIntervalMs));
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
		DebugLog(string.Format("Location registered | name=%1 | type=%2 | position=%3",
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
			DebugLog(string.Format("Distance update | playersWithPosition=%1 | locations=%2",
				playerPositions.Count(), m_aLocations.Count()));

		if (playerPositions.IsEmpty())
		{
			if (logThisUpdate)
				DebugLog("No valid player-controlled or main entities were found.");

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
				DebugLog(string.Format("Location distance | name=%1 | distance=%2m | active=%3 | unavailable=%4",
					location.m_sName, Math.Sqrt(nearestPlayerSq), location.m_bActive, location.m_bUnavailable));
			}

			if (!location.m_bActive && nearestPlayerSq <= spawnDistanceSq)
			{
				DebugLog(string.Format("Spawn threshold reached for %1.", location.m_sName));
				SpawnLocation(location);
			}
			else if (location.m_bActive && nearestPlayerSq > despawnDistanceSq)
			{
				DebugLog(string.Format("Despawn threshold reached for %1.", location.m_sName));
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
			DebugLog("PlayerManager is null.");
			return;
		}

		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);
		if (m_bDebug && m_iDebugUpdateCounter % 5 == 0)
			DebugLog(string.Format("PlayerManager returned %1 connected players.", playerIds.Count()));

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
					DebugLog(string.Format("Player position | id=%1 | source=%2 | class=%3 | position=%4",
						playerId, source, playerEntity.ClassName(), playerEntity.GetOrigin()));
				}
			}
			else
			{
				DebugLog(string.Format("No spatial entity found for player id %1.", playerId));
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
		DebugLog(string.Format("SpawnLocation started | name=%1 | center=%2 | configuredGroupPrefabs=%3",
			location.m_sName, location.m_vCenter, GetSelectedGroupPrefabCount()));

		if (GetSelectedGroupPrefabCount() < 1)
		{
			Print(string.Format("[SPT_WorldGarrison] No group prefabs configured for faction %1.", m_eFaction), LogLevel.ERROR);
			return;
		}

		array<vector> buildings = {};
		SPT_GarrisonDetector.CollectBuildingCenters(
			GetGame().GetWorld(),
			location.m_vCenter,
			m_fBuildingSearchRadius,
			buildings
		);
		DebugLog(string.Format("Building detector returned %1 structural centers within %2m.",
			buildings.Count(), m_fBuildingSearchRadius));

		if (buildings.IsEmpty())
		{
			Print(string.Format("[SPT_WorldGarrison] No usable buildings around %1 at %2",
				location.m_sName, location.m_vCenter), LogLevel.WARNING);
			location.m_bUnavailable = true;
			return;
		}

		ShufflePositions(buildings);

		int desiredUnits = Math.RandomInt(m_iMinimumUnits, m_iMaximumUnits + 1);
		location.m_bSpawning = true;
		location.m_iPendingGroups = 0;
		location.m_iSuccessfulGroups = 0;
		location.m_iDesiredUnits = desiredUnits;
		int remainingUnits = desiredUnits;
		int buildingIndex = 0;
		int spawnAttempts = 0;
		int maximumSpawnAttempts = desiredUnits + buildings.Count();

		while (remainingUnits > 0 && spawnAttempts < maximumSpawnAttempts)
		{
			spawnAttempts++;
			int requestedUnits = m_iUnitsPerBuilding;
			if (requestedUnits > remainingUnits)
				requestedUnits = remainingUnits;

			int spawnedUnits;
			int selectedBuilding = buildingIndex % buildings.Count();
			vector buildingCenter = buildings[selectedBuilding];
			ResourceName groupPrefab;
			Resource groupResource;
			if (!SelectRandomGroupResource(groupPrefab, groupResource))
			{
				Print(string.Format("[SPT_WorldGarrison] None of the %1 configured group prefabs for faction %2 could be loaded.",
					GetSelectedGroupPrefabCount(), m_eFaction), LogLevel.ERROR);
				break;
			}

			DebugLog(string.Format("Random group prefab selected | attempt=%1 | prefab=%2",
				spawnAttempts, groupPrefab));
			SCR_AIGroup group = SpawnGroup(groupResource, buildingCenter, requestedUnits, spawnedUnits);
			buildingIndex++;
			if (!group)
			{
				DebugLog(string.Format("Group spawn attempt %1 failed at %2.", spawnAttempts, buildingCenter));
				continue;
			}

			if (spawnedUnits < 1)
			{
				DebugLog(string.Format("Spawned group %1 reported zero requested members; deleting it.", group));
				DeleteGroup(group);
				continue;
			}

			location.m_aGroupIds.Insert(group.GetID());
			location.m_iPendingGroups++;
			remainingUnits = remainingUnits - spawnedUnits;
			DebugLog(string.Format("Group created and members requested | id=%1 | requested=%2 | center=%3 | remaining=%4",
				group.GetID(), spawnedUnits, buildingCenter, remainingUnits));
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
			Print(string.Format("[SPT_WorldGarrison] Failed to create any groups for %1.", location.m_sName), LogLevel.ERROR);
			return;
		}

		int actualUnits = desiredUnits - remainingUnits;
		Print(string.Format("[SPT_WorldGarrison] Requested %1/%2 units in %3 (%4 groups); waiting for agents",
			actualUnits, desiredUnits, location.m_sName, location.m_aGroupIds.Count()));
	}

	protected SCR_AIGroup SpawnGroup(Resource groupResource, vector buildingCenter, int requestedUnits, out int spawnedUnits)
	{
		spawnedUnits = 0;

		EntitySpawnParams spawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		Math3D.MatrixIdentity4(spawnParams.Transform);
		buildingCenter[1] = GetGame().GetWorld().GetSurfaceY(buildingCenter[0], buildingCenter[2]) + 0.2;
		spawnParams.Transform[3] = buildingCenter;

		IEntity spawnedEntity = GetGame().SpawnEntityPrefab(groupResource, GetGame().GetWorld(), spawnParams);

		SCR_AIGroup group = SCR_AIGroup.Cast(spawnedEntity);
		if (!group)
		{
			DebugLog(string.Format("SpawnEntityPrefab did not return SCR_AIGroup | entity=%1", spawnedEntity));
			if (spawnedEntity)
				SCR_EntityHelper.DeleteEntityAndChildren(spawnedEntity);
			return null;
		}

		int capacity = group.m_aUnitPrefabSlots.Count();
		DebugLog(string.Format("Group template instantiated | group=%1 | unitSlotCapacity=%2 | requested=%3",
			group, capacity, requestedUnits));
		if (capacity < 1)
		{
			Print("[SPT_WorldGarrison] Group template has no unit prefab slots", LogLevel.ERROR);
			return group;
		}

		int unitsToSpawn = requestedUnits;
		if (unitsToSpawn > capacity)
			unitsToSpawn = capacity;

		group.SetNumberOfMembersToSpawn(unitsToSpawn);
		group.SpawnAllImmediately();
		spawnedUnits = unitsToSpawn;
		DebugLog(string.Format("SpawnAllImmediately called | group=%1 | requestedMembers=%2 | currentAgents=%3 | queue=%4 | initializing=%5",
			group, unitsToSpawn, group.GetAgentsCount(), group.GetSpawnQueueSize(), group.IsInitializing()));
		return group;
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
			DebugLog(string.Format("Garrison callback skipped: group no longer exists at center %1.", buildingCenter));
			CompletePendingGroup(location, false);
			return;
		}

		int agentCount = group.GetAgentsCount();
		if (agentCount < 1 && pollsLeft > 0)
		{
			DebugLog(string.Format("Waiting for group members | group=%1 | agents=%2 | queue=%3 | initializing=%4 | pollsLeft=%5",
				group, agentCount, group.GetSpawnQueueSize(), group.IsInitializing(), pollsLeft));
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
			Print(string.Format("[SPT_WorldGarrison] Group failed to create members after 5 seconds | group=%1 | center=%2",
				group, buildingCenter), LogLevel.ERROR);
			RemoveGroupId(location, group.GetID());
			DeleteGroup(group);
			CompletePendingGroup(location, false);
			return;
		}

		DebugLog(string.Format("Sending populated group to SPT garrison | group=%1 | agents=%2 | expected=%3 | center=%4",
			group, agentCount, expectedMembers, buildingCenter));
		SPT_AIGarrisonHelper.GarrisonGroup(group, buildingCenter, 25);
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
		Print(string.Format("[SPT_WorldGarrison] Despawned %1 (%2 groups)", location.m_sName, groupCount));
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
			Print(string.Format("[SPT_WorldGarrison] Garrison ready in %1 | successfulGroups=%2 | trackedGroups=%3",
				location.m_sName, location.m_iSuccessfulGroups, location.m_aGroupIds.Count()));
		}
		else
		{
			Print(string.Format("[SPT_WorldGarrison] Garrison failed in %1; location will retry while a player remains nearby.",
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

	protected bool SelectRandomGroupResource(out ResourceName selectedPrefab, out Resource selectedResource)
	{
		selectedPrefab = ResourceName.Empty;
		selectedResource = null;

		array<ResourceName> prefabs;
		if (m_eFaction == SPT_EGarrisonFaction.BLUFOR)
			prefabs = m_aBluforGroupPrefabs;
		else if (m_eFaction == SPT_EGarrisonFaction.INDEPENDENT)
			prefabs = m_aIndependentGroupPrefabs;
		else
			prefabs = m_aOpforGroupPrefabs;

		if (!prefabs || prefabs.IsEmpty())
			return false;

		int startIndex = Math.RandomInt(0, prefabs.Count());
		for (int offset = 0; offset < prefabs.Count(); offset++)
		{
			int index = (startIndex + offset) % prefabs.Count();
			ResourceName candidate = prefabs[index];
			if (candidate.IsEmpty())
			{
				DebugLog(string.Format("Ignoring empty group prefab entry at index %1.", index));
				continue;
			}

			if (candidate.Contains("_NotSpawned"))
			{
				Print(string.Format("[SPT_WorldGarrison] Unsupported _NotSpawned group prefab ignored at index %1: %2",
					index, candidate), LogLevel.WARNING);
				continue;
			}

			Resource candidateResource = Resource.Load(candidate);
			if (!candidateResource)
			{
				Print(string.Format("[SPT_WorldGarrison] Could not load group prefab at index %1: %2",
					index, candidate), LogLevel.WARNING);
				continue;
			}

			selectedPrefab = candidate;
			selectedResource = candidateResource;
			return true;
		}

		return false;
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
