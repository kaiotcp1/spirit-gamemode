enum SPT_EWarfareMissionInteractionState
{
	IDLE,
	RUNNING,
	COMPLETED
}

//! Controlador autoritativo compartilhado por objetivos interativos.
[ComponentEditorProps(category: "SPT/Warfare/Mission", description: "Controla interacoes de sabotagem, coleta e defesa.")]
class SPT_WarfareMissionInteractionComponentClass : ScriptComponentClass
{
}

class SPT_WarfareMissionInteractionComponent : ScriptComponent
{
	protected const float MAX_INTERACTION_DISTANCE = 4.0;
	protected const int TIMER_TICK_MS = 1000;
	protected const int RUIN_REPLACEMENT_DELAY_MS = 500;
	protected const float EXPLOSION_CLEANUP_RADIUS = 30.0;

	[Attribute("10", desc: "Duracao da sabotagem ou defesa em segundos.")]
	protected int m_iDurationSeconds;

	[Attribute("20", desc: "Raio no qual um jogador deve permanecer durante a defesa.")]
	protected float m_fDefenseRadius;

	[Attribute("{B4569479BFB3DBA2}Prefabs/Weapons/Warheads/Explosions/Cinematic_Explosion.et",
		UIWidgets.ResourceNamePicker, desc: "Primeiro efeito da sequencia de explosao.", params: "et")]
	protected ResourceName m_rExplosionPrefab1;

	[Attribute("{BCE4E0823FCFBCB7}Prefabs/Weapons/Warheads/Explosions/Explosion_AmmoRack_Large.et",
		UIWidgets.ResourceNamePicker, desc: "Segundo efeito da sequencia de explosao.", params: "et")]
	protected ResourceName m_rExplosionPrefab2;

	[Attribute("{72BEEF40AF179763}Prefabs/Weapons/Warheads/Explosions/Explosion_Tnt_Large.et",
		UIWidgets.ResourceNamePicker, desc: "Terceiro efeito da sequencia de explosao.", params: "et")]
	protected ResourceName m_rExplosionPrefab3;

	[Attribute("{490CCFCB2ACC99CF}Prefabs/Weapons/Warheads/Explosions/Explosion_AmmoRack_Medium_Sound.et",
		UIWidgets.ResourceNamePicker, desc: "Som reproduzido localmente para todos os clientes.", params: "et")]
	protected ResourceName m_rExplosionSoundPrefab;

	[RplProp(onRplName: "OnStateChanged")]
	protected int m_iState;

	[RplProp()]
	protected int m_iRemainingSeconds;

	protected string m_sPointId;
	[RplProp()]
	protected int m_iMissionType;
	protected ref array<EntityID> m_aExplosionCleanupEntityIds;

	void SPT_WarfareMissionInteractionComponent(IEntityComponentSource src, IEntity ent, IEntity parent)
	{
		m_iState = SPT_EWarfareMissionInteractionState.IDLE;
		m_aExplosionCleanupEntityIds = new array<EntityID>();
	}

	void ~SPT_WarfareMissionInteractionComponent()
	{
		GetGame().GetCallqueue().Remove(TimerTick);
		GetGame().GetCallqueue().Remove(SpawnDelayedExplosion);
		GetGame().GetCallqueue().Remove(ReplaceRuinTargets);
	}

	void Initialize(string pointId, SPT_EWarfareMissionType missionType)
	{
		if (!Replication.IsServer())
			return;

		m_sPointId = pointId;
		m_iMissionType = missionType;
		Replication.BumpMe();
	}

	bool TryInteract(IEntity player)
	{
		if (!Replication.IsServer() || !player || player.IsDeleted())
			return false;
		if (m_iState != SPT_EWarfareMissionInteractionState.IDLE)
			return false;

		IEntity owner = GetOwner();
		if (!owner || owner.IsDeleted())
			return false;
		if (vector.Distance(player.GetOrigin(), owner.GetOrigin()) > MAX_INTERACTION_DISTANCE)
			return false;

		DamageManagerComponent playerDamage = DamageManagerComponent.Cast(
			player.FindComponent(DamageManagerComponent));
		if (playerDamage && playerDamage.GetHealthScaled() <= 0.0)
			return false;

		SPT_WarfareMissionManagerComponent manager = SPT_WarfareMissionManagerComponent.GetInstance();
		if (!manager || !manager.CanInteractWithMission(m_sPointId, m_iMissionType, owner))
			return false;

		if (m_iMissionType == SPT_EWarfareMissionType.STEAL_INTELLIGENCE)
		{
			CompleteInteraction();
			return true;
		}

		m_iDurationSeconds = Math.Max(m_iDurationSeconds, 1);
		m_iRemainingSeconds = m_iDurationSeconds;
		m_iState = SPT_EWarfareMissionInteractionState.RUNNING;
		Replication.BumpMe();
		OnStateChanged();
		GetGame().GetCallqueue().CallLater(TimerTick, TIMER_TICK_MS, true);
		return true;
	}

	protected void TimerTick()
	{
		if (!Replication.IsServer() || m_iState != SPT_EWarfareMissionInteractionState.RUNNING)
		{
			GetGame().GetCallqueue().Remove(TimerTick);
			return;
		}

		if (m_iMissionType == SPT_EWarfareMissionType.DEFEND_TRANSMITTER && !IsAnyPlayerDefending())
			return;

		m_iRemainingSeconds = Math.Max(m_iRemainingSeconds - 1, 0);
		Replication.BumpMe();
		if (m_iRemainingSeconds > 0)
			return;

		GetGame().GetCallqueue().Remove(TimerTick);
		CompleteInteraction();
	}

	protected bool IsAnyPlayerDefending()
	{
		PlayerManager playerManager = GetGame().GetPlayerManager();
		IEntity owner = GetOwner();
		if (!playerManager || !owner)
			return false;

		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			IEntity player = playerManager.GetPlayerControlledEntity(playerId);
			if (player && !player.IsDeleted() &&
				vector.Distance(player.GetOrigin(), owner.GetOrigin()) <= Math.Max(m_fDefenseRadius, 1.0))
				return true;
		}

		return false;
	}

	protected void CompleteInteraction()
	{
		if (!Replication.IsServer() || m_iState == SPT_EWarfareMissionInteractionState.COMPLETED)
			return;

		m_iState = SPT_EWarfareMissionInteractionState.COMPLETED;
		m_iRemainingSeconds = 0;

		SPT_WarfareMissionObjectiveComponent objective = SPT_WarfareMissionObjectiveComponent.Cast(
			GetOwner().FindComponent(SPT_WarfareMissionObjectiveComponent));
		if (objective)
			objective.CompleteObjective();

		if (m_iMissionType == SPT_EWarfareMissionType.SABOTAGE_COMMUNICATIONS ||
			m_iMissionType == SPT_EWarfareMissionType.DESTROY_FUEL_DEPOT)
			ExecuteDestructionSequence();

		Replication.BumpMe();
		OnStateChanged();
	}

	protected void ExecuteDestructionSequence()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return;

		vector position = owner.GetOrigin();
		SpawnExplosion(m_rExplosionPrefab1, position);
		GetGame().GetCallqueue().CallLater(SpawnDelayedExplosion, 150, false, m_rExplosionPrefab2, position + Vector(1.5, 0.2, -1.0));
		GetGame().GetCallqueue().CallLater(SpawnDelayedExplosion, 350, false, m_rExplosionPrefab3, position + Vector(-1.0, 0.1, 1.5));
		PlayExplosionSound(position);
		GetGame().GetCallqueue().CallLater(ReplaceRuinTargets, RUIN_REPLACEMENT_DELAY_MS, false);
	}

	protected void SpawnDelayedExplosion(ResourceName prefab, vector position)
	{
		if (!Replication.IsServer())
			return;

		SpawnExplosion(prefab, position);
	}

	protected void SpawnExplosion(ResourceName prefab, vector position)
	{
		if (prefab.IsEmpty())
			return;

		Resource resource = Resource.Load(prefab);
		if (!resource)
		{
			Print(string.Format("[SPT_WarfareMission] Explosao invalida | prefab=%1", prefab), LogLevel.ERROR);
			return;
		}

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = position;
		GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
	}

	protected void PlayExplosionSound(vector position)
	{
		RpcDo_PlayExplosionSound(position);
		Rpc(RpcDo_PlayExplosionSound, position);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_PlayExplosionSound(vector position)
	{
		if (m_rExplosionSoundPrefab.IsEmpty())
			return;

		Resource resource = Resource.Load(m_rExplosionSoundPrefab);
		if (!resource)
			return;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = position;
		GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
	}

	protected void ReplaceRuinTargets()
	{
		if (!Replication.IsServer())
			return;

		ReplaceRuinTargetsInHierarchy(GetOwner());
		RpcDo_RemoveExplosionDecorations();
		Rpc(RpcDo_RemoveExplosionDecorations);
	}

	protected void ReplaceRuinTargetsInHierarchy(IEntity entity)
	{
		if (!entity)
			return;

		IEntity child = entity.GetChildren();
		while (child)
		{
			IEntity nextChild = child.GetSibling();
			ReplaceRuinTargetsInHierarchy(child);
			child = nextChild;
		}

		SPT_WarfareMissionRuinTargetComponent ruinTarget = SPT_WarfareMissionRuinTargetComponent.Cast(
			entity.FindComponent(SPT_WarfareMissionRuinTargetComponent));
		if (ruinTarget)
			ruinTarget.ReplaceWithRuin();
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_RemoveExplosionDecorations()
	{
		IEntity owner = GetOwner();
		BaseWorld world = GetGame().GetWorld();
		if (!owner || !world)
			return;

		m_aExplosionCleanupEntityIds.Clear();
		world.QueryEntitiesBySphere(
			owner.GetOrigin(),
			EXPLOSION_CLEANUP_RADIUS,
			CollectExplosionCleanupEntity,
			FilterExplosionCleanupEntity,
			EQueryEntitiesFlags.ALL);

		foreach (EntityID entityId : m_aExplosionCleanupEntityIds)
		{
			IEntity entity = world.FindEntityByID(entityId);
			if (entity && !entity.IsDeleted())
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
		}
	}

	protected bool FilterExplosionCleanupEntity(IEntity entity)
	{
		if (!entity || entity.IsDeleted())
			return false;

		return entity.FindComponent(SPT_WarfareMissionExplosionCleanupComponent) != null;
	}

	protected bool CollectExplosionCleanupEntity(IEntity entity)
	{
		m_aExplosionCleanupEntityIds.Insert(entity.GetID());
		return false;
	}

	protected void OnStateChanged()
	{
		SCR_HintManagerComponent hintManager = SCR_HintManagerComponent.GetInstance();
		if (!hintManager)
			return;

		if (m_iState == SPT_EWarfareMissionInteractionState.RUNNING)
			hintManager.ShowCustom(string.Format("%1 iniciada: %2s.", GetMissionLabel(), m_iRemainingSeconds));
		else if (m_iState == SPT_EWarfareMissionInteractionState.COMPLETED)
			hintManager.ShowCustom(string.Format("%1 concluida.", GetMissionLabel()));
	}

	bool CanInteract()
	{
		return m_iState == SPT_EWarfareMissionInteractionState.IDLE;
	}

	string GetActionName()
	{
		switch (m_iMissionType)
		{
			case SPT_EWarfareMissionType.SABOTAGE_COMMUNICATIONS: return "Sabotar comunicacoes";
			case SPT_EWarfareMissionType.DESTROY_FUEL_DEPOT: return "Plantar explosivos";
			case SPT_EWarfareMissionType.STEAL_INTELLIGENCE: return "Coletar inteligencia";
			case SPT_EWarfareMissionType.DEFEND_TRANSMITTER: return "Ativar transmissor";
		}
		return "Interagir";
	}

	protected string GetMissionLabel()
	{
		switch (m_iMissionType)
		{
			case SPT_EWarfareMissionType.SABOTAGE_COMMUNICATIONS: return "Sabotagem";
			case SPT_EWarfareMissionType.DESTROY_FUEL_DEPOT: return "Demolicao";
			case SPT_EWarfareMissionType.STEAL_INTELLIGENCE: return "Inteligencia";
			case SPT_EWarfareMissionType.DEFEND_TRANSMITTER: return "Defesa";
		}
		return "Missao";
	}
}
