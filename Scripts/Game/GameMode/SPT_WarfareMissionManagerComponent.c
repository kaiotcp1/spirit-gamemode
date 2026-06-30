class SPT_WarfareMissionRuntime : Managed
{
	string m_sPointId;
	string m_sDisplayName;
	vector m_vCenter;
	ref SPT_WarfareMissionConfig m_Config;
	SPT_EWarfareMissionState m_eState;
	bool m_bActivationFailed;
	EntityID m_ScenarioEntityId;
	EntityID m_ObjectiveEntityId;
}

[ComponentEditorProps(category: "SPT/GameMode", description: "Gerencia missoes independentes por WarfarePoint.")]
class SPT_WarfareMissionManagerComponentClass : ScriptComponentClass
{
}

class SPT_WarfareMissionManagerComponent : ScriptComponent
{
	protected const int MONITOR_INTERVAL_MS = 1000;
	protected const int TERRAIN_CANDIDATE_COUNT = 24;
	protected const float TERRAIN_SAMPLE_DISTANCE = 5.0;

	protected static SPT_WarfareMissionManagerComponent s_Instance;

	protected ref map<string, ref SPT_WarfareMissionRuntime> m_mMissions;
	protected ref array<string> m_aMissionOrder;
	protected ref map<string, SPT_EWarfareMissionState> m_mClientStates;
	protected ref map<string, SPT_EWarfareMissionType> m_mClientTypes;

	//! Copiada explicitamente pelo SaveData do EPF.
	protected ref array<string> m_aCompletedPointIds;

	static SPT_WarfareMissionManagerComponent GetInstance()
	{
		return s_Instance;
	}

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		s_Instance = this;
		m_mMissions = new map<string, ref SPT_WarfareMissionRuntime>();
		m_aMissionOrder = new array<string>();
		m_mClientStates = new map<string, SPT_EWarfareMissionState>();
		m_mClientTypes = new map<string, SPT_EWarfareMissionType>();
		m_aCompletedPointIds = new array<string>();

		if (SCR_Global.IsEditMode() || !Replication.IsServer())
			return;

		GetGame().GetCallqueue().CallLater(MonitorMissions, MONITOR_INTERVAL_MS, true);
	}

	void ~SPT_WarfareMissionManagerComponent()
	{
		if (Replication.IsServer())
			CleanupScenarios();

		if (s_Instance == this)
			s_Instance = null;
	}

	void ResetRegistrations()
	{
		if (!Replication.IsServer())
			return;

		CleanupScenarios();
		m_mMissions.Clear();
		m_aMissionOrder.Clear();
	}

	bool RegisterMission(string pointId, string displayName, vector center, SPT_WarfareMissionConfig config)
	{
		if (!Replication.IsServer() || !config || !config.m_bEnabled)
			return false;
		if (config.m_eType == SPT_EWarfareMissionType.NONE)
			return false;
		if (pointId.IsEmpty() || m_mMissions.Contains(pointId))
			return false;

		SPT_WarfareMissionRuntime runtime = new SPT_WarfareMissionRuntime();
		runtime.m_sPointId = pointId;
		runtime.m_sDisplayName = displayName;
		runtime.m_vCenter = center;
		runtime.m_Config = config;
		runtime.m_eState = SPT_EWarfareMissionState.INACTIVE;
		if (IsPersistedCompleted(pointId))
			runtime.m_eState = SPT_EWarfareMissionState.COMPLETED;

		m_mMissions.Set(pointId, runtime);
		m_aMissionOrder.Insert(pointId);
		BroadcastMissionState(runtime, false);
		return true;
	}

	SPT_EWarfareMissionState GetMissionState(string pointId)
	{
		if (Replication.IsServer())
		{
			SPT_WarfareMissionRuntime runtime = m_mMissions.Get(pointId);
			if (runtime)
				return runtime.m_eState;
		}
		else if (m_mClientStates.Contains(pointId))
		{
			return m_mClientStates.Get(pointId);
		}

		return SPT_EWarfareMissionState.INACTIVE;
	}

	SPT_EWarfareMissionType GetMissionType(string pointId)
	{
		if (Replication.IsServer())
		{
			SPT_WarfareMissionRuntime runtime = m_mMissions.Get(pointId);
			if (runtime && runtime.m_Config)
				return runtime.m_Config.m_eType;
		}
		else if (m_mClientTypes.Contains(pointId))
		{
			return m_mClientTypes.Get(pointId);
		}

		return SPT_EWarfareMissionType.NONE;
	}

	bool HasMission(string pointId)
	{
		if (Replication.IsServer())
			return m_mMissions.Contains(pointId);

		return m_mClientTypes.Contains(pointId);
	}

	bool CanStartAmmoCacheDestruction(string pointId, IEntity scenarioEntity)
	{
		if (!Replication.IsServer() || pointId.IsEmpty() || !scenarioEntity || scenarioEntity.IsDeleted())
			return false;

		SPT_WarfareMissionRuntime runtime = m_mMissions.Get(pointId);
		if (!runtime || !runtime.m_Config)
			return false;
		if (runtime.m_Config.m_eType != SPT_EWarfareMissionType.DESTROY_AMMO_CACHE)
			return false;
		if (runtime.m_ScenarioEntityId != scenarioEntity.GetID())
			return false;

		return runtime.m_eState == SPT_EWarfareMissionState.ACTIVE;
	}

	bool CanInteractWithMission(string pointId, SPT_EWarfareMissionType missionType, IEntity scenarioEntity)
	{
		if (!Replication.IsServer() || pointId.IsEmpty() || !scenarioEntity || scenarioEntity.IsDeleted())
			return false;

		SPT_WarfareMissionRuntime runtime = m_mMissions.Get(pointId);
		if (!runtime || !runtime.m_Config)
			return false;
		if (runtime.m_Config.m_eType != missionType)
			return false;
		if (runtime.m_ScenarioEntityId != scenarioEntity.GetID())
			return false;

		return runtime.m_eState == SPT_EWarfareMissionState.ACTIVE;
	}

	protected void MonitorMissions()
	{
		if (!Replication.IsServer())
			return;

		foreach (string pointId : m_aMissionOrder)
		{
			SPT_WarfareMissionRuntime runtime = m_mMissions.Get(pointId);
			if (!runtime)
				continue;

			if (runtime.m_eState == SPT_EWarfareMissionState.INACTIVE)
			{
				if (!runtime.m_bActivationFailed &&
					IsPlayerNear(runtime.m_vCenter, runtime.m_Config.m_fActivationDistance))
					ActivateMission(runtime);
				continue;
			}

			if (runtime.m_eState == SPT_EWarfareMissionState.ACTIVE && IsObjectiveDestroyed(runtime))
				CompleteMission(runtime);
		}
	}

	protected bool IsPlayerNear(vector center, float distance)
	{
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return false;

		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);
		float distanceSq = distance * distance;
		foreach (int playerId : playerIds)
		{
			IEntity player = playerManager.GetPlayerControlledEntity(playerId);
			if (!player || player.IsDeleted())
				continue;

			vector delta = player.GetOrigin() - center;
			if (delta.LengthSq() <= distanceSq)
				return true;
		}

		return false;
	}

	protected void ActivateMission(SPT_WarfareMissionRuntime runtime)
	{
		if (!runtime || runtime.m_eState != SPT_EWarfareMissionState.INACTIVE)
			return;

		if (runtime.m_Config.m_rScenarioPrefab.IsEmpty())
		{
			Print(string.Format("[SPT_WarfareMission] Missao %1 sem prefab de cenario.", runtime.m_sPointId), LogLevel.ERROR);
			runtime.m_bActivationFailed = true;
			return;
		}

		vector position;
		if (!FindTerrainPosition(runtime, position))
		{
			Print(string.Format("[SPT_WarfareMission] Nenhum terreno valido | pointId=%1 | centro=%2 | raio=%3 | inclinacaoMax=%4",
				runtime.m_sPointId, runtime.m_vCenter, runtime.m_Config.m_fTerrainSearchRadius,
				runtime.m_Config.m_fMaximumSlopeDegrees), LogLevel.ERROR);
			runtime.m_bActivationFailed = true;
			return;
		}

		Resource resource = Resource.Load(runtime.m_Config.m_rScenarioPrefab);
		if (!resource)
		{
			Print(string.Format("[SPT_WarfareMission] Prefab invalido | pointId=%1 | prefab=%2",
				runtime.m_sPointId, runtime.m_Config.m_rScenarioPrefab), LogLevel.ERROR);
			runtime.m_bActivationFailed = true;
			return;
		}

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = position;
		IEntity scenarioEntity = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
		if (!scenarioEntity)
		{
			Print(string.Format("[SPT_WarfareMission] Falha ao criar cenario | pointId=%1", runtime.m_sPointId), LogLevel.ERROR);
			runtime.m_bActivationFailed = true;
			return;
		}

		SPT_WarfareMissionScenarioComponent scenario = SPT_WarfareMissionScenarioComponent.Cast(
			scenarioEntity.FindComponent(SPT_WarfareMissionScenarioComponent));
		if (!scenario)
		{
			Print(string.Format("[SPT_WarfareMission] Prefab sem SPT_WarfareMissionScenarioComponent | pointId=%1",
				runtime.m_sPointId), LogLevel.ERROR);
			SCR_EntityHelper.DeleteEntityAndChildren(scenarioEntity);
			runtime.m_bActivationFailed = true;
			return;
		}

		RplComponent replication = RplComponent.Cast(scenarioEntity.FindComponent(RplComponent));
		if (!replication)
		{
			Print(string.Format("[SPT_WarfareMission] Cenario sem RplComponent no prefab-raiz | pointId=%1",
				runtime.m_sPointId), LogLevel.ERROR);
			SCR_EntityHelper.DeleteEntityAndChildren(scenarioEntity);
			runtime.m_bActivationFailed = true;
			return;
		}

		scenario.SnapMarkedEntitiesToTerrain();
		SPT_WarfareMissionObjectiveComponent objective = scenario.FindObjective();
		if (!objective)
		{
			Print(string.Format("[SPT_WarfareMission] Cenario sem objetivo destrutivel | pointId=%1",
				runtime.m_sPointId), LogLevel.ERROR);
			SCR_EntityHelper.DeleteEntityAndChildren(scenarioEntity);
			runtime.m_bActivationFailed = true;
			return;
		}

		SPT_AmmoCacheDestructionComponent destruction = SPT_AmmoCacheDestructionComponent.Cast(
			scenarioEntity.FindComponent(SPT_AmmoCacheDestructionComponent));
		if (runtime.m_Config.m_eType == SPT_EWarfareMissionType.DESTROY_AMMO_CACHE && !destruction)
		{
			Print(string.Format("[SPT_WarfareMission] Ammo Cache sem controlador na raiz | pointId=%1",
				runtime.m_sPointId), LogLevel.ERROR);
			SCR_EntityHelper.DeleteEntityAndChildren(scenarioEntity);
			runtime.m_bActivationFailed = true;
			return;
		}
		if (destruction)
			destruction.Initialize(runtime.m_sPointId);

		SPT_WarfareMissionInteractionComponent interaction = SPT_WarfareMissionInteractionComponent.Cast(
			scenarioEntity.FindComponent(SPT_WarfareMissionInteractionComponent));
		if (RequiresInteraction(runtime.m_Config.m_eType) && !interaction)
		{
			Print(string.Format("[SPT_WarfareMission] Missao interativa sem controlador na raiz | pointId=%1 | tipo=%2",
				runtime.m_sPointId, runtime.m_Config.m_eType), LogLevel.ERROR);
			SCR_EntityHelper.DeleteEntityAndChildren(scenarioEntity);
			runtime.m_bActivationFailed = true;
			return;
		}
		if (interaction)
			interaction.Initialize(runtime.m_sPointId, runtime.m_Config.m_eType);

		objective.Initialize(runtime.m_sPointId);
		runtime.m_ScenarioEntityId = scenarioEntity.GetID();
		runtime.m_ObjectiveEntityId = objective.GetOwner().GetID();
		runtime.m_eState = SPT_EWarfareMissionState.ACTIVE;
		BroadcastMissionState(runtime, true);

		Print(string.Format("[SPT_WarfareMission] Missao ativada | pointId=%1 | posicao=%2",
			runtime.m_sPointId, position));
	}

	protected bool FindTerrainPosition(SPT_WarfareMissionRuntime runtime, out vector outPosition)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		float radius = runtime.m_Config.m_fTerrainSearchRadius;
		for (int i = 0; i <= TERRAIN_CANDIDATE_COUNT; i++)
		{
			vector candidate = runtime.m_vCenter;
			if (i > 0 && radius > 0.0)
			{
				int ringIndex = ((i - 1) / 8) + 1;
				int angleIndex = (i - 1) % 8;
				float ringRadius = radius * ringIndex / 3.0;
				float angle = angleIndex * 45.0 * Math.DEG2RAD;
				candidate[0] = candidate[0] + Math.Cos(angle) * ringRadius;
				candidate[2] = candidate[2] + Math.Sin(angle) * ringRadius;
			}

			if (!IsTerrainSlopeValid(world, candidate, runtime.m_Config.m_fMaximumSlopeDegrees))
				continue;

			candidate[1] = world.GetSurfaceY(candidate[0], candidate[2]);
			outPosition = candidate;
			return true;
		}

		return false;
	}

	protected bool IsTerrainSlopeValid(BaseWorld world, vector position, float maxSlopeDegrees)
	{
		float centerY = world.GetSurfaceY(position[0], position[2]);
		float maxDelta = 0.0;
		float sampleY = world.GetSurfaceY(position[0] + TERRAIN_SAMPLE_DISTANCE, position[2]);
		maxDelta = Math.Max(maxDelta, Math.AbsFloat(sampleY - centerY));
		sampleY = world.GetSurfaceY(position[0] - TERRAIN_SAMPLE_DISTANCE, position[2]);
		maxDelta = Math.Max(maxDelta, Math.AbsFloat(sampleY - centerY));
		sampleY = world.GetSurfaceY(position[0], position[2] + TERRAIN_SAMPLE_DISTANCE);
		maxDelta = Math.Max(maxDelta, Math.AbsFloat(sampleY - centerY));
		sampleY = world.GetSurfaceY(position[0], position[2] - TERRAIN_SAMPLE_DISTANCE);
		maxDelta = Math.Max(maxDelta, Math.AbsFloat(sampleY - centerY));

		float allowedDelta = Math.Tan(maxSlopeDegrees * Math.DEG2RAD) * TERRAIN_SAMPLE_DISTANCE;
		return maxDelta <= allowedDelta;
	}

	protected bool IsObjectiveDestroyed(SPT_WarfareMissionRuntime runtime)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		IEntity objectiveEntity = world.FindEntityByID(runtime.m_ObjectiveEntityId);
		if (!objectiveEntity || objectiveEntity.IsDeleted())
			return true;

		SPT_WarfareMissionObjectiveComponent objective = SPT_WarfareMissionObjectiveComponent.Cast(
			objectiveEntity.FindComponent(SPT_WarfareMissionObjectiveComponent));
		if (!objective)
			return true;

		return objective.IsCompleted();
	}

	protected bool RequiresInteraction(SPT_EWarfareMissionType missionType)
	{
		return missionType == SPT_EWarfareMissionType.SABOTAGE_COMMUNICATIONS ||
			missionType == SPT_EWarfareMissionType.DESTROY_FUEL_DEPOT ||
			missionType == SPT_EWarfareMissionType.STEAL_INTELLIGENCE ||
			missionType == SPT_EWarfareMissionType.DEFEND_TRANSMITTER;
	}

	protected void CompleteMission(SPT_WarfareMissionRuntime runtime)
	{
		if (!runtime || runtime.m_eState != SPT_EWarfareMissionState.ACTIVE)
			return;

		runtime.m_eState = SPT_EWarfareMissionState.COMPLETED;
		if (!IsPersistedCompleted(runtime.m_sPointId))
			m_aCompletedPointIds.Insert(runtime.m_sPointId);

		SavePersistence();
		BroadcastMissionState(runtime, true);
		Print(string.Format("[SPT_WarfareMission] Missao concluida | pointId=%1", runtime.m_sPointId));
	}

	protected void BroadcastMissionState(SPT_WarfareMissionRuntime runtime, bool notify)
	{
		int notifyRaw = 0;
		if (notify)
			notifyRaw = 1;

		RpcDo_MissionState(runtime.m_sPointId, runtime.m_sDisplayName,
			runtime.m_Config.m_eType, runtime.m_eState, notifyRaw);
		Rpc(RpcDo_MissionState, runtime.m_sPointId, runtime.m_sDisplayName,
			runtime.m_Config.m_eType, runtime.m_eState, notifyRaw);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_MissionState(string pointId, string displayName, int typeRaw, int stateRaw, int notifyRaw)
	{
		SPT_EWarfareMissionState oldState = SPT_EWarfareMissionState.INACTIVE;
		if (m_mClientStates.Contains(pointId))
			oldState = m_mClientStates.Get(pointId);

		m_mClientTypes.Set(pointId, typeRaw);
		m_mClientStates.Set(pointId, stateRaw);
		if (notifyRaw == 0 || oldState == stateRaw)
			return;

		string objectiveName = GetMissionObjectiveName(typeRaw);
		string message;
		if (stateRaw == SPT_EWarfareMissionState.ACTIVE)
			message = string.Format("Missao em %1: %2.", displayName, objectiveName);
		else if (stateRaw == SPT_EWarfareMissionState.COMPLETED)
			message = string.Format("Missao concluida em %1: %2.", displayName, objectiveName);

		if (message.IsEmpty())
			return;

		SCR_HintManagerComponent hintManager = SCR_HintManagerComponent.GetInstance();
		if (hintManager)
			hintManager.ShowCustom(message);
	}

	protected string GetMissionObjectiveName(SPT_EWarfareMissionType missionType)
	{
		switch (missionType)
		{
			case SPT_EWarfareMissionType.DESTROY_AMMO_CACHE: return "destrua a ammo cache";
			case SPT_EWarfareMissionType.SABOTAGE_COMMUNICATIONS: return "sabote o posto de comunicacao";
			case SPT_EWarfareMissionType.DESTROY_FUEL_DEPOT: return "destrua o deposito de combustivel";
			case SPT_EWarfareMissionType.STEAL_INTELLIGENCE: return "roube a inteligencia inimiga";
			case SPT_EWarfareMissionType.ELIMINATE_OFFICER: return "elimine o oficial inimigo";
			case SPT_EWarfareMissionType.DEFEND_TRANSMITTER: return "ative e defenda o transmissor";
		}
		return "conclua o objetivo";
	}

	override bool RplSave(ScriptBitWriter writer)
	{
		int count = 0;
		if (m_aMissionOrder)
			count = m_aMissionOrder.Count();
		writer.WriteInt(count);

		foreach (string pointId : m_aMissionOrder)
		{
			SPT_WarfareMissionRuntime runtime = m_mMissions.Get(pointId);
			writer.WriteString(pointId);
			writer.WriteInt(runtime.m_Config.m_eType);
			writer.WriteInt(runtime.m_eState);
		}

		return true;
	}

	override bool RplLoad(ScriptBitReader reader)
	{
		m_mClientStates.Clear();
		m_mClientTypes.Clear();

		int count;
		reader.ReadInt(count);
		for (int i = 0; i < count; i++)
		{
			string pointId;
			int typeRaw;
			int stateRaw;
			reader.ReadString(pointId);
			reader.ReadInt(typeRaw);
			reader.ReadInt(stateRaw);
			m_mClientTypes.Set(pointId, typeRaw);
			m_mClientStates.Set(pointId, stateRaw);
		}

		return true;
	}

	void CopyCompletedPointIds(notnull array<string> outIds)
	{
		outIds.Clear();
		foreach (string pointId : m_aCompletedPointIds)
			outIds.Insert(pointId);
	}

	void RestoreCompletedPointIds(array<string> ids)
	{
		m_aCompletedPointIds.Clear();
		if (!ids)
			return;

		foreach (string pointId : ids)
		{
			if (!pointId.IsEmpty() && m_aCompletedPointIds.Find(pointId) < 0)
				m_aCompletedPointIds.Insert(pointId);
		}

		foreach (string registeredId : m_aMissionOrder)
		{
			SPT_WarfareMissionRuntime runtime = m_mMissions.Get(registeredId);
			if (runtime && IsPersistedCompleted(registeredId))
				runtime.m_eState = SPT_EWarfareMissionState.COMPLETED;
		}
	}

	protected bool IsPersistedCompleted(string pointId)
	{
		return m_aCompletedPointIds.Find(pointId) >= 0;
	}

	protected void SavePersistence()
	{
		#ifndef PLATFORM_CONSOLE
		IEntity owner = GetOwner();
		if (!owner)
			return;

		EPF_PersistenceComponent persistence = EPF_PersistenceComponent.Cast(
			owner.FindComponent(EPF_PersistenceComponent));
		if (persistence)
			persistence.Save();
		#endif
	}

	protected void CleanupScenarios()
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world || !m_mMissions)
			return;

		foreach (string pointId : m_aMissionOrder)
		{
			SPT_WarfareMissionRuntime runtime = m_mMissions.Get(pointId);
			if (!runtime)
				continue;

			IEntity scenario = world.FindEntityByID(runtime.m_ScenarioEntityId);
			if (scenario && !scenario.IsDeleted())
				SCR_EntityHelper.DeleteEntityAndChildren(scenario);
		}
	}
}
