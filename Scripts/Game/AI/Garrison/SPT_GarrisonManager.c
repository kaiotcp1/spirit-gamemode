// version date 06-14-26
// SPT AI Garrison






// one garrisoned soldier and the way we want him kept, pinned, facing his post
class SPT_GarrisonHold
{
	IEntity m_Entity;
	SCR_AIGroup m_Group;
	SCR_AIUtilityComponent m_Utility;
	vector m_Facing;

	void SPT_GarrisonHold(IEntity entity, SCR_AIGroup group, SCR_AIUtilityComponent utility, vector facing)
	{
		m_Entity = entity;
		m_Group = group;
		m_Utility = utility;
		m_Facing = facing;
	}
}

class SPT_GarrisonManager
{
	protected const int   RETRY_DELAY_MS   = 750;
	protected const int   MAX_RETRIES      = 3;
	protected const int   HOLD_POLL_MS     = 400;
	protected const int   NAV_POLL_MS      = 200;
	protected const int   NAV_MAX_POLLS    = 10;
	protected const float NAV_TILE_STEP_M  = 8.0;
	protected const float FACE_PROJECT_M   = 5.0;
	protected const float FACE_DURATION    = 2.0;

	protected static ref SPT_GarrisonManager s_Instance;

	protected ref array<ref SPT_GarrisonHold> m_aHolds;
	protected bool m_bHoldPollRunning;

	void SPT_GarrisonManager()
	{
		m_aHolds = {};
	}

	static SPT_GarrisonManager Get()
	{
		if (!s_Instance)
			s_Instance = new SPT_GarrisonManager();
		return s_Instance;
	}

	// drop a group into the nearest building around center
	void Garrison(SCR_AIGroup group, vector center, float radius)
	{
		if (!group)
			return;
		EnsureNavmeshReady(group, center, radius, NAV_MAX_POLLS);
	}

	// building may have just spawned, kick a navmesh rebuild and wait for the tiles b4 sampling
	protected void EnsureNavmeshReady(SCR_AIGroup group, vector center, float radius, int pollsLeft)
	{
		if (!group)
			return;

		array<AIAgent> agents = {};
		group.GetAgents(agents);
		if (agents.IsEmpty())
			return;

		AIPathfindingComponent pathing = FindGroupPathing(agents);
		vector mins, maxs;
		if (!pathing || !SPT_GarrisonDetector.ResolveBuildingBounds(group.GetWorld(), center, mins, maxs))
		{
			TryGarrison(group, center, radius, MAX_RETRIES);
			return;
		}

		NavmeshWorldComponent navmesh = pathing.GetNavmeshComponent();
		if (!navmesh)
		{
			TryGarrison(group, center, radius, MAX_RETRIES);
			return;
		}

		if (pollsLeft >= NAV_MAX_POLLS)
		{
			AIWorld aiWorld = GetGame().GetAIWorld();
			if (aiWorld)
				aiWorld.RequestNavmeshRebuild(mins, maxs);
		}

		if (TilesReady(navmesh, mins, maxs) || pollsLeft <= 0)
		{
			TryGarrison(group, center, radius, MAX_RETRIES);
			return;
		}

		GetGame().GetCallqueue().CallLater(EnsureNavmeshReady, NAV_POLL_MS, false, group, center, radius, pollsLeft - 1);
	}

	// poke every tile under the building's footprint; returns true once they're all in
	protected bool TilesReady(NavmeshWorldComponent navmesh, vector mins, vector maxs)
	{
		bool allLoaded = true;
		int xSteps = Math.Floor((maxs[0] - mins[0]) / NAV_TILE_STEP_M) + 1;
		int zSteps = Math.Floor((maxs[2] - mins[2]) / NAV_TILE_STEP_M) + 1;
		float midY = (mins[1] + maxs[1]) * 0.5;

		for (int xi = 0; xi <= xSteps; xi++)
		{
			for (int zi = 0; zi <= zSteps; zi++)
			{
				float x = mins[0] + xi * NAV_TILE_STEP_M;
				float z = mins[2] + zi * NAV_TILE_STEP_M;
				for (int yl = 0; yl < 3; yl++)
				{
					float y = mins[1];
					if (yl == 1)
						y = midY;
					else if (yl == 2)
						y = maxs[1];

					vector wp = Vector(x, y, z);
					if (!navmesh.IsTileLoaded(wp))
					{
						navmesh.LoadTileIn(wp);
						allLoaded = false;
					}
				}
			}
		}
		return allLoaded;
	}

	// find posts, teleport each member onto one and pin him; retries if the navmesh isn't ready
	protected void TryGarrison(SCR_AIGroup group, vector center, float radius, int retriesLeft)
	{
		if (!group)
			return;

		array<AIAgent> agents = {};
		group.GetAgents(agents);
		if (agents.IsEmpty())
			return;

		AIPathfindingComponent pathing = FindGroupPathing(agents);
		if (!pathing)
			return;

		// don't hand out spotsalready filled from an earlier garrison
		array<vector> excluded = {};
		GatherOccupied(excluded);

		array<ref SPT_GarrisonPost> posts = {};
		SPT_EGarrisonDetectResult result = SPT_GarrisonDetector.DetectPosts(
			group.GetWorld(), center, pathing, agents.Count(), excluded, posts);

		if ((result == SPT_EGarrisonDetectResult.NAVMESH_NOT_READY || result == SPT_EGarrisonDetectResult.NO_POSITIONS) && retriesLeft > 0)
		{
			GetGame().GetCallqueue().CallLater(RetryGarrison, RETRY_DELAY_MS, false, group, center, radius, retriesLeft - 1);
			return;
		}

		if (result != SPT_EGarrisonDetectResult.OK || posts.IsEmpty())
			return;

		SetGroupEngaging(group);

		int assigned = 0;
		foreach (AIAgent agent : agents)
		{
			if (assigned >= posts.Count())
				break;
			if (TeleportMemberToPost(agent, group, posts[assigned]))
				assigned++;
		}
	}

	protected void RetryGarrison(SCR_AIGroup group, vector center, float radius, int retriesLeft)
	{
		TryGarrison(group, center, radius, retriesLeft);
	}

	// snap one soldier onto his post, aim him, pin him, and remember him for the hold poll
	protected bool TeleportMemberToPost(AIAgent agent, SCR_AIGroup group, SPT_GarrisonPost post)
	{
		if (!agent || !post)
			return false;

		IEntity entity = agent.GetControlledEntity();
		if (!entity)
			return false;

		BaseGameEntity gEntity = BaseGameEntity.Cast(entity);
		if (!gEntity)
			return false;

		SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));

		vector mat[4];
		if (post.m_Facing.Length() > 0.001)
			Math3D.AnglesToMatrix(post.m_Facing.VectorToAngles(), mat);
		else
			entity.GetWorldTransform(mat);
		mat[3] = post.m_Pos;
		gEntity.Teleport(mat);

		SPT_AIMovementLockHelper.ApplyLockState(entity, true);
		m_aHolds.Insert(new SPT_GarrisonHold(entity, group, utility, post.m_Facing));
		EnsureHoldPoll();
		return true;
	}

	protected void EnsureHoldPoll()
	{
		if (m_bHoldPollRunning)
			return;
		GetGame().GetCallqueue().CallLater(HoldPoll, HOLD_POLL_MS, true);
		m_bHoldPollRunning = true;
	}

	// runs while anyone's garrisoned: pop the prone back upright and keep them looking the right way
	protected void HoldPoll()
	{
		for (int i = m_aHolds.Count() - 1; i >= 0; i--)
		{
			SPT_GarrisonHold h = m_aHolds[i];
			if (!h.m_Entity)
			{
				m_aHolds.Remove(i);
				continue;
			}

			CharacterControllerComponent cc = CharacterControllerComponent.Cast(h.m_Entity.FindComponent(CharacterControllerComponent));
			if (cc && cc.GetStance() == ECharacterStance.PRONE)
				cc.SetStanceChange(ECharacterStanceChange.STANCECHANGE_TOERECTED);

			FaceDirection(h);
		}

		if (m_aHolds.IsEmpty())
		{
			GetGame().GetCallqueue().Remove(HoldPoll);
			m_bHoldPollRunning = false;
		}
	}

	// nudge his angle toward where his post should be watching
	protected void FaceDirection(SPT_GarrisonHold h)
	{
		if (!h.m_Utility || !h.m_Utility.m_LookAction)
			return;
		if (h.m_Facing.Length() < 0.001)
			return;

		vector target = h.m_Entity.GetOrigin() + h.m_Facing * FACE_PROJECT_M + "0 1.5 0";
		h.m_Utility.m_LookAction.LookAt(target, SCR_AILookAction.PRIO_DANGER_EVENT, FACE_DURATION);
	}

	// true if anyone in this group is garrisoned; the smoke gate asks so dug-in AI don't smoke themselves
	bool IsGroupGarrisoned(SCR_AIGroup group)
	{
		if (!group)
			return false;
		foreach (SPT_GarrisonHold h : m_aHolds)
		{
			if (h.m_Group == group)
				return true;
		}
		return false;
	}

	protected void GatherOccupied(out array<vector> excluded)
	{
		foreach (SPT_GarrisonHold h : m_aHolds)
		{
			if (h.m_Entity)
				excluded.Insert(h.m_Entity.GetOrigin());
		}
	}

	// let them shoot on sight once they're in place
	protected void SetGroupEngaging(SCR_AIGroup group)
	{
		SCR_AIGroupUtilityComponent gu = SCR_AIGroupUtilityComponent.Cast(group.FindComponent(SCR_AIGroupUtilityComponent));
		if (gu)
			gu.SetCombatMode(EAIGroupCombatMode.FIRE_AT_WILL);
	}

	// any member's pathfinding will do, they all share the same navmesh
	protected AIPathfindingComponent FindGroupPathing(notnull array<AIAgent> agents)
	{
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;
			AIPathfindingComponent p = AIPathfindingComponent.Cast(agent.FindComponent(AIPathfindingComponent));
			if (p)
				return p;
		}
		return null;
	}

	// let a group go, unpin everyone held for it
	void Ungarrison(SCR_AIGroup group)
	{
		if (!group)
			return;

		for (int i = m_aHolds.Count() - 1; i >= 0; i--)
		{
			SPT_GarrisonHold h = m_aHolds[i];
			if (h.m_Group != group)
				continue;
			if (h.m_Entity)
				SPT_AIMovementLockHelper.ApplyLockState(h.m_Entity, false);
			m_aHolds.Remove(i);
		}
	}
}
