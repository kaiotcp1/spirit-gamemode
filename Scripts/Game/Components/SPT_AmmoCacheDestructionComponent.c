//! Controlador autoritativo de uma composicao Ammo Cache.
//! Existe uma unica instancia na raiz replicada do cenario.
[ComponentEditorProps(category: "SPT/Warfare/Mission", description: "Controla o timer e a explosao de um Ammo Cache. Coloque somente na raiz do cenario.")]
class SPT_AmmoCacheDestructionComponentClass : ScriptComponentClass
{
}

enum SPT_EAmmoCacheDestructionState
{
	IDLE,
	ARMED,
	DESTROYED
}

class SPT_AmmoCacheDestructionComponent : ScriptComponent
{
	protected const float MAX_INTERACTION_DISTANCE = 4.0;
	protected const int TIMER_TICK_MS = 1000;
	protected const int SCENARIO_DELETE_DELAY_MS = 2000;
	protected const ResourceName DEFAULT_EXPLOSION_PREFAB =
		"{7E091BB73AC18ED0}Prefabs/Weapons/Warheads/Explosions/Explosion_AmmoRack_Medium.et";
	protected const ResourceName DEFAULT_EXPLOSION_SOUND_PREFAB =
		"{490CCFCB2ACC99CF}Prefabs/Weapons/Warheads/Explosions/Explosion_AmmoRack_Medium_Sound.et";
	protected const float SCENARIO_ENTITY_SEARCH_RADIUS = 12.0;

	[Attribute("10", desc: "Tempo entre plantar os explosivos e detonar, em segundos.")]
	protected int m_iDestructionDelaySeconds;

	[Attribute("{7E091BB73AC18ED0}Prefabs/Weapons/Warheads/Explosions/Explosion_AmmoRack_Medium.et",
		UIWidgets.ResourceNamePicker, desc: "Explosao criada pelo servidor ao terminar o timer.", params: "et")]
	protected ResourceName m_rExplosionPrefab;

	[Attribute("{490CCFCB2ACC99CF}Prefabs/Weapons/Warheads/Explosions/Explosion_AmmoRack_Medium_Sound.et",
		UIWidgets.ResourceNamePicker, desc: "Som criado localmente em cada cliente ao detonar.", params: "et")]
	protected ResourceName m_rExplosionSoundPrefab;

	[RplProp(onRplName: "OnDestructionStateChanged")]
	protected int m_iDestructionState;

	[RplProp()]
	protected int m_iRemainingSeconds;

	protected string m_sPointId;
	protected ref array<EntityID> m_aScenarioChildIds;

	void SPT_AmmoCacheDestructionComponent(IEntityComponentSource src, IEntity ent, IEntity parent)
	{
		m_iDestructionState = SPT_EAmmoCacheDestructionState.IDLE;
		m_iRemainingSeconds = 0;
		m_aScenarioChildIds = new array<EntityID>();
	}

	void ~SPT_AmmoCacheDestructionComponent()
	{
		GetGame().GetCallqueue().Remove(TimerTick);
		GetGame().GetCallqueue().Remove(DeleteScenario);
	}

	void Initialize(string pointId)
	{
		if (!Replication.IsServer())
			return;

		m_sPointId = pointId;
		CollectScenarioChildIds();
		Print(string.Format("[SPT_AmmoCache] Cenario inicializado | pointId=%1 | filhos=%2",
			m_sPointId, m_aScenarioChildIds.Count()));
	}

	//! Entrada exclusivamente autoritativa, chamada pelo componente do PlayerController.
	bool TryStartDestruction(IEntity player)
	{
		if (!Replication.IsServer() || !player || player.IsDeleted())
			return false;
		if (m_iDestructionState != SPT_EAmmoCacheDestructionState.IDLE)
			return false;

		IEntity owner = GetOwner();
		if (!owner || owner.IsDeleted())
			return false;

		if (vector.Distance(player.GetOrigin(), owner.GetOrigin()) > MAX_INTERACTION_DISTANCE)
		{
			Print(string.Format("[SPT_AmmoCache] Pedido rejeitado por distancia | pointId=%1", m_sPointId), LogLevel.WARNING);
			return false;
		}

		DamageManagerComponent playerDamage = DamageManagerComponent.Cast(
			player.FindComponent(DamageManagerComponent));
		if (playerDamage && playerDamage.GetHealthScaled() <= 0.0)
			return false;

		SPT_WarfareMissionManagerComponent missionManager = SPT_WarfareMissionManagerComponent.GetInstance();
		if (!missionManager || !missionManager.CanStartAmmoCacheDestruction(m_sPointId, owner))
		{
			Print(string.Format("[SPT_AmmoCache] Pedido rejeitado por estado da missao | pointId=%1", m_sPointId), LogLevel.WARNING);
			return false;
		}

		m_iDestructionDelaySeconds = Math.Max(m_iDestructionDelaySeconds, 1);
		m_iRemainingSeconds = m_iDestructionDelaySeconds;
		m_iDestructionState = SPT_EAmmoCacheDestructionState.ARMED;
		Replication.BumpMe();
		OnDestructionStateChanged();

		GetGame().GetCallqueue().CallLater(TimerTick, TIMER_TICK_MS, true);
		Print(string.Format("[SPT_AmmoCache] Destruicao iniciada | pointId=%1 | timer=%2s",
			m_sPointId, m_iRemainingSeconds));
		return true;
	}

	protected void TimerTick()
	{
		if (!Replication.IsServer() || m_iDestructionState != SPT_EAmmoCacheDestructionState.ARMED)
		{
			GetGame().GetCallqueue().Remove(TimerTick);
			return;
		}

		m_iRemainingSeconds = Math.Max(m_iRemainingSeconds - 1, 0);
		Replication.BumpMe();
		if (m_iRemainingSeconds > 0)
			return;

		GetGame().GetCallqueue().Remove(TimerTick);
		ExecuteDestruction();
	}

	protected void ExecuteDestruction()
	{
		if (!Replication.IsServer() || m_iDestructionState != SPT_EAmmoCacheDestructionState.ARMED)
			return;

		IEntity owner = GetOwner();
		if (!owner || owner.IsDeleted())
			return;

		m_iDestructionState = SPT_EAmmoCacheDestructionState.DESTROYED;
		m_iRemainingSeconds = 0;

		SPT_WarfareMissionObjectiveComponent objective = SPT_WarfareMissionObjectiveComponent.Cast(
			owner.FindComponent(SPT_WarfareMissionObjectiveComponent));
		if (objective)
			objective.TriggerDestruction();

		SpawnExplosion(owner.GetOrigin());
		PlayExplosionSound(owner.GetOrigin());
		RpcDo_RemoveScenarioDecorations();
		Rpc(RpcDo_RemoveScenarioDecorations);
		Replication.BumpMe();
		OnDestructionStateChanged();

		Print(string.Format("[SPT_AmmoCache] Cache detonado | pointId=%1 | posicao=%2",
			m_sPointId, owner.GetOrigin()));

		// Mantem a entidade replicada viva tempo suficiente para entregar o estado terminal.
		GetGame().GetCallqueue().CallLater(DeleteScenario, SCENARIO_DELETE_DELAY_MS, false);
	}

	protected void SpawnExplosion(vector position)
	{
		ResourceName explosionPrefab = m_rExplosionPrefab;
		if (explosionPrefab.IsEmpty())
			explosionPrefab = DEFAULT_EXPLOSION_PREFAB;

		Resource resource = Resource.Load(explosionPrefab);
		if (!resource)
		{
			Print(string.Format("[SPT_AmmoCache] Prefab de explosao invalido | prefab=%1", explosionPrefab), LogLevel.ERROR);
			return;
		}

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		position[1] = position[1] + 0.5;
		params.Transform[3] = position;

		IEntity explosion = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
		if (!explosion)
			Print(string.Format("[SPT_AmmoCache] Falha ao criar explosao | prefab=%1", explosionPrefab), LogLevel.ERROR);
	}

	protected void PlayExplosionSound(vector position)
	{
		// Execucao direta cobre o listen-server; o RPC cobre clientes remotos.
		RpcDo_PlayExplosionSound(position);
		Rpc(RpcDo_PlayExplosionSound, position);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_PlayExplosionSound(vector position)
	{
		ResourceName soundPrefab = m_rExplosionSoundPrefab;
		if (soundPrefab.IsEmpty())
			soundPrefab = DEFAULT_EXPLOSION_SOUND_PREFAB;

		Resource resource = Resource.Load(soundPrefab);
		if (!resource)
		{
			Print(string.Format("[SPT_AmmoCache] Prefab de som invalido | prefab=%1", soundPrefab), LogLevel.ERROR);
			return;
		}

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = position;
		IEntity sound = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
		if (!sound)
			Print(string.Format("[SPT_AmmoCache] Falha ao criar som da explosao | prefab=%1", soundPrefab), LogLevel.ERROR);
	}

	protected void DeleteScenario()
	{
		if (!Replication.IsServer())
			return;

		// Fallback autoritativo caso alguma entidade nao estivesse disponivel
		// no frame em que a explosao foi executada.
		RemoveScenarioDecorations();

		Print(string.Format("[SPT_AmmoCache] Raiz do cenario removida | pointId=%1", m_sPointId));

		IEntity owner = GetOwner();
		if (owner && !owner.IsDeleted())
			SCR_EntityHelper.DeleteEntityAndChildren(owner);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_RemoveScenarioDecorations()
	{
		CollectScenarioChildIds();
		RemoveScenarioDecorations();
	}

	protected void RemoveScenarioDecorations()
	{
		BaseWorld world = GetGame().GetWorld();
		int deletedChildren = 0;
		if (world)
		{
			foreach (EntityID childId : m_aScenarioChildIds)
			{
				IEntity child = world.FindEntityByID(childId);
				if (!child || child.IsDeleted())
					continue;

				SCR_EntityHelper.DeleteEntityAndChildren(child);
				deletedChildren++;
			}
		}

		Print(string.Format("[SPT_AmmoCache] Decoracao removida | pointId=%1 | entidadesRemovidas=%2/%3",
			m_sPointId, deletedChildren, m_aScenarioChildIds.Count()));
	}

	protected void CollectScenarioChildIds()
	{
		m_aScenarioChildIds.Clear();

		IEntity owner = GetOwner();
		if (!owner)
			return;

		IEntity child = owner.GetChildren();
		while (child)
		{
			m_aScenarioChildIds.Insert(child.GetID());
			child = child.GetSibling();
		}

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		// Alguns prefabs vanilla com persistencia/replicacao propria deixam de
		// pertencer a hierarquia da composicao em runtime. O placement marker
		// identifica somente as entidades explicitamente declaradas no cenario.
		world.QueryEntitiesBySphere(
			owner.GetOrigin(),
			SCENARIO_ENTITY_SEARCH_RADIUS,
			CollectMarkedScenarioEntity,
			FilterMarkedScenarioEntity,
			EQueryEntitiesFlags.ALL);
	}

	protected bool FilterMarkedScenarioEntity(IEntity entity)
	{
		if (!entity || entity.IsDeleted() || entity == GetOwner())
			return false;

		return entity.FindComponent(SPT_WarfareMissionPlacementComponent) != null;
	}

	protected bool CollectMarkedScenarioEntity(IEntity entity)
	{
		EntityID entityId = entity.GetID();
		if (m_aScenarioChildIds.Find(entityId) < 0)
			m_aScenarioChildIds.Insert(entityId);

		return true;
	}

	protected void OnDestructionStateChanged()
	{
		SCR_HintManagerComponent hintManager = SCR_HintManagerComponent.GetInstance();
		if (!hintManager)
			return;

		if (m_iDestructionState == SPT_EAmmoCacheDestructionState.ARMED)
			hintManager.ShowCustom(string.Format("Ammo Cache: explosivos plantados! %1s...", m_iRemainingSeconds));
		else if (m_iDestructionState == SPT_EAmmoCacheDestructionState.DESTROYED)
			hintManager.ShowCustom("Ammo Cache destruida!");
	}

	bool IsDestroyed()
	{
		return m_iDestructionState == SPT_EAmmoCacheDestructionState.DESTROYED;
	}

	bool IsDestructionInProgress()
	{
		return m_iDestructionState == SPT_EAmmoCacheDestructionState.ARMED;
	}

	int GetRemainingSeconds()
	{
		return m_iRemainingSeconds;
	}

	string GetPointId()
	{
		return m_sPointId;
	}
}
