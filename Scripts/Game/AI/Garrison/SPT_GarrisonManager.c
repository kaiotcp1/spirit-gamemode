// data da versao 06-14-26
// SPT AI Garrison – gerenciador central de guarnicao de IA

// Representa um soldado guarnecido e o estado em que deve ser mantido:
// entidade travada, grupo ao qual pertence, componente de utilidade e direcao de vigilancia
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

// Rede de patrulha de um grupo entre os pontos internos das construcoes da area.
class SPT_GarrisonInteriorPatrol
{
	SCR_AIGroup m_Group;
	vector m_BuildingCenter;
	ref array<ref SPT_GarrisonPost> m_Posts = {};
	AIWaypoint m_Waypoint;
	int m_iLastPost = -1;
	int m_iWaitPolls;
	SCR_EAIGroupFormation m_eFormation;
	EMovementType m_eMovementType;

	void SPT_GarrisonInteriorPatrol(
		SCR_AIGroup group,
		vector buildingCenter,
		SCR_EAIGroupFormation formation,
		EMovementType movementType)
	{
		m_Group = group;
		m_BuildingCenter = buildingCenter;
		m_eFormation = formation;
		m_eMovementType = movementType;
	}
}

// Gerenciador singleton de guarnicao: detecta postos, teleporta soldados, mantem vigilancia
class SPT_GarrisonManager
{
	protected const int   RETRY_DELAY_MS   = 750;   // intervalo entre tentativas de re-guarnicao (ms)
	protected const int   MAX_RETRIES      = 3;     // numero maximo de tentativas antes de desistir
	protected const int   HOLD_POLL_MS     = 400;   // intervalo do polling de manutencao de postura
	protected const int   NAV_POLL_MS      = 200;   // intervalo de espera entre checagens de tiles do navmesh
	protected const int   NAV_MAX_POLLS    = 10;    // maximo de checagens de navmesh antes de seguir em frente
	protected const float NAV_TILE_STEP_M  = 8.0;   // passo de amostragem de tiles sob a construcao
	protected const float FACE_PROJECT_M   = 5.0;   // distancia de projecao do alvo de olhar
	protected const float FACE_DURATION    = 2.0;   // duracao da acao de olhar (s)
	protected const int   PATROL_WAIT_MIN_POLLS = 0;
	protected const int   PATROL_WAIT_MAX_POLLS = 7; // no maximo 2,8 segundos entre deslocamentos
	protected const int   PATROL_RING_COUNT = 4;
	protected const int   PATROL_SAMPLES_PER_RING = 12;
	protected const int   PATROL_NAV_POLL_MS = 500;
	protected const int   PATROL_NAV_MAX_POLLS = 20;
	protected const ResourceName DEFAULT_PATROL_WAYPOINT_PREFAB = "{FFF9518F73279473}PrefabsEditable/Auto/AI/Waypoints/E_AIWaypoint_Move.et";

	protected static ref SPT_GarrisonManager s_Instance;

	protected ref array<ref SPT_GarrisonHold> m_aHolds;
	protected ref array<ref SPT_GarrisonInteriorPatrol> m_aInteriorPatrols;
	protected ref Resource m_PatrolWaypointResource;
	protected ResourceName m_sPatrolWaypointPrefab;
	protected bool m_bPatrolWaypointLoadAttempted;
	protected bool m_bHoldPollRunning;

	void SPT_GarrisonManager()
	{
		m_aHolds = {};
		m_aInteriorPatrols = {};
		m_sPatrolWaypointPrefab = DEFAULT_PATROL_WAYPOINT_PREFAB;
	}

	// Acesso singleton – garante que so exista uma instancia do gerenciador
	static SPT_GarrisonManager Get()
	{
		if (!s_Instance)
			s_Instance = new SPT_GarrisonManager();
		return s_Instance;
	}

	// Entrada principal: guarnece um grupo na construcao mais proxima ao ponto central
	void Garrison(SCR_AIGroup group, vector center, float radius)
	{
		if (!group)
			return;

		array<AIAgent> agents = {};
		group.GetAgents(agents);
		SetMaximumAgentSkill(agents);
		EnsureNavmeshReady(group, center, radius, NAV_MAX_POLLS);
	}

	void Patrol(
		SCR_AIGroup group,
		vector areaCenter,
		float areaRadius,
		ResourceName patrolWaypointPrefab,
		SCR_EAIGroupFormation patrolFormation,
		EMovementType patrolMovementType)
	{
		if (!group)
			return;

		if (!patrolWaypointPrefab.IsEmpty() && patrolWaypointPrefab != m_sPatrolWaypointPrefab)
		{
			m_sPatrolWaypointPrefab = patrolWaypointPrefab;
			m_PatrolWaypointResource = null;
			m_bPatrolWaypointLoadAttempted = false;
		}
		if (m_sPatrolWaypointPrefab.Contains("AIWaypoint_Patrol"))
		{
			Print(string.Format("[SPT_Garrison] Waypoint Patrol nao e compativel com destinos individuais; usando Move: %1",
				DEFAULT_PATROL_WAYPOINT_PREFAB), LogLevel.WARNING);
			m_sPatrolWaypointPrefab = DEFAULT_PATROL_WAYPOINT_PREFAB;
			m_PatrolWaypointResource = null;
			m_bPatrolWaypointLoadAttempted = false;
		}

		array<AIAgent> agents = {};
		group.GetAgents(agents);
		if (agents.IsEmpty())
			return;

		SetMaximumAgentSkill(agents);
		AIPathfindingComponent pathing = FindGroupPathing(agents);
		if (!pathing)
		{
			Print(string.Format("[SPT_Garrison] Patrulha nao iniciada: grupo sem pathfinding | grupo=%1", group), LogLevel.ERROR);
			return;
		}

		StartAreaPatrol(
			group,
			areaCenter,
			areaRadius,
			pathing,
			patrolFormation,
			patrolMovementType,
			PATROL_NAV_MAX_POLLS);
	}

	protected void ApplyPatrolMovementSettings(
		notnull SCR_AIGroup group,
		SCR_EAIGroupFormation formation,
		EMovementType movementType)
	{
		AIFormationComponent formationComponent = group.GetFormationComponent();
		if (formationComponent)
			formationComponent.SetFormation(SCR_Enum.GetEnumName(SCR_EAIGroupFormation, formation));

		SCR_AIGroupUtilityComponent utility = SCR_AIGroupUtilityComponent.Cast(
			group.FindComponent(SCR_AIGroupUtilityComponent));
		AIGroupMovementComponent movementComponent;
		if (utility)
			movementComponent = utility.m_GroupMovementComponent;
		if (!movementComponent)
			movementComponent = AIGroupMovementComponent.Cast(group.FindComponent(AIGroupMovementComponent));
		if (movementComponent)
			movementComponent.SetGroupCharactersWantedMovementType(movementType);

		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;
			AICharacterMovementComponent characterMovement = AICharacterMovementComponent.Cast(
				agent.GetMovementComponent());
			if (characterMovement)
				characterMovement.SetMovementTypeWanted(movementType);
		}
	}

	// A construcao pode ter acabado de spawnar; solicita rebuild do navmesh e aguarda os tiles
	// antes de amostrar as posicoes de posto
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

		// Dispara rebuild do navmesh apenas na primeira poll
		if (pollsLeft >= NAV_MAX_POLLS)
		{
			AIWorld aiWorld = GetGame().GetAIWorld();
			if (aiWorld)
				aiWorld.RequestNavmeshRebuild(mins, maxs);
		}

		// Tiles prontos ou sem polls restantes: tenta guarnecer de qualquer forma
		if (TilesReady(navmesh, mins, maxs) || pollsLeft <= 0)
		{
			TryGarrison(group, center, radius, MAX_RETRIES);
			return;
		}

		GetGame().GetCallqueue().CallLater(EnsureNavmeshReady, NAV_POLL_MS, false, group, center, radius, pollsLeft - 1);
	}

	// Percorre cada tile sob a area da construcao; retorna true somente quando todos estao carregados
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
				// Amostra 3 alturas: piso, meio e teto da construcao
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

	// Encontra postos via detector, teleporta cada membro do grupo para um posto e trava o movimento.
	// Re-tenta se o navmesh ainda nao estiver pronto.
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

		// Coleta posicoes ja ocupadas por guarnicoes anteriores para nao reutiliza-las
		array<vector> excluded = {};
		GatherOccupied(excluded);

		array<ref SPT_GarrisonPost> posts = {};
		SPT_EGarrisonDetectResult result = SPT_GarrisonDetector.DetectPosts(
			group.GetWorld(), center, pathing, agents.Count(), excluded, posts);

		// Navmesh nao pronto ou sem posicoes ainda: agenda re-tentativa
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

		if (assigned > 0)
			Print(string.Format("[SPT_Garrison] CQB estatico aplicado | grupo=%1 | soldadosPosicionados=%2 | centro=%3",
				group, assigned, center));
	}

	// Callback de re-tentativa de guarnicao
	protected void RetryGarrison(SCR_AIGroup group, vector center, float radius, int retriesLeft)
	{
		TryGarrison(group, center, radius, retriesLeft);
	}

	// Encaixa um soldado no posto inicial; a trava e removida somente quando uma rota interna valida existe.
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

	// Inicia o loop de manutencao (hold poll) se ainda nao estiver rodando
	protected void EnsureHoldPoll()
	{
		if (m_bHoldPollRunning)
			return;
		GetGame().GetCallqueue().CallLater(HoldPoll, HOLD_POLL_MS, true);
		m_bHoldPollRunning = true;
	}

	// Loop periodico executado enquanto houver soldados guarnecidos:
	// levanta quem estiver deitado (prone) e mantem a direcao de vigilancia de cada um
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

			SPT_AIMovementLockHelper.ApplyLockState(h.m_Entity, true);

			CharacterControllerComponent cc = CharacterControllerComponent.Cast(h.m_Entity.FindComponent(CharacterControllerComponent));
			if (cc && cc.GetStance() == ECharacterStance.PRONE)
				cc.SetStanceChange(ECharacterStanceChange.STANCECHANGE_TOERECTED);

			if (!IsGroupInteriorPatrolling(h.m_Group))
				FaceDirection(h);
		}

		UpdateInteriorPatrols();

		if (m_aHolds.IsEmpty() && m_aInteriorPatrols.IsEmpty())
		{
			GetGame().GetCallqueue().Remove(HoldPoll);
			m_bHoldPollRunning = false;
		}
	}

	protected void StartAreaPatrol(
		SCR_AIGroup group,
		vector areaCenter,
		float areaRadius,
		AIPathfindingComponent pathing,
		SCR_EAIGroupFormation formation,
		EMovementType movementType,
		int navPollsLeft)
	{
		if (!group)
			return;

		array<ref SPT_GarrisonPost> areaPosts = {};
		CollectRadialPatrolPosts(group.GetWorld(), areaCenter, areaRadius, pathing, areaPosts);
		if (areaPosts.Count() < 2 && navPollsLeft > 0)
		{
			RequestPatrolNavmeshTiles(
				group.GetWorld(),
				areaCenter,
				areaRadius,
				pathing.GetNavmeshComponent());
			if (navPollsLeft == PATROL_NAV_MAX_POLLS)
				Print(string.Format("[SPT_Garrison] Aguardando navmesh da patrulha | grupo=%1 | centro=%2 | raio=%3",
					group, areaCenter, areaRadius));
			GetGame().GetCallqueue().CallLater(
				StartAreaPatrol,
				PATROL_NAV_POLL_MS,
				false,
				group,
				areaCenter,
				areaRadius,
				pathing,
				formation,
				movementType,
				navPollsLeft - 1);
			return;
		}
		if (areaPosts.Count() < 2)
			CollectPatrolFallbackPosts(group.GetWorld(), areaCenter, areaRadius, pathing, areaPosts);
		if (areaPosts.Count() < 2)
		{
			Print(string.Format("[SPT_Garrison] Patrulha nao iniciada: menos de 2 pontos validos | grupo=%1 | centro=%2 | raio=%3",
				group, areaCenter, areaRadius), LogLevel.WARNING);
			return;
		}

		SPT_GarrisonInteriorPatrol patrol = new SPT_GarrisonInteriorPatrol(
			group,
			areaCenter,
			formation,
			movementType);
		foreach (SPT_GarrisonPost post : areaPosts)
			patrol.m_Posts.Insert(new SPT_GarrisonPost(post.m_Pos, post.m_Facing));

		RelocatePatrolGroupToNavmeshStart(group, areaCenter, areaRadius, pathing, areaPosts);
		ApplyPatrolMovementSettings(group, formation, movementType);

		patrol.m_iWaitPolls = Math.RandomInt(PATROL_WAIT_MIN_POLLS, PATROL_WAIT_MAX_POLLS + 1);
		m_aInteriorPatrols.Insert(patrol);
		EnsureHoldPoll();
		Print(string.Format("[SPT_Garrison] Patrulha de area iniciada | grupo=%1 | pontos=%2 | centroCidade=%3 | raio=%4 | formacao=%5 | movimento=%6",
			group,
			patrol.m_Posts.Count(),
			patrol.m_BuildingCenter,
			areaRadius,
			SCR_Enum.GetEnumName(SCR_EAIGroupFormation, formation),
			SCR_Enum.GetEnumName(EMovementType, movementType)));
	}

	protected bool RelocatePatrolGroupToNavmeshStart(
		notnull SCR_AIGroup group,
		vector areaCenter,
		float areaRadius,
		notnull AIPathfindingComponent pathing,
		notnull array<ref SPT_GarrisonPost> patrolPosts)
	{
		array<vector> buildingCenters = {};
		SPT_GarrisonDetector.CollectBuildingCenters(group.GetWorld(), areaCenter, areaRadius, buildingCenters);

		vector startPosition;
		float bestBuildingDistanceSq = -1;
		foreach (SPT_GarrisonPost post : patrolPosts)
		{
			float nearestBuildingSq = 1000000000.0;
			foreach (vector buildingCenter : buildingCenters)
			{
				float distanceSq = HorizontalDistanceSq(post.m_Pos, buildingCenter);
				if (distanceSq < nearestBuildingSq)
					nearestBuildingSq = distanceSq;
			}
			if (nearestBuildingSq <= bestBuildingDistanceSq)
				continue;
			bestBuildingDistanceSq = nearestBuildingSq;
			startPosition = post.m_Pos;
		}

		if (bestBuildingDistanceSq < 0)
			return false;

		array<AIAgent> agents = {};
		group.GetAgents(agents);
		int relocated;
		foreach (int index, AIAgent agent : agents)
		{
			if (!agent)
				continue;
			IEntity entity = agent.GetControlledEntity();
			BaseGameEntity gameEntity = BaseGameEntity.Cast(entity);
			if (!gameEntity)
				continue;

			float angle = index * 137.5 * Math.PI / 180.0;
			float distance = 1.5 + index * 0.35;
			vector candidate = startPosition + Vector(
				Math.Cos(angle) * distance,
				0,
				Math.Sin(angle) * distance);
			vector navPosition;
			if (!pathing.GetClosestPositionOnNavmesh(candidate, "4 3 4", navPosition))
				navPosition = startPosition;

			vector transform[4];
			entity.GetWorldTransform(transform);
			transform[3] = navPosition;
			gameEntity.Teleport(transform);
			relocated++;
		}

		Print(string.Format("[SPT_Garrison] Grupo de patrulha reposicionado no navmesh externo | grupo=%1 | soldados=%2 | posicao=%3 | distanciaConstrucao=%4m",
			group, relocated, startPosition, Math.Sqrt(bestBuildingDistanceSq)));
		return relocated > 0;
	}

	protected void RequestPatrolNavmeshTiles(
		BaseWorld world,
		vector areaCenter,
		float areaRadius,
		NavmeshWorldComponent navmesh)
	{
		if (!world || !navmesh)
			return;

		navmesh.LoadTileIn(areaCenter);
		for (int ring = 1; ring <= PATROL_RING_COUNT; ring++)
		{
			float distance = areaRadius * ring / PATROL_RING_COUNT;
			for (int sample = 0; sample < PATROL_SAMPLES_PER_RING; sample++)
			{
				float angle = sample * 2.0 * Math.PI / PATROL_SAMPLES_PER_RING;
				vector candidate = areaCenter + Vector(
					Math.Cos(angle) * distance,
					0,
					Math.Sin(angle) * distance);
				candidate[1] = world.GetSurfaceY(candidate[0], candidate[2]) + 0.2;
				navmesh.LoadTileIn(candidate);
			}
		}
	}

	protected void CollectRadialPatrolPosts(
		BaseWorld world,
		vector areaCenter,
		float areaRadius,
		AIPathfindingComponent pathing,
		out array<ref SPT_GarrisonPost> outPosts)
	{
		outPosts = {};
		for (int ring = 1; ring <= PATROL_RING_COUNT; ring++)
		{
			float distance = areaRadius * ring / PATROL_RING_COUNT;
			for (int sample = 0; sample < PATROL_SAMPLES_PER_RING; sample++)
			{
				float angle = sample * 2.0 * Math.PI / PATROL_SAMPLES_PER_RING;
				vector candidate = areaCenter + Vector(
					Math.Cos(angle) * distance,
					0,
					Math.Sin(angle) * distance);
				candidate[1] = world.GetSurfaceY(candidate[0], candidate[2]) + 0.2;

				vector navPosition;
				if (!pathing.GetClosestPositionOnNavmesh(candidate, "15 5 15", navPosition))
					continue;
				if (vector.DistanceSq(navPosition, areaCenter) > areaRadius * areaRadius)
					continue;

				bool duplicate;
				foreach (SPT_GarrisonPost existing : outPosts)
				{
					if (vector.DistanceSq(existing.m_Pos, navPosition) < 100.0)
					{
						duplicate = true;
						break;
					}
				}
				if (duplicate)
					continue;

				vector facing = areaCenter - navPosition;
				facing[1] = 0;
				if (facing.LengthSq() > 0.001)
					facing.Normalize();
				outPosts.Insert(new SPT_GarrisonPost(navPosition, facing));
			}
		}

	}

	protected void CollectPatrolFallbackPosts(
		BaseWorld world,
		vector areaCenter,
		float areaRadius,
		AIPathfindingComponent pathing,
		out array<ref SPT_GarrisonPost> outPosts)
	{
		array<vector> buildingCenters = {};
		SPT_GarrisonDetector.CollectBuildingCenters(world, areaCenter, areaRadius, buildingCenters);
		foreach (vector buildingCenter : buildingCenters)
		{
			vector navPosition;
			if (!pathing.GetClosestPositionOnNavmesh(buildingCenter, "8 5 8", navPosition))
				continue;

			bool duplicate;
			foreach (SPT_GarrisonPost existing : outPosts)
			{
				if (vector.DistanceSq(existing.m_Pos, navPosition) < 25.0)
				{
					duplicate = true;
					break;
				}
			}
			if (duplicate)
				continue;

			vector facing = areaCenter - navPosition;
			facing[1] = 0;
			if (facing.LengthSq() > 0.001)
				facing.Normalize();
			outPosts.Insert(new SPT_GarrisonPost(navPosition, facing));
		}

		Print(string.Format("[SPT_Garrison] Fallback de patrulha por centros de construcao | pontos=%1 | centro=%2 | raio=%3",
			outPosts.Count(), areaCenter, areaRadius));
	}

	protected void UpdateInteriorPatrols()
	{
		for (int i = m_aInteriorPatrols.Count() - 1; i >= 0; i--)
		{
			SPT_GarrisonInteriorPatrol patrol = m_aInteriorPatrols[i];
			if (!patrol || !patrol.m_Group)
			{
				m_aInteriorPatrols.Remove(i);
				continue;
			}

			ApplyPatrolMovementSettings(
				patrol.m_Group,
				patrol.m_eFormation,
				patrol.m_eMovementType);

			if (patrol.m_Group.GetCurrentWaypoint())
				continue;

			if (patrol.m_Waypoint)
				SCR_EntityHelper.DeleteEntityAndChildren(patrol.m_Waypoint);
			patrol.m_Waypoint = null;
			if (patrol.m_iWaitPolls > 0)
			{
				patrol.m_iWaitPolls--;
				continue;
			}

			CreateNextInteriorWaypoint(patrol);
			patrol.m_iWaitPolls = Math.RandomInt(PATROL_WAIT_MIN_POLLS, PATROL_WAIT_MAX_POLLS + 1);
		}
	}

	protected void CreateNextInteriorWaypoint(SPT_GarrisonInteriorPatrol patrol)
	{
		if (!patrol || patrol.m_Posts.Count() < 2)
			return;

		int nextPost = Math.RandomInt(0, patrol.m_Posts.Count());
		if (nextPost == patrol.m_iLastPost)
			nextPost = (nextPost + 1) % patrol.m_Posts.Count();

		if (!m_bPatrolWaypointLoadAttempted)
		{
			m_bPatrolWaypointLoadAttempted = true;
			m_PatrolWaypointResource = Resource.Load(m_sPatrolWaypointPrefab);
			if (!m_PatrolWaypointResource)
				Print(string.Format("[SPT_Garrison] Resource.Load retornou null para o waypoint interno: %1", m_sPatrolWaypointPrefab), LogLevel.ERROR);
		}

		if (!m_PatrolWaypointResource)
			return;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		Math3D.MatrixIdentity4(params.Transform);
		params.Transform[3] = patrol.m_Posts[nextPost].m_Pos;

		IEntity waypointEntity = GetGame().SpawnEntityPrefab(m_PatrolWaypointResource, patrol.m_Group.GetWorld(), params);
		AIWaypoint waypoint = AIWaypoint.Cast(waypointEntity);
		if (!waypoint)
		{
			string waypointClass = "<null>";
			if (waypointEntity)
				waypointClass = waypointEntity.ClassName();
			Print(string.Format("[SPT_Garrison] Spawn do waypoint interno falhou | recurso=%1 | entidade=%2 | classe=%3",
				m_sPatrolWaypointPrefab, waypointEntity, waypointClass), LogLevel.ERROR);
			if (waypointEntity)
				SCR_EntityHelper.DeleteEntityAndChildren(waypointEntity);
			return;
		}

		waypoint.SetCompletionRadius(1.0);
		waypoint.SetCompletionYPrecision(1.5);
		patrol.m_iLastPost = nextPost;
		patrol.m_Waypoint = waypoint;
		patrol.m_Group.AddWaypoint(waypoint);
		Print(string.Format("[SPT_Garrison] Waypoint de patrulha adicionado | grupo=%1 | destino=%2 | indice=%3",
			patrol.m_Group, patrol.m_Posts[nextPost].m_Pos, nextPost));
	}

	protected bool IsGroupInteriorPatrolling(SCR_AIGroup group)
	{
		foreach (SPT_GarrisonInteriorPatrol patrol : m_aInteriorPatrols)
		{
			if (patrol.m_Group == group)
				return true;
		}
		return false;
	}

	// Ajusta o angulo de olhar do soldado em direcao ao ponto de vigilancia do seu posto
	protected void FaceDirection(SPT_GarrisonHold h)
	{
		if (!h.m_Utility || !h.m_Utility.m_LookAction)
			return;
		if (h.m_Facing.Length() < 0.001)
			return;

		vector target = h.m_Entity.GetOrigin() + h.m_Facing * FACE_PROJECT_M + "0 1.5 0";
		h.m_Utility.m_LookAction.LookAt(target, SCR_AILookAction.PRIO_DANGER_EVENT, FACE_DURATION);
	}

	// Retorna true se algum membro deste grupo estiver guarnecido.
	// Usado pelo bloqueio de fumaca para impedir que IA entrincheirada lance fumaca em si mesma.
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

	// Coleta as posicoes de todos os soldados atualmente guarnecidos para evitar sobreposicao
	protected void GatherOccupied(out array<vector> excluded)
	{
		foreach (SPT_GarrisonHold h : m_aHolds)
		{
			if (h.m_Entity)
				excluded.Insert(h.m_Entity.GetOrigin());
		}
	}

	// Coloca o grupo em modo de combate "atirar a vontade" assim que estiverem posicionados
	protected void SetGroupEngaging(SCR_AIGroup group)
	{
		SCR_AIGroupUtilityComponent gu = SCR_AIGroupUtilityComponent.Cast(group.FindComponent(SCR_AIGroupUtilityComponent));
		if (gu)
			gu.SetCombatMode(EAIGroupCombatMode.FIRE_AT_WILL);
	}

	protected void SetMaximumAgentSkill(notnull array<AIAgent> agents)
	{
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			SCR_AICombatComponent combat = SCR_AICombatComponent.Cast(agent.FindComponent(SCR_AICombatComponent));
			if (!combat)
				continue;

			combat.SetAISkill(EAISkill.EXPERT);
			combat.SetPerceptionFactor(3.0);
			combat.SetFireRateCoef(2.0, true);
		}
	}

	// O pathfinding de qualquer membro do grupo serve, todos compartilham o mesmo navmesh
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

	protected float HorizontalDistanceSq(vector a, vector b)
	{
		float deltaX = a[0] - b[0];
		float deltaZ = a[2] - b[2];
		return deltaX * deltaX + deltaZ * deltaZ;
	}

	// Libera um grupo: destrava o movimento de todos os membros guarnecidos e remove os registros
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

		for (int p = m_aInteriorPatrols.Count() - 1; p >= 0; p--)
		{
			SPT_GarrisonInteriorPatrol patrol = m_aInteriorPatrols[p];
			if (patrol.m_Group != group)
				continue;
			if (patrol.m_Waypoint)
				SCR_EntityHelper.DeleteEntityAndChildren(patrol.m_Waypoint);
			m_aInteriorPatrols.Remove(p);
		}
	}
}
