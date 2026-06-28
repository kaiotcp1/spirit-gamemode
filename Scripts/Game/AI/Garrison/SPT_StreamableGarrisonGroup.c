//! Holds the cached state of a garrison group after it has been streamed out.
//! When the player returns, only the survivors are respawned instead of a fresh group,
//! making the game finite and rewarding the player for clearing locations.
class SPT_StreamableGarrisonGroup : Managed
{
	//! Original group prefab used to spawn this group.
	ResourceName m_rGroupPrefab;

	//! Prefab names of the members that were still alive at stream-out time.
	//! If empty, the group was wiped out and will not be respawned.
	ref array<ResourceName> m_aAliveMembers;

	//! Center of the surviving members at stream-out time.
	vector m_vPosition;

	//! If true the group uses CQB interior posts; if false it patrols outdoors.
	bool m_bIsCQB;

	//-----------------------------------------------------------------------
	// LIFECYCLE
	//-----------------------------------------------------------------------

	void SPT_StreamableGarrisonGroup()
	{
		m_aAliveMembers = new array<ResourceName>();
	}

	//-----------------------------------------------------------------------
	// PUBLIC METHODS
	//-----------------------------------------------------------------------

	//! Number of members that will be respawned on the next stream-in.
	//! Returns 0 for fully eliminated groups.
	int GetCachedManpower()
	{
		if (m_aAliveMembers)
			return m_aAliveMembers.Count();

		return 0;
	}

	//! Populate this cache entry from a live SCR_AIGroup before it is deleted.
	//! Returns the number of alive members saved.
	int CaptureFromGroup(notnull SCR_AIGroup group, ResourceName groupPrefab, vector fallbackPosition, bool isCQB)
	{
		m_rGroupPrefab = groupPrefab;
		m_vPosition = fallbackPosition;
		m_bIsCQB = isCQB;

		array<AIAgent> agents = {};
		group.GetAgents(agents);

		m_aAliveMembers.Clear();
		vector survivingCenter = vector.Zero;
		int positionedSurvivors;
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			IEntity entity = agent.GetControlledEntity();
			if (!entity)
				continue;

			SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(
				entity.FindComponent(SCR_CharacterControllerComponent));
			if (!controller)
				continue;

			if (controller.GetLifeState() >= ECharacterLifeState.DEAD)
				continue;

			EntityPrefabData prefabData = entity.GetPrefabData();
			if (!prefabData)
			{
				Print(string.Format("[SPT_WorldGarrison] Membro vivo ignorado no cache porque nao possui prefab | entidade=%1",
					entity), LogLevel.WARNING);
				continue;
			}

			ResourceName memberPrefab = prefabData.GetPrefabName();
			if (memberPrefab.IsEmpty())
			{
				Print(string.Format("[SPT_WorldGarrison] Membro vivo ignorado no cache porque o prefab esta vazio | entidade=%1",
					entity), LogLevel.WARNING);
				continue;
			}

			m_aAliveMembers.Insert(memberPrefab);
			survivingCenter = survivingCenter + entity.GetOrigin();
			positionedSurvivors++;
		}

		if (positionedSurvivors > 0)
			m_vPosition = survivingCenter * (1.0 / positionedSurvivors);

		return m_aAliveMembers.Count();
	}
}
