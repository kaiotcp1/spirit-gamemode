[ComponentEditorProps(category: "SPT/GameMode", description: "Finds mines and creates defusal interaction proxies for them")]
class SPT_MineDefusalManagerComponentClass : ScriptComponentClass
{
}

class SPT_MineDefusalManagerComponent : ScriptComponent
{
	[Attribute("1", desc: "Automatically scan and configure all mines in the world")]
	bool m_bAutoScanMines;

	[Attribute("0", desc: "Force debug mode on all configured mines")]
	bool m_bDebugAllMines;

	[Attribute("5", desc: "Default wire count for configured mines")]
	int m_iDefaultWireCount;

	[Attribute("45.0", desc: "Default timer in seconds for configured mines")]
	float m_fDefaultTimer;

	[Attribute("1", desc: "Default tool requirement for configured mines")]
	bool m_bDefaultRequiresTool;

	[Attribute("10.0", desc: "Seconds between scans. Also finds mines spawned after game start. Set to 0 for one scan only.")]
	float m_fRescanInterval;

	[Attribute("{B72C51EA34D8F921}Prefabs/MineDefusal/SPT_MineDefusalProxy.et", UIWidgets.ResourceNamePicker, desc: "Interaction proxy spawned on vanilla mines", params: "et")]
	ResourceName m_sDefusalProxyPrefab;

	protected ref array<EntityID> m_aProcessedMines;

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (SCR_Global.IsEditMode())
			return;

		m_aProcessedMines = new array<EntityID>();

		if (!m_bAutoScanMines)
			return;

		// Proxies must only be spawned by the authority. Their RplComponent
		// creates them on all connected clients.
		if (!Replication.IsServer())
			return;

		GetGame().GetCallqueue().CallLater(ScanAndAttach, 1000, false);

		if (m_fRescanInterval > 0)
			GetGame().GetCallqueue().CallLater(ScanAndAttach, Math.Round(m_fRescanInterval * 1000), true);
	}

	void ScanAndAttach()
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		int previousCount = m_aProcessedMines.Count();

		world.QueryEntitiesBySphere(
			vector.Zero,
			99999999,
			AttachDefusalInteraction,
			FilterMineEntities,
			EQueryEntitiesFlags.ALL
		);

		int addedCount = m_aProcessedMines.Count() - previousCount;
		Print(string.Format("[SPT_MineDefusalManager] Scan complete: %1 new, %2 total mines configured",
			addedCount, m_aProcessedMines.Count()));
	}

	protected bool FilterMineEntities(IEntity entity)
	{
		if (!entity || entity.IsDeleted())
			return false;

		if (IsProcessed(entity.GetID()))
			return false;

		// All vanilla pressure mines use this component.
		SCR_PressureTriggerComponent pressureTrigger = SCR_PressureTriggerComponent.Cast(
			entity.FindComponent(SCR_PressureTriggerComponent)
		);
		if (pressureTrigger)
			return true;

		// This also covers compatible modded mines which expose the vanilla
		// disarm action but use a custom trigger implementation.
		ActionsManagerComponent actionsManager = ActionsManagerComponent.Cast(
			entity.FindComponent(ActionsManagerComponent)
		);
		if (!actionsManager)
			return false;

		array<BaseUserAction> actions = {};
		actionsManager.GetActionsList(actions);

		foreach (BaseUserAction action : actions)
		{
			if (SCR_DisarmMineAction.Cast(action))
				return true;
		}

		return false;
	}

	protected bool AttachDefusalInteraction(IEntity mine)
	{
		SPT_MineDefusalComponent existingComponent = SPT_MineDefusalComponent.Cast(
			mine.FindComponent(SPT_MineDefusalComponent)
		);

		if (existingComponent && HasSPTDefuseAction(mine))
		{
			ConfigureComponent(existingComponent);
			m_aProcessedMines.Insert(mine.GetID());
			LogConfiguredMine(mine, false);
			return false;
		}

		Resource proxyResource = Resource.Load(m_sDefusalProxyPrefab);
		if (!proxyResource)
		{
			Print("[SPT_MineDefusalManager] Could not load defusal proxy prefab: " + m_sDefusalProxyPrefab, LogLevel.ERROR);
			return false;
		}

		EntitySpawnParams spawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		vector mineTransform[4];
		mine.GetWorldTransform(mineTransform);
		spawnParams.Transform = mineTransform;

		IEntity proxy = GetGame().SpawnEntityPrefab(proxyResource, GetGame().GetWorld(), spawnParams);
		if (!proxy)
		{
			Print(string.Format("[SPT_MineDefusalManager] Failed to create interaction proxy for %1 at %2",
				mine.ClassName(), mine.GetOrigin()), LogLevel.ERROR);
			return false;
		}

		SPT_MineDefusalComponent proxyComponent = SPT_MineDefusalComponent.Cast(
			proxy.FindComponent(SPT_MineDefusalComponent)
		);
		if (!proxyComponent)
		{
			Print("[SPT_MineDefusalManager] Proxy prefab has no SPT_MineDefusalComponent", LogLevel.ERROR);
			SCR_EntityHelper.DeleteEntityAndChildren(proxy);
			return false;
		}

		proxyComponent.SetTargetMine(mine);
		ConfigureComponent(proxyComponent);
		m_aProcessedMines.Insert(mine.GetID());
		LogConfiguredMine(mine, true);

		// false means "continue the world query". Returning true here was the
		// reason the old implementation stopped after its first result.
		return false;
	}

	protected bool IsProcessed(EntityID entityId)
	{
		foreach (EntityID processedId : m_aProcessedMines)
		{
			if (processedId == entityId)
				return true;
		}

		return false;
	}

	protected bool HasSPTDefuseAction(IEntity entity)
	{
		ActionsManagerComponent actionsManager = ActionsManagerComponent.Cast(
			entity.FindComponent(ActionsManagerComponent)
		);
		if (!actionsManager)
			return false;

		array<BaseUserAction> actions = {};
		actionsManager.GetActionsList(actions);

		foreach (BaseUserAction action : actions)
		{
			if (SPT_DefuseMineAction.Cast(action))
				return true;
		}

		return false;
	}

	protected void ConfigureComponent(SPT_MineDefusalComponent component)
	{
		component.m_iWireCount = m_iDefaultWireCount;
		component.m_fTimerSeconds = m_fDefaultTimer;
		component.m_bRequiresTool = m_bDefaultRequiresTool;

		if (m_bDebugAllMines)
			component.m_bDebugMode = true;
	}

	protected void LogConfiguredMine(IEntity mine, bool usesProxy)
	{
		Print(string.Format("[SPT_MineDefusalManager] Configured %1 at %2 | proxy: %3 | wires: %4 | timer: %5",
			mine.ClassName(), mine.GetOrigin(), usesProxy, m_iDefaultWireCount, m_fDefaultTimer));
	}
}
