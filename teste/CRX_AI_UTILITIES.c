  /* ------------------------------------------------------------------------------------------------ /
 / CRX-EAI Base Class ( #CRX-EAIBaseClass )                                                          /
/ ------------------------------------------------------------------------------------------------ */

//------------------------------------------------------------------------------------------------
class CRX_AIUtilities : CRX_AIBase
{
	private static IEntity m_Entity;
	
	//------------------------------------------------------------------------------------------------
	//! Target Visible
	private static ref TraceParam m_TraceParamTargetVisible;
	
	private static ref array<IEntity> m_aTraceParamTargetVisibleExclude;
	
	private const float TRACE_TARGET_VISIBLE_CHECK_RESULT_THRESHOLD = 0.5;
	
	#ifdef WORKBENCH
	private static ref array<ref Shape> m_aTraceParamTargetVisibleDebugShapes = {};
	#endif
	
	//------------------------------------------------------------------------------------------------
	//! Building
	private static bool m_bIsInBuilding;
	
	private static SCR_AIWorld m_AIWorld;
	
	private static IEntity m_BuildingEntity;
	
	private static bool m_bIsInBuildingTrace;
	
	private static string m_sBuildingSearchPurpose;
	
	private static ref TraceParam m_TraceParamInBuilding;
	
	private static ref array<IEntity> m_aBuildingEntities = {};
	
	private ref array<IEntity> m_TraceParamInBuildingExcludeArray;
	
	#ifdef WORKBENCH
	private static int m_iQueryBuildingEntitiesBySphereDebugCounter;
	#endif
	
	//------------------------------------------------------------------------------------------------
	//! Trench
	private static IEntity m_TrenchEntity;
	
	private static int m_iTrenchQueryEntitiesBySphereCounter;
	
	#ifdef WORKBENCH
	private static int m_iQueryTrenchEntitiesBySphereDebugCounter;
	#endif
	
	#ifdef WORKBENCH
	private static ref array<ref Shape> m_aTrenchEntityDebugCylinders = {};
	#endif
	
	//---------------------------------------------------------------------------
	void CRX_AIUtilities()
	{
		m_AIWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
	}
	
	//---------------------------------------------------------------------------
	static void SetAIWorld(SCR_AIWorld aiWorld)
	{
		m_AIWorld = aiWorld;
	}
	
	//---------------------------------------------------------------------------
	static bool IsTargetVisible(notnull IEntity entity, vector position = vector.Zero)
	{
		if (!m_TraceParamTargetVisible)
		{
			m_TraceParamTargetVisible = new TraceParam();
			m_TraceParamTargetVisible.TargetLayers = EPhysicsLayerDefs.FireGeometry;
			
			m_TraceParamTargetVisible.Flags = TraceFlags.ENTS | TraceFlags.OCEAN | TraceFlags.WORLD | TraceFlags.ANY_CONTACT;
			
			m_aTraceParamTargetVisibleExclude = {};
			
			m_TraceParamTargetVisible.ExcludeArray = m_aTraceParamTargetVisibleExclude;
		}
		
		m_aTraceParamTargetVisibleExclude.Clear();
		m_aTraceParamTargetVisibleExclude.Insert(entity);
		
		IEntity entityParent = entity.GetParent();
		
		if (entityParent)
		{
			m_aTraceParamTargetVisibleExclude.Insert(entityParent);
			
			IEntity entityParentParent = entityParent.GetParent();
			
			if (entityParentParent)
				m_aTraceParamTargetVisibleExclude.Insert(entityParentParent);
		}
		
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		
		if (character)
			m_TraceParamTargetVisible.Start = character.EyePosition();
		else
			m_TraceParamTargetVisible.Start = entity.GetOrigin();
		
		vector traceEndPosIdeal = position + Vector(0, 2.0, 0);
		
		m_TraceParamTargetVisible.End = vector.Lerp(m_TraceParamTargetVisible.Start, traceEndPosIdeal, TRACE_TARGET_VISIBLE_CHECK_RESULT_THRESHOLD);
		
		float traceResult = GetGame().GetWorld().TraceMove(m_TraceParamTargetVisible, null);
		
		bool visible = traceResult == 1.0;
		
		#ifdef WORKBENCH
		if (DiagMenu.GetBool(SCR_DebugMenuID.DEBUGUI_AI_SHOW_DEBUG_SHAPES))
		{
			m_aTraceParamTargetVisibleDebugShapes.Clear();
			
			int lineColor = Color.RED;
			
			if (visible)
				lineColor = Color.GREEN;
			
			vector lineVerts[2];
			
			lineVerts[1] = m_TraceParamTargetVisible.End;
			lineVerts[0] = m_TraceParamTargetVisible.Start;
			
			Shape lineShape = Shape.CreateLines(lineColor, ShapeFlags.DEFAULT, lineVerts, 2.0);
			
			m_aTraceParamTargetVisibleDebugShapes.Insert(lineShape);
			
			if (traceResult != 1.0)
			{
				vector hitPos = m_TraceParamTargetVisible.Start + traceResult * (m_TraceParamTargetVisible.End - m_TraceParamTargetVisible.Start);
				
				Shape sphereShape = Shape.CreateSphere(Color.RED, ShapeFlags.DEFAULT, hitPos, 0.2);
				
				m_aTraceParamTargetVisibleDebugShapes.Insert(sphereShape);
			}
		}
		#endif
		
		return visible;
	}
	
	//------------------------------------------------------------------------------------------------
	static bool IsInBuilding(notnull IEntity entity, SCR_AIInfoComponent infoComponent, float searchRadius = 1.0, vector position = vector.Zero, string buildingSearchPurpose = "Unknown")
	{
		m_Entity = entity;
		
		m_bIsInBuilding = false;
		
		m_sBuildingSearchPurpose = buildingSearchPurpose;
		
		bool isInBuilding = infoComponent.GetIsInBuilding();
		
		float worldTime_ms = GetGame().GetWorld().GetWorldTime();
		
		ChimeraCharacter chimeraCharacter = ChimeraCharacter.Cast(entity);
		
		float isInBuildingTimeout_ms = infoComponent.GetIsInBuildingTimeout();
		
		if (chimeraCharacter)
		{
			bool isInVehicle = chimeraCharacter.IsInVehicle();
			
			if (isInVehicle)
			{
				isInBuilding = false;
				
				isInBuildingTimeout_ms = (worldTime_ms + 5000.0);
			}
		}
		
		if (worldTime_ms > isInBuildingTimeout_ms)
			isInBuilding = false;
		
		if (isInBuilding)
		{
			m_bIsInBuilding = true;
			
			#ifdef WORKBENCH
			// SCR_AIDebugVisualization.VisualizeMessage(entity, "CRX_AIUtilities > IsInsideBuilding > Cached", EAIDebugCategory.CRX, 1, Color.Yellow, 10);
			
			// SCR_AIDebugVisualization.VisualizeMessage(entity, "CRX_AIUtilities > IsInsideBuilding > Cached", EAIDebugCategory.NONE, 1, Color.Yellow, 10, true);
			#endif
		}
		else
		{
			if (buildingSearchPurpose == "BuildingSearch")
				isInBuildingTimeout_ms = worldTime_ms;
			
			if (worldTime_ms < isInBuildingTimeout_ms)
			{
				#ifdef WORKBENCH
				// SCR_AIDebugVisualization.VisualizeMessage(entity, "CRX_AIUtilities > IsInsideBuilding > Timeout", EAIDebugCategory.CRX, 1, Color.Gray, 10);
				#endif
			}
			else
			{
				#ifdef WORKBENCH
				m_iQueryBuildingEntitiesBySphereDebugCounter = 0;
				#endif
				
				if (position == vector.Zero)
					position = entity.GetOrigin();
				
				//---------------------------------------------------------------------------
				m_bIsInBuildingTrace = false;
				
				bool isInBuildingTrace = false;
				
				if (!m_TraceParamInBuilding)
				{
					m_TraceParamInBuilding = new TraceParam();
					
					m_TraceParamInBuilding.Flags = TraceFlags.ENTS;
					
//					m_TraceParamInBuildingExcludeArray = {};
					
//					m_TraceParam.ExcludeArray = m_TraceParamInBuildingExcludeArray;
				}
				
				m_TraceParamInBuilding.Exclude = entity;
				
				vector traceMoveStart = entity.GetOrigin();
				
				traceMoveStart = position;
				
				m_TraceParamInBuilding.Start = traceMoveStart;
				
				vector traceMoveEnd = traceMoveStart + Vector(0.0, 5.0, 0.0);
				
				// traceMoveEnd[1] = traceMoveEnd[1] + 5.0;
				
				// traceMoveEnd[1] = 5.0 + traceMoveStart[1];
				
				m_TraceParamInBuilding.End = traceMoveEnd;
				
				float traceDistance = GetGame().GetWorld().TraceMove(m_TraceParamInBuilding, null);
				
				// float traceDistance = GetGame().GetWorld().TraceMove(m_TraceParamInBuilding, TraceParamInBuildingCallback);
				
				#ifdef WORKBENCH
				// SCR_AIDebugVisualization.VisualizeDebugMessage(entity, "CRX_AIUtilities > IsInsideBuilding > Trace Distance" + " > " + traceDistance, 1.0, Color.White, 10.0);
				#endif
				
				float positionY = position[1];
				
				IEntity traceMoveEntity = m_TraceParamInBuilding.TraceEnt;
				
				float tracePositionSurfaceY = GetGame().GetWorld().GetSurfaceY(position[0], position[2]);
				
				float positionHeight = positionY - tracePositionSurfaceY;
				
				if (traceMoveEntity || positionHeight > 1.0)
				{
					// string colliderName = traceMove.ColliderName;
					
					// string traceMaterial = traceMove.TraceMaterial;
					
					// SurfaceProperties surfaceProps = traceMove.SurfaceProps;
					
					typename entityType;
					
					SCR_DestructibleBuildingEntity destructibleBuildingEntity;
					
					if (traceMoveEntity)
					{
						entityType = traceMoveEntity.Type();
						
						destructibleBuildingEntity = SCR_DestructibleBuildingEntity.Cast(traceMoveEntity);
					}
					
					if (positionHeight > 3.0 || destructibleBuildingEntity)
					{
						isInBuildingTrace = true;
						
						#ifdef WORKBENCH
						SCR_AIDebugVisualization.VisualizeDebugMessage(entity, "CRX_AIUtilities > IsInsideBuilding > Trace > DestructibleBuildingEntity" + " > " + entityType + " > " + traceDistance, 3.0, Color.White, 10.0);
						#endif
					}
				}
				else
				{
					#ifdef WORKBENCH
					// SCR_AIDebugVisualization.VisualizeMessage(entity, "CRX_AIUtilities > IsInsideBuilding > Trace" + " > " + "Null" + " > " + traceDistance, EAIDebugCategory.NONE, 1, Color.Gray, 10, true);
					#endif
				}
				//---------------------------------------------------------------------------
				
				if (isInBuildingTrace || m_bIsInBuildingTrace)
				{
					// GetGame().GetWorld().QueryEntitiesBySphere(position, 3.0, FirstEntity, QueryEntities);
					
					// GetGame().GetWorld().QueryEntitiesByAABB(position + vector.One, position + "5 5 5", GetFirstBuildingEntity);
					
					// GetGame().GetWorld().QueryEntitiesByAABB(position + vector.One, position + vector.One, GetFirstBuildingEntity);
					
					// GetGame().GetWorld().QueryEntitiesByAABB(position + vector.One, position + vector.One * 5.0, FirstEntity, QueryEntities, EQueryEntitiesFlags.STATIC);
					
					// GetGame().GetWorld().QueryEntitiesByAABB(position + vector.One, position + "5 5 5", GetFirstBuildingEntity, QueryBuildingEntity); // EQueryEntitiesFlags.STATIC
					
					GetGame().GetWorld().QueryEntitiesBySphere(position, searchRadius, QueryBuildingEntityBySphere, QueryBuildingEntitiesBySphere, EQueryEntitiesFlags.STATIC | EQueryEntitiesFlags.NO_PROXIES);
					
					#ifdef WORKBENCH
					// SCR_AIDebugVisualization.VisualizeDebugMessage(entity, "CRX_AIUtilities > IsInsideBuilding > QueryEntitiesBySphereCount" + " > " + m_iQueryBuildingEntitiesBySphereDebugCounter, 1.0, Color.SpringGreen);
					#endif
				}
				
				if (m_bIsInBuilding)
				{
//					bool damaged;
//					
//					bool destroyed;
//					
//					vector boundsMins, boundsMaxs;
					
					infoComponent.SetIsInBuilding(true);
					
					// SCR_DestructibleBuildingEntity destructibleBuildingEntity = SCR_DestructibleBuildingEntity.Cast(m_BuildingEntity);
					
					isInBuildingTimeout_ms = (worldTime_ms + 15000.0);
					
					infoComponent.SetBuildingEntity(m_BuildingEntity);
					
//					m_BuildingEntity.GetBounds(boundsMins, boundsMaxs);
//					
//					if (destructibleBuildingEntity)
//					{
//						#ifdef ENABLE_BUILDING_DESTRUCTION
//						damaged = destructibleBuildingEntity.GetDamaged();
//						
//						destroyed = destructibleBuildingEntity.GetDestroyed();
//						#endif
//					}
					
					infoComponent.SetIsInBuildingTimeout(isInBuildingTimeout_ms);
					
					#ifdef WORKBENCH
					// SCR_AIDebugVisualization.VisualizeMessage(entity, "CRX_AIUtilities > IsInsideBuilding > Success", EAIDebugCategory.NONE, 3, Color.Gray, 10, true);
					#endif
					
					#ifdef WORKBENCH
					// SCR_AIDebugVisualization.VisualizeMessage(entity, "CRX_AIUtilities > IsInsideBuilding > BuildingEntity" + " > " + boundsMin + " > " + boundsMax, EAIDebugCategory.CRX, 5, Color.SpringGreen, 10);
					#endif
					
					#ifdef WORKBENCH
					vector boundsMin, boundsMax;
					
					m_BuildingEntity.GetBounds(boundsMin, boundsMax);
					
					// SCR_AIDebugVisualization.VisualizeMessage(entity, "CRX_AIUtilities > IsInsideBuilding > BuildingEntity" + " > " + m_BuildingEntity + " > " + boundsMin + " > " + boundsMax, EAIDebugCategory.CRX, 5, Color.SpringGreen, 10);
					
					// SCR_AIDebugVisualization.VisualizeMessage(entity, "CRX_AIUtilities > IsInsideBuilding > BuildingEntity" + " > " + m_BuildingEntity + " > " + boundsMin + " > " + boundsMax, EAIDebugCategory.NONE, 5, Color.SpringGreen, 10, true);
					#endif
				}
				else
				{
					infoComponent.SetIsInBuilding(false);
					
					isInBuildingTimeout_ms = (worldTime_ms + 5000.0);
					
					infoComponent.SetIsInBuildingTimeout(isInBuildingTimeout_ms);
					
					#ifdef WORKBENCH
					// SCR_AIDebugVisualization.VisualizeMessage(entity, "CRX_AIUtilities > IsInsideBuilding > Timeout", EAIDebugCategory.CRX, 1, Color.SpringGreen, 10);
					
					// SCR_AIDebugVisualization.VisualizeMessage(entity, "CRX_AIUtilities > IsInsideBuilding > Fail", EAIDebugCategory.NONE, 3, Color.SpringGreen, 10, true);
					#endif
				}
			}
		}
		
		return m_bIsInBuilding;
	}
	
	//------------------------------------------------------------------------------------------------
	static bool TraceParamInBuildingCallback(notnull IEntity entity)
	{
		SCR_DestructibleBuildingEntity destructibleBuildingEntity = SCR_DestructibleBuildingEntity.Cast(entity);
		
		if (destructibleBuildingEntity)
		{
			m_bIsInBuildingTrace = true;
			
			#ifdef WORKBENCH
			SCR_AIDebugVisualization.VisualizeDebugMessage(entity, "CRX_AIUtilities > IsInsideBuilding > TraceParamInBuildingCallback", 1.0, Color.Pink);
			#endif
			
			return false;
		}
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	static bool QueryBuildingEntityBySphere(IEntity entity)
	{
		#ifdef WORKBENCH
		// Print(entity.Type());
		
		m_iQueryBuildingEntitiesBySphereDebugCounter++;
		#endif
		
		SCR_DestructibleBuildingEntity destructibleBuildingEntity = SCR_DestructibleBuildingEntity.Cast(entity);
		
		if (destructibleBuildingEntity)
		{
			m_bIsInBuilding = true;
			
			m_BuildingEntity = entity;
			
			vector boundsMin, boundsMax;
			
			m_BuildingEntity.GetBounds(boundsMin, boundsMax);
			
			if (boundsMax[0] < 3.0 || boundsMax[1] < 3.0 || boundsMax[2] < 3.0) // if (boundsMax[0] < 1.0 || boundsMax[1] < 3.0 || boundsMax[2] < 1.0)
			{
				m_bIsInBuilding = false;
				
				m_BuildingEntity = null;
			}
			
			if (m_BuildingEntity && m_sBuildingSearchPurpose == "BuildingGarrison")
			{
				if (m_AIWorld)
				{
					array<IEntity> buildingGarrisonEntities = m_AIWorld.m_aBuildingGarrisonEntities;
					
					if (buildingGarrisonEntities.Find(m_BuildingEntity) > -1)
					{
						m_bIsInBuilding = false;
						
						m_BuildingEntity = null;
					}
				}
			}
			
			#ifdef WORKBENCH
			// Print("QueryBuildingEntityBySphere" + " > " + m_iQueryBuildingEntitiesBySphereDebugCounter);
			
			// SCR_AIDebugVisualization.VisualizeDebugMessage(m_Entity, "CRX_AIUtilities > IsInsideBuilding > QueryBuildingEntityBySphere" + " > " + m_iQueryBuildingEntitiesBySphereDebugCounter, 3.0, Color.SpringGreen, 11.0);
			#endif
		}
		
		if (m_bIsInBuilding)
			return false;
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	static bool QueryBuildingEntitiesBySphere(IEntity entity)
	{
		if (m_bIsInBuilding)
			return false;
		
		#ifdef WORKBENCH
		// Print(entity.Type());
		
		m_iQueryBuildingEntitiesBySphereDebugCounter++;
		#endif
		
		SCR_DestructibleBuildingEntity destructibleBuildingEntity = SCR_DestructibleBuildingEntity.Cast(entity);
		
		if (destructibleBuildingEntity)
		{
			m_bIsInBuilding = true;
			
			m_BuildingEntity = entity;
			
			vector boundsMin, boundsMax;
			
			m_BuildingEntity.GetBounds(boundsMin, boundsMax);
			
			if (boundsMax[0] < 3.0 || boundsMax[1] < 3.0 || boundsMax[2] < 3.0) // if (boundsMax[0] < 1.0 || boundsMax[1] < 3.0 || boundsMax[2] < 1.0)
			{
				m_bIsInBuilding = false;
				
				m_BuildingEntity = null;
			}
			
			if (m_BuildingEntity && m_sBuildingSearchPurpose == "BuildingGarrison")
			{
				if (m_AIWorld)
				{
					array<IEntity> buildingGarrisonEntities = m_AIWorld.m_aBuildingGarrisonEntities;
					
					if (buildingGarrisonEntities.Find(m_BuildingEntity) > -1)
					{
						m_bIsInBuilding = false;
						
						m_BuildingEntity = null;
					}
				}
			}
			
			#ifdef WORKBENCH_CRX
			// Print("QueryBuildingEntitiesBySphere" + " > " + m_iQueryBuildingEntitiesBySphereDebugCounter);
			
			// SCR_AIDebugVisualization.VisualizeDebugMessage(m_Entity, "CRX_AIUtilities > IsInsideBuilding > QueryBuildingEntitiesBySphere" + " > " + m_iQueryBuildingEntitiesBySphereDebugCounter, 3.0, Color.Orange, 11.0);
			#endif
		}
		
		if (m_bIsInBuilding)
			return false;
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	static bool IsInBuilding(notnull IEntity entity, SCR_AICombatComponent combatComponent, vector position = vector.Zero)
	{
		m_Entity = entity;
		
		m_bIsInBuilding = false;
		
		m_BuildingEntity = null;
		
		float searchRadius = 1.0;
		
		if (position == vector.Zero)
			position = entity.GetOrigin();
		
//		if (combatComponent)
//		{
//			array<float> buildingBounds = {};
//			
//			buildingBounds.Insert(boundsMax);
//			
//			combatComponent.buildingBounds = buildingBounds;
//		}
		
		// m_aBuildingEntities = combatComponent.m_aBuildingEntities;
		
		// GetGame().GetWorld().QueryEntitiesByAABB(position + vector.One, position + vector.One, GetFirstBuildingEntity);
		
		GetGame().GetWorld().QueryEntitiesBySphere(position, searchRadius, GetFirstBuildingEntity, null, EQueryEntitiesFlags.STATIC | EQueryEntitiesFlags.NO_PROXIES);
		
		if (m_BuildingEntity)
		{
			m_bIsInBuilding = true;
			
//			vector boundsMin, boundsMax;
//			
//			m_BuildingEntity.GetBounds(boundsMin, boundsMax);
			
			if (combatComponent)
			{
				// combatComponent.m_vBuildingBounds = boundsMax;
				
				// combatComponent.m_BuildingEntity = m_BuildingEntity;
				
//				IEntity buildingEntity = combatComponent.m_BuildingEntity;
//				
//				if (buildingEntity)
//					return m_bIsInBuilding;
				
				// combatComponent.m_aBuildingEntities.Insert(m_BuildingEntity);
			}
			
			// m_BuildingEntity = null;
		}
//		else
//		{
//			searchRadius = 50.0;
//			
//			GetGame().GetWorld().QueryEntitiesBySphere(position, searchRadius, GetFirstBuildingEntity, null, EQueryEntitiesFlags.STATIC | EQueryEntitiesFlags.NO_PROXIES);
//		}
		
		if (combatComponent)
			combatComponent.m_BuildingEntity = m_BuildingEntity;
		
//		if (m_bIsInBuilding)
//		{
//			#ifdef WORKBENCH
//			// SCR_AIDebugVisualization.VisualizeMessage(entity, "CRX_AIUtilities > IsInsideBuilding", EAIDebugCategory.NONE, 1, Color.SpringGreen, 10, true);
//			#endif
//		}
		
		return m_bIsInBuilding;
	}
	
	//------------------------------------------------------------------------------------------------
	static bool GetFirstBuildingEntity(IEntity entity)
	{
		Building building = Building.Cast(entity);
		
		if (building)
		{
			m_bIsInBuilding = true;
			
			m_BuildingEntity = entity;
			
			vector boundsMin, boundsMax;
			
			m_BuildingEntity.GetBounds(boundsMin, boundsMax);
			
			#ifdef WORKBENCH
			// SCR_AIDebugVisualization.VisualizeMessage(m_Entity, "CRX_AIUtilities > IsInsideBuilding > GetFirstBuildingEntity", EAIDebugCategory.NONE, 1, Color.SpringGreen, 10, true);
			#endif
			
			if (m_aBuildingEntities.Contains(building))
			{
				m_BuildingEntity = null;
				
				return true;
			}
			else
			{
				m_aBuildingEntities.Insert(m_BuildingEntity);
				
				return false;
			}
			
			return false;
		}
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	static bool IsInTrench(SCR_ChimeraAIAgent chimeraAIAgent, float trenchSearchRadius = 1.0, vector trenchSearchPosition = vector.Zero)
	{
		bool isInTrench;
		
		m_TrenchEntity = null;
		
		m_iTrenchQueryEntitiesBySphereCounter = 0;
		
		// GetGame().GetWorld().QueryEntitiesBySphere(trenchSearchPosition, trenchSearchRadius, QueryTrenchEntityBySphere, null, EQueryEntitiesFlags.ALL);
		
		GetGame().GetWorld().QueryEntitiesBySphere(trenchSearchPosition, trenchSearchRadius, QueryTrenchEntityBySphere, null, EQueryEntitiesFlags.STATIC | EQueryEntitiesFlags.NO_PROXIES);
		
		// GetGame().GetWorld().QueryEntitiesBySphere(trenchSearchPosition, trenchSearchRadius, QueryTrenchEntityBySphere, QueryTrenchEntitiesBySphere, EQueryEntitiesFlags.STATIC | EQueryEntitiesFlags.NO_PROXIES);
		
		if (m_TrenchEntity && m_iTrenchQueryEntitiesBySphereCounter > 2)
		{
			isInTrench = true;
			
			AIGroup group = chimeraAIAgent.GetParentGroup();
			
			if (group)
			{
				SCR_AIGroup aiGroup = SCR_AIGroup.Cast(group);
				
				if (aiGroup)
					aiGroup.SetTrenchEntity(m_TrenchEntity);
			}
			
			#ifdef WORKBENCH
			// vector worldBoundsMin, worldBoundsMax;
			
			// vector origin = m_TrenchEntity.GetOrigin();
			
			// float originY = origin[1];
			
			// m_TrenchEntity.GetWorldBounds(worldBoundsMin, worldBoundsMax);
			
			// float worldBoundsMaxY = worldBoundsMax[1];
			
			// float trenchY = worldBoundsMaxY - originY;
			
			// IEntity entity = chimeraAIAgent.GetControlledEntity();
			
			// SCR_AIDebugVisualization.VisualizeMessage(entity, "CRX_AIUtilities > IsInTrench", EAIDebugCategory.NONE, 5, Color.SpringGreen, 10, true);
			
			// SCR_AIDebugVisualization.VisualizeDebugMessage(m_TrenchEntity, "CRX_AIUtilities > IsInTrench" + " > " + trenchY, 5.0, Color.SpringGreen);
			
			// SCR_AIDebugVisualization.VisualizeMessage(m_TrenchEntity, "CRX_AIUtilities > IsInTrench > ErodedEmbankment", EAIDebugCategory.NONE, 5, Color.SpringGreen, 10, true);
			#endif
		}
		else if (m_TrenchEntity)
		{
			#ifdef WORKBENCH
			SCR_AIDebugVisualization.VisualizeDebugMessage(m_TrenchEntity, "CRX_AIUtilities > IsInTrench > ErodedEmbankment > Fail", 3.0, Color.Yellow, 13.0);
			#endif
		}
		
		return isInTrench;
	}
	
	//------------------------------------------------------------------------------------------------
	static bool QueryTrenchEntityBySphere(IEntity entity)
	{
		if (entity)
		{
			// Print(entity);
			
			string className = entity.ClassName();
			
			#ifdef WORKBENCH
			m_iQueryTrenchEntitiesBySphereDebugCounter++;
			#endif
			
			if (className == "StaticModelEntity")
			
			// if (className == "GenericEntity" || className == "StaticModelEntity")
			{
				string name = entity.GetName();
				
				if (name == "")
				{
					EntityPrefabData prefabData = entity.GetPrefabData();
					
					if (prefabData)
						name = prefabData.GetPrefabName();
				}
				
				bool contains = name.Contains("ErodedEmbankment");
				
				// contains = false;
				
				if (contains)
				{
					m_TrenchEntity = entity;
					
					m_iTrenchQueryEntitiesBySphereCounter++;
					
					#ifdef WORKBENCH
					// SCR_AIDebugVisualization.VisualizeMessage(entity, "CRX_AIUtilities > IsInTrench > QueryTrenchEntityBySphere > ErodedEmbankment", EAIDebugCategory.NONE, 5, Color.SpringGreen, 10, true);
					#endif
					
//					if (m_iTrenchQueryEntitiesBySphereCounter == 1)
//						return true;
//					else
//						return false;
				}
				else
				{
					vector boundsMin, boundsMax;
					
					entity.GetBounds(boundsMin, boundsMax);
					
					// string boundsMaxString = boundsMax.ToString();
					
					vector trenchBoundsMax = Vector(7.584232, 1.844885, 6.299444);
					
					if (boundsMax == trenchBoundsMax)
					{
						m_TrenchEntity = entity;
						
						m_iTrenchQueryEntitiesBySphereCounter++;
						
						#ifdef WORKBENCH
						// SCR_AIDebugVisualization.VisualizeMessage(entity, "CRX_AIUtilities > IsInTrench > QueryTrenchEntityBySphere > Bounds > ErodedEmbankment" + " > " + boundsMax, EAIDebugCategory.NONE, 5, Color.SpringGreen, 10, true);
						#endif
						
//						if (m_iTrenchQueryEntitiesBySphereCounter == 1)
//							return true;
//						else
//							return false;
					}
				}
				
				if (m_iTrenchQueryEntitiesBySphereCounter == 3)
					return false;
				
				// ///////////////////////////////////////////////////////////////////////////////////////////////
				#ifdef WORKBENCH
				
				if (entity && m_TrenchEntity && entity == m_TrenchEntity)
				{
					vector boundsMin, boundsMax;
					
					entity.GetBounds(boundsMin, boundsMax);
					
					vector trenchOrigin = entity.GetOrigin();
					
					// trenchOrigin[1] = trenchOrigin[1] + boundsMax[1];
					
					vector worldBoundsMin, worldBoundsMax;
					
					entity.GetWorldBounds(worldBoundsMin, worldBoundsMax);
					
					// float worldBoundsMaxY = worldBoundsMax[1];
					
					vector trenchEdgePosition = trenchOrigin;
					
					trenchEdgePosition[1] = trenchEdgePosition[1] + boundsMax[1];
					
					if (DiagMenu.GetBool(SCR_DebugMenuID.DEBUGUI_AI_SHOW_DEBUG_SHAPES))
					{
						// Shape trenchEntityDebugCylinder = Shape.CreateCylinder(Color.DODGER_BLUE, ShapeFlags.WIREFRAME, trenchEdgePosition, 0.5, 0.5);
						
						// m_aTrenchEntityDebugCylinders.Insert(trenchEntityDebugCylinder);
					}
					
					trenchEdgePosition = trenchOrigin;
					
					trenchEdgePosition[1] = trenchEdgePosition[1] + worldBoundsMax[1];
					
					float trenchSurfaceY = trenchEdgePosition[1] - trenchOrigin[1];
					
					trenchSurfaceY = worldBoundsMax[1] - trenchOrigin[1];
					
					float surfaceY = GetGame().GetWorld().GetSurfaceY(trenchOrigin[0], trenchOrigin[2]);
					
					// vector trenchEntityTop = Vector((worldBoundsMax[0] + worldBoundsMin[0]) * 0.5, worldBoundsMax[1], (worldBoundsMax[2] + worldBoundsMin[2]) * 0.5);
					
					vector trenchSurfacePosition = trenchOrigin;
					
					trenchSurfacePosition[1] = surfaceY;
					
					vector trenchTerrainPosition = trenchOrigin;
					
					float trenchDistanceToSurface = vector.Distance(trenchSurfacePosition, trenchTerrainPosition);
					
					trenchSurfaceY = boundsMax[1] - trenchDistanceToSurface;
					
					float trenchSurfaceYThreshold = trenchSurfaceY / 1.5;
					
					// trenchSurfaceY = surfaceY - trenchOrigin;
					
					// trenchSurfaceY = worldBoundsMax[1] - surfaceY;
					
					// trenchSurfaceY = worldBoundsMax[1] - boundsMax[1];
					
					// SCR_AIDebugVisualization.VisualizeDebugMessage(m_TrenchEntity, "CRX_AIUtilities > IsInTrench > QueryTrenchEntityBySphere > TrenchBoundYMax" + " > " + trenchSurfaceY + " > " + trenchSurfaceYThreshold, 7.0, Color.White);
				}
				
				#endif
				// ///////////////////////////////////////////////////////////////////////////////////////////////
			}
		}
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	static bool QueryTrenchEntitiesBySphere(IEntity entity)
	{
		if (entity)
		{
			// Print(entity);
			
			string className = entity.ClassName();
			
			#ifdef WORKBENCH
			m_iQueryTrenchEntitiesBySphereDebugCounter++;
			#endif
			
			if (m_iTrenchQueryEntitiesBySphereCounter > 2)
				return false;
//			else
//			{
//				#ifdef WORKBENCH
//				SCR_AIDebugVisualization.VisualizeDebugMessage(entity, "CRX_AIUtilities > IsInTrench > QueryTrenchEntitiesBySphere > ErodedEmbankment", 5.0, Color.DodgerBlue);
//				#endif
//			}
			
			if (className == "StaticModelEntity")
			{
				string name = entity.GetName();
				
				if (name == "")
				{
					EntityPrefabData prefabData = entity.GetPrefabData();
					
					if (prefabData)
						name = prefabData.GetPrefabName();
				}
				
				bool contains = name.Contains("ErodedEmbankment");
				
				// contains = false;
				
				if (contains)
				{
					m_TrenchEntity = entity;
					
					m_iTrenchQueryEntitiesBySphereCounter++;
					
					#ifdef WORKBENCH
					// SCR_AIDebugVisualization.VisualizeMessage(entity, "CRX_AIUtilities > IsInTrench > QueryTrenchEntitiesBySphere > ErodedEmbankment", EAIDebugCategory.NONE, 5, Color.DodgerBlue, 10, true);
					#endif
					
					return false;
				}
				else
				{
					vector boundsMin, boundsMax;
					
					entity.GetBounds(boundsMin, boundsMax);
					
					// string boundsMaxString = boundsMax.ToString();
					
					vector trenchBoundsMax = Vector(7.584232, 1.844885, 6.299444);
					
//					trenchBoundsMax = Vector(7.938350, 1.844885, 6.305583);
//					
//					trenchBoundsMax = Vector(7.584232, 2.206865, 6.299444);
//					
//					trenchBoundsMax = Vector(8.400788, 1.948338, 5.940144);
					
					if (boundsMax == trenchBoundsMax)
					{
						m_TrenchEntity = entity;
						
						m_iTrenchQueryEntitiesBySphereCounter++;
						
						#ifdef WORKBENCH
						// SCR_AIDebugVisualization.VisualizeMessage(entity, "CRX_AIUtilities > IsInTrench > QueryTrenchEntitiesBySphere > Bounds > ErodedEmbankment" + " > " + boundsMax, EAIDebugCategory.NONE, 5, Color.SpringGreen, 10, true);
						#endif
						
						return false;
					}
				}
			}
		}
		
		return true;
	}
	
	//--------------------------------------------------------------------------------------------
	static void SetStanceChange(SCR_AIInfoComponent infoComponent, CharacterControllerComponent characterControllerComponent, ECharacterStanceChange characterStanceChange)
	{
		if (infoComponent)
		{
			bool stanceChangeAutonomous = infoComponent.GetStanceChangeAutonomous();
			
			if (stanceChangeAutonomous)
				characterControllerComponent.SetStanceChange(characterStanceChange);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	static void ToggleFlashlightState(CRX_EAIFlashlightState flashlightState, notnull SCR_GadgetManagerComponent gadgetManagerComponent)
	{
		if (gadgetManagerComponent)
		{
			array<SCR_GadgetComponent> gadgets = gadgetManagerComponent.GetGadgetsByType(EGadgetType.FLASHLIGHT);
			
			if (gadgets)
			{
				if (gadgets.IsEmpty())
					return;
				
				foreach (SCR_GadgetComponent gadget : gadgets)
				{
					InventoryItemComponent inventoryItemComponent = InventoryItemComponent.Cast(gadget.GetOwner().FindComponent(InventoryItemComponent));
					
					if (inventoryItemComponent)
					{
						EquipmentStorageSlot equipmentStorageSlot = EquipmentStorageSlot.Cast(inventoryItemComponent.GetParentSlot());
						
						if (equipmentStorageSlot)
						{
							bool newFlashlightState;
							
							if (flashlightState == CRX_EAIFlashlightState.ON)
								newFlashlightState = true;
							
							bool occluded = equipmentStorageSlot.IsOccluded();
							
							if (occluded)
								return;
							
							gadget.ToggleActive(newFlashlightState, SCR_EUseContext.FROM_ACTION);
							
							break;
						}
					}
				}
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	static SCR_ChimeraAIAgent GetChimeraAIAgentFromEntity(IEntity entity)
	{
		SCR_ChimeraAIAgent chimeraAIAgent;
		
		ChimeraCharacter chimeraCharacter = ChimeraCharacter.Cast(entity);
		
		if (chimeraCharacter)
		{
			AIControlComponent controlComponent = chimeraCharacter.GetAIControlComponent();
			
			if (controlComponent)
			{
				AIAgent agent = controlComponent.GetAIAgent();
				
				if (agent)
					chimeraAIAgent = SCR_ChimeraAIAgent.Cast(agent);
			}
		}
		
		return chimeraAIAgent;
	}
	
	//------------------------------------------------------------------------------------------------
	static SCR_AIGroup GetChimeraAIGroupFromChimeraAIAgent(SCR_ChimeraAIAgent chimeraAIAgent)
	{
		SCR_AIGroup aiGroup;
		
		if (chimeraAIAgent)
		{
			AIGroup group = chimeraAIAgent.GetParentGroup();
			
			if (group)
				aiGroup = SCR_AIGroup.Cast(group);
		}
		
		return aiGroup;
	}
	
	//------------------------------------------------------------------------------------------------
	static bool VehicleHasGunner(IEntity vehicleEntity)
	{
		bool vehicleHasGunner = false;
		
		array<IEntity> occupants = {};
		
		SCR_BaseCompartmentManagerComponent baseCompartmentManagerComponent = SCR_BaseCompartmentManagerComponent.Cast(vehicleEntity.FindComponent(SCR_BaseCompartmentManagerComponent));
		
		if (baseCompartmentManagerComponent)
			baseCompartmentManagerComponent.GetOccupantsOfType(occupants, ECompartmentType.TURRET);
		
		if (occupants.Count() > 0)
			vehicleHasGunner = true;
		
		return vehicleHasGunner;
	}
}