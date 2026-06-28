  /* ------------------------------------------------------------------------------------------------ /
 / CRX-EAI Modded Class ( #CRX-EAIModdedClass )                                                      /
/ ------------------------------------------------------------------------------------------------ */

//------------------------------------------------------------------------------------------------
modded class SCR_AIBehaviorBase : SCR_AIActionBase
{
	// protected IEntity m_Entity;
	
	//------------------------------------------------------------------------------------------------
	protected SCR_ChimeraAIAgent m_ChimeraAIAgent;
	protected SCR_ChimeraCharacter m_ChimeraCharacter;
	
	//------------------------------------------------------------------------------------------------
	protected SCR_AIInfoComponent m_InfoComponent;
	protected SCR_AICombatMoveState m_CombatMoveState;
	
	//------------------------------------------------------------------------------------------------
	protected SCR_AISettingsComponent m_SettingsComponent;
	
	//------------------------------------------------------------------------------------------------
	protected SCR_DamageManagerComponent m_DamageManagerComponent;
	protected SCR_CharacterControllerComponent m_CharacterControllerComponent;
	
	//------------------------------------------------------------------------------------------------
	void SCR_AIBehaviorBase(SCR_AIUtilityComponent utility, SCR_AIActivityBase groupActivity)
	{
		if (utility)
		{
			m_bUseCombatMove = true;
			
			AIAgent agent = utility.GetOwner();
			
			if (agent)
			{
				m_InfoComponent = utility.GetInfoComponent();
				
				m_CombatMoveState = utility.GetCombatMoveState();
				
				m_ChimeraAIAgent = SCR_ChimeraAIAgent.Cast(agent);
				
				// m_SettingsComponent = SCR_AISettingsComponent.GetInstance();
				
				if (m_ChimeraAIAgent)
				{
					IEntity entity = m_ChimeraAIAgent.GetControlledEntity();
					
					if (entity)
					{
						// m_Entity = entity;
						
						SCR_ChimeraCharacter chimeraCharacter = SCR_ChimeraCharacter.Cast(entity);
						
						if (chimeraCharacter)
						{
							m_ChimeraCharacter = chimeraCharacter;
							
							m_DamageManagerComponent = chimeraCharacter.GetDamageManager();
						}
					}
				}
				
				m_CharacterControllerComponent = utility.GetCharacterControllerComponent();
				
//				if (m_SettingsComponent)
//				{
//					bool useCombatMoveSystemPermanent = m_SettingsComponent.m_bUseCombatMoveSystemPermanent;
//					
//					m_bUseCombatMove = useCombatMoveSystemPermanent;
//				}
			}
		}
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_AIIdleBehavior : SCR_AIBehaviorBase
{
	protected AIAgent m_Agent;
	protected AIGroup m_Group;
	
	//------------------------------------------------------------------------------------------------
	protected SCR_AIWorld m_AIWorld;
	
	//------------------------------------------------------------------------------------------------
	protected bool m_bOnActionSelected;
	
	//------------------------------------------------------------------------------------------------
	protected bool m_bIsBuildingGarrison;
	
	//------------------------------------------------------------------------------------------------
	protected float m_fCustomEvaluateTimeout_ms;
	
	//------------------------------------------------------------------------------------------------
	protected SCR_AIThreatSystem m_ThreatSystem;
	protected SCR_AICombatComponent m_CombatComponent;
	
	// ///////////////////////////////////////////////////////////////////////////////////////////////
	#ifdef CRX_DEBUG
	protected ref Shape m_BuildingGarrisonSearchPositionDebugCylinder;
	#endif
	// ///////////////////////////////////////////////////////////////////////////////////////////////
	
	//------------------------------------------------------------------------------------------------
	void SCR_AIIdleBehavior(SCR_AIUtilityComponent utility, SCR_AIActivityBase groupActivity)
	{
		if (utility)
		{
			m_Agent = utility.GetOwner();
			
			if (m_Agent)
			{
				m_ThreatSystem = utility.m_ThreatSystem;
				
				m_CombatComponent = m_Utility.m_CombatComponent;
				
				m_AIWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnActionSelected()
	{
		m_bOnActionSelected = true;
		
		vanilla.OnActionSelected();
		
		m_bIsBuildingGarrison = false;
		
		if (m_bUseCombatMove)
			m_CombatMoveState.ResetRequest();
		
		if (m_Agent)
			m_Group = m_Agent.GetParentGroup();
		
		m_Utility.m_fInfoshareReactionTiming_ms = 0;
		
		if (m_CombatComponent)
			m_CombatComponent.m_BuildingEntity = null;
		
		if (m_InfoComponent)
		{
			m_InfoComponent.SetBuildingEntity(null);
			
			CRX_EAIWeaponRaised weaponRaised = m_InfoComponent.GetWeaponRaised();
			
			if (weaponRaised == CRX_EAIWeaponRaised.AUTONOMOUS)
			{
				m_CharacterControllerComponent.SetWeaponADS(false);
				
				m_CharacterControllerComponent.SetWeaponRaised(false);
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnActionDeselected()
	{
		m_bOnActionSelected = false;
		
		vanilla.OnActionDeselected();
	}
	
	//------------------------------------------------------------------------------------------------
	override float CustomEvaluate()
	{
		float customEvaluate = vanilla.CustomEvaluate();
		
		if (m_bOnActionSelected)
		{
			float worldTime_ms = GetGame().GetWorld().GetWorldTime();
			
			if (worldTime_ms > m_fCustomEvaluateTimeout_ms)
			{
				m_fCustomEvaluateTimeout_ms = (worldTime_ms + 5000.0);
				
				// ///////////////////////////////////////////////////////////////////////////////////////////////
				#ifdef CRX_DEVELOPMENT_WIP
				
				if (m_Group)
				{
					if (m_bIsBuildingGarrison)
						m_fCustomEvaluateTimeout_ms = (worldTime_ms + 30000.0);
					else
					{
						AIAgent leaderAgent = m_Group.GetLeaderAgent();
						
						if (leaderAgent && m_Agent == leaderAgent)
						{
							IEntity leaderEntity = m_Group.GetLeaderEntity();
							
							if (leaderEntity)
							{
								vector origin = leaderEntity.GetOrigin();
								
								SCR_ChimeraAIAgent chimeraAIAgent = SCR_ChimeraAIAgent.Cast(leaderAgent);
								
								if (chimeraAIAgent)
								{
									SCR_AIInfoComponent infoComponent = chimeraAIAgent.m_InfoComponent;
									
									if (infoComponent)
									{
										float searchRadius = 15.0;
										
										vector searchPosition = s_AIRandomGenerator.GenerateRandomPointInRadius(5.0, 30.0, origin);
										
										if (Math.RandomFloat01() < 0.5)
										{
											m_fCustomEvaluateTimeout_ms = (worldTime_ms + 3000.0);
											
											searchPosition = s_AIRandomGenerator.GenerateRandomPointInRadius(1.0, 3.0, origin);
										}
										
										float surfaceY = GetGame().GetWorld().GetSurfaceY(searchPosition[0], searchPosition[2]);
										
										searchPosition[1] = surfaceY;
										
										// leaderEntity.SetOrigin(searchPosition);
										
										string buildingSearchPurpose = "BuildingGarrison";
										
										// ///////////////////////////////////////////////////////////////////////////////////////////////
										#ifdef CRX_DEBUG
										m_BuildingGarrisonSearchPositionDebugCylinder = Shape.CreateCylinder(Color.CYAN, ShapeFlags.WIREFRAME, searchPosition, 1.0, 3.0);
										#endif
										// ///////////////////////////////////////////////////////////////////////////////////////////////
										
										bool isInBuilding = CRX_AIUtilities.IsInBuilding(leaderEntity, infoComponent, searchRadius, searchPosition, buildingSearchPurpose);
										
										if (isInBuilding)
										{
											IEntity buildingEntity = infoComponent.GetBuildingEntity();
											
											if (buildingEntity)
											{
												vector boundsMin, boundsMax;
												
												array<AIAgent> agents = {};
												
												m_Group.GetAgents(agents);
												
												m_bIsBuildingGarrison = true;
												
												SCR_AIUtilityComponent utilityComponent;
												
												buildingEntity.GetBounds(boundsMin, boundsMax);
												
												float buildingY = boundsMax[1] / 1.5; // 1.5;
												
												vector buildingOrigin = buildingEntity.GetOrigin();
												
												m_AIWorld.m_aBuildingGarrisonEntities.Insert(buildingEntity);
												
												// chimeraAIAgent.SetIsBuildingGarrison(m_bIsBuildingGarrison);
												
												float buildingRadius = (boundsMax[0] + boundsMax[2]) / 2.0; // 1.5;
												
												surfaceY = GetGame().GetWorld().GetSurfaceY(buildingOrigin[0], buildingOrigin[2]);
												
												// ///////////////////////////////////////////////////////////////////////////////////////////////
												#ifdef CRX_DEBUG
												vector buildingGarrisonDebugPosition = buildingOrigin;
												
												buildingGarrisonDebugPosition[1] = buildingGarrisonDebugPosition[1] + (buildingY * 2.0);
												
												m_BuildingGarrisonSearchPositionDebugCylinder = Shape.CreateCylinder(Color.ORANGE, ShapeFlags.WIREFRAME, buildingGarrisonDebugPosition, 1.0, 3.0);
												#endif
												// ///////////////////////////////////////////////////////////////////////////////////////////////
												
												buildingOrigin[1] = surfaceY;
												
												foreach (AIAgent agent : agents)
												{
													if (agent)
													{
														chimeraAIAgent = SCR_ChimeraAIAgent.Cast(agent);
														
														if (chimeraAIAgent)
														{
															utilityComponent = chimeraAIAgent.m_UtilityComponent;
															
															chimeraAIAgent.SetIsBuildingGarrison(m_bIsBuildingGarrison);
															
															if (utilityComponent)
															{
																float buildingHeightRandom = Math.RandomFloat(1.0, buildingY);
																
																vector movePosition = s_AIRandomGenerator.GenerateRandomPointInRadius(1.0, buildingRadius, buildingOrigin);
																
																movePosition[1] = surfaceY;
																
																movePosition[1] = movePosition[1] + buildingHeightRandom;
																
																SCR_AIMoveIndividuallyBehavior moveIndividuallyBehavior = new SCR_AIMoveIndividuallyBehavior(utilityComponent, null, movePosition, priority: SCR_AIActionBase.PRIORITY_BEHAVIOR_MOVE_INDIVIDUALLY, priorityLevel: SCR_AIActionBase.PRIORITY_LEVEL_NORMAL, radius: 1.5);
																
																utilityComponent.AddAction(moveIndividuallyBehavior);
															}
														}
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
				
				#endif
				// ///////////////////////////////////////////////////////////////////////////////////////////////
			}
		}
		
		return customEvaluate;
	}
}