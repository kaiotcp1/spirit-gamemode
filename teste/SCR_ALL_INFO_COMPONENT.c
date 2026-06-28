  /* ------------------------------------------------------------------------------------------------ /
 / CRX-EAI Modded Class ( #CRX-EAIModdedClass )                                                      /
/ ------------------------------------------------------------------------------------------------ */

//------------------------------------------------------------------------------------------------
//! CRX Enums
enum CRX_EAIRankType
{
	CRX,
	VANILLA
}

enum CRX_EAIRearmType
{
	NONE,
	DEFAULT,
	WEAPONS,
	MAGAZINES
}

enum CRX_EAIMovementType
{
	IDLE,
	WALK,
	RUN,
	SPRINT,
	AUTONOMOUS
}

enum CRX_EAIWeaponRaised
{
	RAISE,
	LOWER,
	AUTONOMOUS
}

//enum CRX_EAIMoveTypeState
//{
//	HOLD,
//	MOVE,
//	FIRETEAM,
//	AUTONOMOUS
//}

enum CRX_EAIWeaponFireMode
{
	CRX,
	HOLD,
	SINGLE,
	BURST,
	SUPPRESSIVE,
	AUTONOMOUS
}

enum CRX_EAISimulationState
{
	ON,
	OFF,
	AUTONOMOUS
}

enum CRX_EAIFlashlightState
{
	ON,
	OFF,
	AUTONOMOUS
}

//------------------------------------------------------------------------------------------------
//! Vanilla Enums
modded enum EMovementType
{
	AUTONOMOUS
}

modded enum ECharacterStance
{
	AUTONOMOUS
}

//------------------------------------------------------------------------------------------------
modded class SCR_AIInfoComponent : SCR_AIInfoBaseComponent
{
	private IEntity m_Entity;
	
	//------------------------------------------------------------------------------------------------
	private bool m_bIsEditMode;
	private bool m_bInitialize = true;
	private bool m_bMissionHeaderSettings = true;
	
	//------------------------------------------------------------------------------------------------
	private int m_iAgentIndex;
	private int m_iAgentsCount;
	
	//------------------------------------------------------------------------------------------------
	private SCR_AIWorld m_AIWorld;
	
	private vector m_vSpawnPositionOrigin;
	
	//------------------------------------------------------------------------------------------------
	private SCR_ChimeraAIAgent m_ChimeraAIAgent;
	protected ChimeraCharacter m_ChimeraCharacter;
	
	//------------------------------------------------------------------------------------------------
	private SCR_MissionHeader m_MissionHeader;
	private SCR_AIConfigComponent m_ConfigComponent;
	private SCR_AISettingsComponent m_SettingsComponent;
	
	//------------------------------------------------------------------------------------------------
	private SCR_AIGroup m_Group;
	private SCR_AIGroupInfoComponent m_GroupInfoComponent;
	private SCR_AIGroupUtilityComponent m_GroupUtilityComponent;
	
	//------------------------------------------------------------------------------------------------
	private SCR_GadgetManagerComponent m_GadgetManagerComponent;
	private CharacterControllerComponent m_CharacterControllerComponent;
	
//------------------------------------------------------------------------------------------------
//! Game Master
	
	//------------------------------------------------------------------------------------------------
	//! Config
	private bool m_bUseConfigFiles;
	
	//------------------------------------------------------------------------------------------------
	//! Character
	private CRX_EAIRank m_eRank;
	private CRX_EAIRankType m_eRankType;
	
	//------------------------------------------------------------------------------------------------
	private float m_fPerceptionSafe;
	private float m_fPerceptionVigilant;
	private float m_fPerceptionAlerted;
	private float m_fPerceptionThreatened;
	
	private float m_fPerceptionModifier;
	
	//------------------------------------------------------------------------------------------------
	private float m_fAimAccuracyError;
	private float m_fAimAccuracyErrorOriginal;
	private float m_fAimAccuracyErrorModifier;
	
	//------------------------------------------------------------------------------------------------
	private ECharacterStance m_eStance                = ECharacterStance.AUTONOMOUS;
	private CRX_EAIRearmType m_eRearmType             = CRX_EAIRearmType.DEFAULT;
	private CRX_EAIMovementType m_eMovementType       = CRX_EAIMovementType.AUTONOMOUS;
	private CRX_EAIWeaponRaised m_eWeaponRaised       = CRX_EAIWeaponRaised.AUTONOMOUS;
	private CRX_EAIWeaponFireMode m_eWeaponFireMode   = CRX_EAIWeaponFireMode.AUTONOMOUS;
	private CRX_EAIFlashlightState m_eFlashlightState = CRX_EAIFlashlightState.AUTONOMOUS;
	
	//------------------------------------------------------------------------------------------------
	//! Character Utilities
	private int m_iFleeChance;
	
	//------------------------------------------------------------------------------------------------
	private bool m_bHoldPosition;
	private int m_iHoldPositionRadius;
	private vector m_vHoldPositionOrigin;
	
	//------------------------------------------------------------------------------------------------
	private bool m_bIdleObserve;
	private bool m_bForceStayInVehicle;
	
	//------------------------------------------------------------------------------------------------
	private CRX_EAISimulationState m_eSimulationState;
	
	//------------------------------------------------------------------------------------------------
	private int m_iDangerReactionChance;
	
	//------------------------------------------------------------------------------------------------
	private bool m_bDisableWeaponControls;
	private bool m_bDisableMovementControls;
	
	//------------------------------------------------------------------------------------------------
	private int m_iMagazineConsumptionChance;
	
	//------------------------------------------------------------------------------------------------
	private float m_fAttackReactionDelayModifier;
	
//! Game Master	
//------------------------------------------------------------------------------------------------
	
	//------------------------------------------------------------------------------------------------
	//! Functionality
	
	//------------------------------------------------------------------------------------------------
	//! Trench
	float m_fTrenchSurfaceY;
	
	private SCR_AIGroupFireteam m_Fireteam;
	
	private bool m_bRocketLauncherSuppression;
	
	private IEntity m_CurrentItemInHandsEntity;
	
	//------------------------------------------------------------------------------------------------
	private EAISkill m_eHierarchySkill;
	private CRX_ECharacterRank m_eHierarchyRank;
	
	//------------------------------------------------------------------------------------------------
	private bool m_bReturnToFormation = true;
	
	private bool m_bStanceChangeAutonomous = true;
	private bool m_bMovementTypeAutonomous = true;
	
	//------------------------------------------------------------------------------------------------
	private bool m_bGlobalSettings = true;
	// private bool m_bConfigSettingsPrioritize;
	private bool m_bGlobalSettingsOverrideExclude;
	
	//------------------------------------------------------------------------------------------------
	protected bool m_bIsInBuilding;
	protected IEntity m_BuildingEntity;
	protected float m_fIsInBuildingTimeout;
	// protected ref array<IEntity> m_BuildingEntitiesExclude = {};
	
	//------------------------------------------------------------------------------------------------
	override protected void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		
		m_bInitialize = true;
		
		m_ChimeraAIAgent = SCR_ChimeraAIAgent.Cast(owner);
		
		if (GetGame().GetAIWorld())
			m_AIWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		
		m_SettingsComponent = SCR_AISettingsComponent.GetInstance();
		
		if (m_ChimeraAIAgent)
		{
			m_Entity = m_ChimeraAIAgent.GetControlledEntity();
			
			if (m_Entity)
			{
				vector origin = m_Entity.GetOrigin();
				
				m_vHoldPositionOrigin = origin;
				
				m_vSpawnPositionOrigin = origin;
				
				ChimeraCharacter chimeraCharacter = ChimeraCharacter.Cast(m_Entity);
				
				m_MissionHeader = SCR_MissionHeader.Cast(GetGame().GetMissionHeader());
				
				m_GadgetManagerComponent = SCR_GadgetManagerComponent.GetGadgetManager(m_Entity);
				
				if (chimeraCharacter)
					m_CharacterControllerComponent = chimeraCharacter.GetCharacterController();
				
//				SCR_AIUtilityComponent utilityComponent = m_ChimeraAIAgent.m_UtilityComponent;
//				
//				if (utilityComponent)
//					m_CharacterControllerComponent = m_ChimeraAIAgent.m_CharacterControllerComponent;
				
				m_CombatComponent = SCR_AICombatComponent.Cast(m_Entity.FindComponent(SCR_AICombatComponent));
			}
		}
		
		// GetGame().GetCallqueue().CallLater(CRX_AIInfoComponentInitializeDelayed, initializeDelay, false, initialize, initializeType);
	}
	
	//------------------------------------------------------------------------------------------------
	void CRX_AIInfoComponentInitializeDelayed(bool initialize, string initializeType = "", AIGroup group = null)
	{
//		if (initializeType == "SCR_AIInfoComponent")
//			Print("");
		
		#ifdef WORKBENCH
		// SCR_AIDebugVisualization.VisualizeMessage(m_Entity, "SCR_AIInfoComponent > CRX_AIInfoComponentInitializeDelayed > InitializeType" + " > " + initialize + " > " + m_bInitialize + " > " + initializeType, EAIDebugCategory.NONE, 5, Color.White, 10, true);
		#endif
		
		if (initialize && m_bInitialize)
		{
			group = m_ChimeraAIAgent.GetParentGroup();
			
			// ///////////////////////////////////////////////////////////////////////////////////////////////
			#ifdef CRX_DEVELOPMENT
			
			float worldTime_ms = GetGame().GetWorld().GetWorldTime();
			
			if (worldTime_ms < 3000.0)
				CRX_AIMagazineHandling.RemoveMagazines(m_ChimeraAIAgent);
			
			#endif
			// ///////////////////////////////////////////////////////////////////////////////////////////////
			
			if (group)
			{
				m_Group = SCR_AIGroup.Cast(group);
				
				if (m_Group)
					m_bIsEditMode = m_Group.m_bIsEditMode;
				
				m_iAgentIndex = -1 + group.GetAgentsCount();
				
				m_iAgentsCount = m_Group.GetNumberOfMembersToSpawn();
				
				m_SettingsComponent = SCR_AISettingsComponent.GetInstance();
				
				m_GroupUtilityComponent = m_Group.GetGroupUtilityComponent();
				
				if (m_GroupUtilityComponent)
				{
					m_ConfigComponent = m_GroupUtilityComponent.GetGroupConfigComponent();
					
					m_GroupInfoComponent = m_GroupUtilityComponent.GetGroupInfoComponent();
				}
				
				if (m_GroupInfoComponent)
				{
					m_ChimeraAIAgent.UpdateGroupComponents(group);
					
					SCR_AIThreatSystem threatSystem = this.GetThreatSystem();
					
					if (threatSystem)
						threatSystem.SetGroupInfoComponent(m_GroupInfoComponent);
				}
				
				#ifdef WORKBENCH
				// SCR_AIDebugVisualization.VisualizeMessage(m_Entity, "SCR_AIInfoComponent > CRX_AIInfoComponentInitializeDelayed > SCR_AIGroup" + " > " + agentIndex + " > " + agentsCount, EAIDebugCategory.NONE, 10, Color.White, 13, true);
				#endif
			}
		}
		
		//------------------------------------------------------------------------------------------------
		//! Game Master
		
		if (m_SettingsComponent)
		{
			if (true)
			
			// if (m_bInitialize)
			{
				//------------------------------------------------------------------------------------------------
				//! Config
				m_bUseConfigFiles = m_SettingsComponent.m_bUseConfigFiles;
				
				//------------------------------------------------------------------------------------------------
				//! Character
				m_eStance = m_SettingsComponent.m_eStance;
				
				m_eRankType = m_SettingsComponent.m_eRankType;
				
				m_eRearmType = m_SettingsComponent.m_eRearmType;
				
				if (true)
				
				// if (initialize)
				
				// if (group == m_Group)
				{
					m_eWeaponRaised = m_SettingsComponent.m_eWeaponRaised;
					m_eMovementType = m_SettingsComponent.m_eMovementType;
				}
				
				m_fPerceptionSafe = m_SettingsComponent.m_fPerceptionSafe;
				m_fPerceptionVigilant = m_SettingsComponent.m_fPerceptionVigilant;
				m_fPerceptionAlerted = m_SettingsComponent.m_fPerceptionAlerted;
				m_fPerceptionThreatened = m_SettingsComponent.m_fPerceptionThreatened;
				
				m_fPerceptionModifier = m_SettingsComponent.m_fPerceptionModifier;
				
				m_fAimAccuracyErrorModifier = m_SettingsComponent.m_fAimAccuracyErrorModifier;
				
				//------------------------------------------------------------------------------------------------
				//! Character Utilities
				m_bHoldPosition = m_SettingsComponent.m_bHoldPosition;
				m_iHoldPositionRadius = m_SettingsComponent.m_iHoldPositionRadius;
				
				m_iFleeChance = m_SettingsComponent.m_iFleeChance;
				
				m_bIdleObserve = m_SettingsComponent.m_bIdleObserve;
				
				m_eWeaponFireMode = m_SettingsComponent.m_eWeaponFireMode;
				
				m_eSimulationState = m_SettingsComponent.m_eSimulationState;
				m_eFlashlightState = m_SettingsComponent.m_eFlashlightState;
				
				m_bForceStayInVehicle = m_SettingsComponent.m_bForceStayInVehicle;
				
				m_iDangerReactionChance = m_SettingsComponent.m_iDangerReactionChance;
				
				m_bDisableWeaponControls = m_SettingsComponent.m_bDisableWeaponControls;
				m_bDisableMovementControls = m_SettingsComponent.m_bDisableMovementControls;
				
				m_iMagazineConsumptionChance = m_SettingsComponent.m_iMagazineConsumptionChance;
				
				m_fAttackReactionDelayModifier = m_SettingsComponent.m_fAttackReactionDelayModifier;
			}
			
			//------------------------------------------------------------------------------------------------
			
			if (initialize && m_ConfigComponent)
			{
				m_bIsEditMode = m_ConfigComponent.m_bIsEditMode;
				
				m_bGlobalSettingsOverrideExclude = m_ConfigComponent.m_bGlobalSettingsOverrideExclude;
				
				if (m_bGlobalSettingsOverrideExclude)
				{
					m_bGlobalSettings = false;
					
					m_bMissionHeaderSettings = false;
				}
				
				//------------------------------------------------------------------------------------------------
				//! Character
				if (m_ConfigComponent.m_eRankType != CRX_EAIRankType.CRX)
					m_eRankType = m_ConfigComponent.m_eRankType;
				
				if (m_ConfigComponent.m_eStance != ECharacterStance.AUTONOMOUS)
					m_eStance = m_ConfigComponent.m_eStance;
				
				if (m_ConfigComponent.m_eRearmType != CRX_EAIRearmType.DEFAULT)
					m_eRearmType = m_ConfigComponent.m_eRearmType;
				
				if (m_ConfigComponent.m_eWeaponRaised != CRX_EAIWeaponRaised.AUTONOMOUS)
					m_eWeaponRaised = m_ConfigComponent.m_eWeaponRaised;
				
				if (m_ConfigComponent.m_eMovementType != CRX_EAIMovementType.AUTONOMOUS)
					m_eMovementType = m_ConfigComponent.m_eMovementType;
				
				if (m_ConfigComponent.m_fPerceptionSafe != 1.0)
					m_fPerceptionSafe = m_ConfigComponent.m_fPerceptionSafe;
				
				if (m_ConfigComponent.m_fPerceptionVigilant != 2.5)
					m_fPerceptionVigilant = m_ConfigComponent.m_fPerceptionVigilant;
				
				if (m_ConfigComponent.m_fPerceptionAlerted != 2.5)
					m_fPerceptionAlerted = m_ConfigComponent.m_fPerceptionAlerted;
				
				if (m_ConfigComponent.m_fPerceptionThreatened != 3.0)
					m_fPerceptionThreatened = m_ConfigComponent.m_fPerceptionThreatened;
				
				if (m_ConfigComponent.m_fPerceptionModifier != 0)
					m_fPerceptionModifier = m_ConfigComponent.m_fPerceptionModifier;
				
				if (m_ConfigComponent.m_fAimAccuracyErrorModifier != 0)
					m_fAimAccuracyErrorModifier = m_ConfigComponent.m_fAimAccuracyErrorModifier;
				
				//------------------------------------------------------------------------------------------------
				//! Character Utilities
				if (m_ConfigComponent.m_bHoldPosition != false)
					m_bHoldPosition = m_ConfigComponent.m_bHoldPosition;
				
				if (m_ConfigComponent.m_iHoldPositionRadius != 5.0)
					m_iHoldPositionRadius = m_ConfigComponent.m_iHoldPositionRadius;
				
				if (m_ConfigComponent.m_iFleeChance != 3.0)
					m_iFleeChance = m_ConfigComponent.m_iFleeChance;
				
				if (m_ConfigComponent.m_bIdleObserve != true)
					m_bIdleObserve = m_ConfigComponent.m_bIdleObserve;
				
				if (m_ConfigComponent.m_eWeaponFireMode != CRX_EAIWeaponFireMode.AUTONOMOUS)
					m_eWeaponFireMode = m_ConfigComponent.m_eWeaponFireMode;
				
				if (m_ConfigComponent.m_bForceStayInVehicle != false)
					m_bForceStayInVehicle = m_ConfigComponent.m_bForceStayInVehicle;
				
				if (m_ConfigComponent.m_eSimulationState != CRX_EAISimulationState.AUTONOMOUS)
					m_eSimulationState = m_ConfigComponent.m_eSimulationState;
				
				if (m_ConfigComponent.m_eFlashlightState != CRX_EAIFlashlightState.AUTONOMOUS)
					m_eFlashlightState = m_ConfigComponent.m_eFlashlightState;
				
				if (m_ConfigComponent.m_iDangerReactionChance != 50.0)
					m_iDangerReactionChance = m_ConfigComponent.m_iDangerReactionChance;
				
				if (m_ConfigComponent.m_bDisableWeaponControls != false)
					m_bDisableWeaponControls = m_ConfigComponent.m_bDisableWeaponControls;
				
				if (m_ConfigComponent.m_bDisableMovementControls != false)
					m_bDisableMovementControls = m_ConfigComponent.m_bDisableMovementControls;
				
				if (m_ConfigComponent.m_iMagazineConsumptionChance != 100.0)
					m_iMagazineConsumptionChance = m_ConfigComponent.m_iMagazineConsumptionChance;
				
				if (m_ConfigComponent.m_fAttackReactionDelayModifier != 0)
					m_fAttackReactionDelayModifier = m_ConfigComponent.m_fAttackReactionDelayModifier;
			}
			
			if (initialize && m_bInitialize && m_MissionHeader && m_bMissionHeaderSettings)
			{
				//------------------------------------------------------------------------------------------------
				//! Config
				bool useConfigFiles = m_MissionHeader.m_bUseConfigFiles;
				
				if (useConfigFiles != true)
					m_bUseConfigFiles = useConfigFiles;
				
				//------------------------------------------------------------------------------------------------
				//! Character
				CRX_EAIRankType rankType = m_MissionHeader.m_eRankType;
				
				if (rankType != CRX_EAIRankType.CRX)
					m_eRankType = rankType;
				
				CRX_EAIRearmType rearmType = m_MissionHeader.m_eRearmType;
				
				if (rearmType != CRX_EAIRearmType.DEFAULT)
					m_eRearmType = rearmType;
				
				float perceptionSafe = m_MissionHeader.m_fPerceptionSafe;
				
				if (perceptionSafe != 1.0)
					m_fPerceptionSafe = perceptionSafe;
				
				float perceptionVigilant = m_MissionHeader.m_fPerceptionVigilant;
				
				if (perceptionVigilant != 2.5)
					m_fPerceptionVigilant = perceptionVigilant;
				
				float perceptionModifier = m_MissionHeader.m_fPerceptionModifier;
				
				if (perceptionModifier != 0)
					m_fPerceptionModifier = perceptionModifier;
				
				float aimAccuracyErrorModifier = m_MissionHeader.m_fAimAccuracyErrorModifier;
				
				if (aimAccuracyErrorModifier != 0)
					m_fAimAccuracyErrorModifier = aimAccuracyErrorModifier;
				
				//------------------------------------------------------------------------------------------------
				//! Character Utilities
				int magazineConsumptionChance = m_MissionHeader.m_iMagazineConsumptionChance;
				
				if (magazineConsumptionChance != 100.0)
					m_iMagazineConsumptionChance = magazineConsumptionChance;
				
				float attackReactionDelayModifier = m_MissionHeader.m_fAttackReactionDelayModifier;
				
				if (attackReactionDelayModifier != 0)
					m_fAttackReactionDelayModifier = attackReactionDelayModifier;
			}
			
			if (m_bGlobalSettings)
			{
				//------------------------------------------------------------------------------------------------
				//! Global
				float globalRearmType = m_SettingsComponent.m_eGlobalRearmType;
				
				if (globalRearmType != CRX_EAIRearmType.DEFAULT)
					m_eRearmType = globalRearmType;
				else
					m_SettingsComponent.m_eGlobalRearmType = m_SettingsComponent.m_eGlobalRearmType;
				
				float globalPerceptionSafe = m_SettingsComponent.m_fGlobalPerceptionSafe;
				
				if (globalPerceptionSafe != 1.0)
					m_fPerceptionSafe = globalPerceptionSafe;
				else
					m_SettingsComponent.m_fGlobalPerceptionSafe = m_SettingsComponent.m_fPerceptionSafe;
				
				float globalPerceptionVigilant = m_SettingsComponent.m_fGlobalPerceptionVigilant;
				
				if (globalPerceptionVigilant != 2.5)
					m_fPerceptionVigilant = globalPerceptionVigilant;
				else
					m_SettingsComponent.m_fGlobalPerceptionVigilant = m_SettingsComponent.m_fPerceptionVigilant;
				
				float globalPerceptionModifier = m_SettingsComponent.m_fGlobalPerceptionModifier;
				
				if (globalPerceptionModifier != 0)
					m_fPerceptionModifier = globalPerceptionModifier;
				else
					m_SettingsComponent.m_fGlobalPerceptionModifier = m_SettingsComponent.m_fPerceptionModifier;
				
				float globalAimAccuracyErrorModifier = m_SettingsComponent.m_fGlobalAimAccuracyErrorModifier;
				
				if (globalAimAccuracyErrorModifier != 0)
					m_fAimAccuracyErrorModifier = globalAimAccuracyErrorModifier;
				else
					m_SettingsComponent.m_fGlobalAimAccuracyErrorModifier = m_SettingsComponent.m_fAimAccuracyErrorModifier;
				
				//------------------------------------------------------------------------------------------------
				int globalMagazineConsumptionChance = m_SettingsComponent.m_iGlobalMagazineConsumptionChance;
				
				if (globalMagazineConsumptionChance != 100.0)
					m_iMagazineConsumptionChance = globalMagazineConsumptionChance;
				else
					m_SettingsComponent.m_iGlobalMagazineConsumptionChance = m_SettingsComponent.m_iGlobalMagazineConsumptionChance;
			}
		}
		//! Game Master
		//------------------------------------------------------------------------------------------------
		
		// ///////////////////////////////////////////////////////////////////////////////////////////////
		//------------------------------------------------------------------------------------------------
		//! CRX_ConfigFile.c
		if (m_bInitialize && m_bUseConfigFiles)
		{
			if (m_AIWorld)
			{
				bool applyConfigFilesSettings = true;
				
				bool configFilesSettingsOverrideExclude = m_AIWorld.m_bConfigFilesSettingsOverrideExclude;
				
				if (configFilesSettingsOverrideExclude)
					applyConfigFilesSettings = false;
				else
				{
					configFilesSettingsOverrideExclude = m_ConfigComponent.m_bConfigFilesSettingsOverrideExclude;
					
					if (configFilesSettingsOverrideExclude)
						applyConfigFilesSettings = false;
				}
				
				if (applyConfigFilesSettings)
				{
					CRX_AIInfoComponentConfigFile.LoadConfig(this, configFilesSettingsOverrideExclude);
					
					if (m_SettingsComponent.m_fGlobalPerceptionModifier == 0)
					{
						m_SettingsComponent.m_fGlobalPerceptionSafe     += m_SettingsComponent.m_fPerceptionModifier;
						m_SettingsComponent.m_fGlobalPerceptionVigilant += m_SettingsComponent.m_fPerceptionModifier;
					}
					else
					{
						m_SettingsComponent.m_fGlobalPerceptionSafe     += m_SettingsComponent.m_fGlobalPerceptionModifier;
						m_SettingsComponent.m_fGlobalPerceptionVigilant += m_SettingsComponent.m_fGlobalPerceptionModifier;
					}
					
					m_fPerceptionSafe     = m_SettingsComponent.m_fGlobalPerceptionSafe;
					m_fPerceptionVigilant = m_SettingsComponent.m_fGlobalPerceptionVigilant;
				}
				else
				{
					if (m_ConfigComponent.m_eRearmType != CRX_EAIRearmType.DEFAULT)
						m_eRearmType                = m_ConfigComponent.m_eRearmType;
					
					if (m_ConfigComponent.m_fPerceptionSafe != 1.0)
						m_fPerceptionSafe           = (m_ConfigComponent.m_fPerceptionSafe + m_ConfigComponent.m_fPerceptionModifier);
					if (m_ConfigComponent.m_fPerceptionVigilant != 2.5)
						m_fPerceptionVigilant       = (m_ConfigComponent.m_fPerceptionVigilant + m_ConfigComponent.m_fPerceptionModifier);
					if (m_ConfigComponent.m_fPerceptionModifier != 0.0)
						m_fPerceptionModifier       = m_ConfigComponent.m_fPerceptionModifier;
					
					if (m_ConfigComponent.m_fAimAccuracyErrorModifier != 0)
						m_fAimAccuracyErrorModifier = m_ConfigComponent.m_fAimAccuracyErrorModifier;
				}
			}
		}
		//------------------------------------------------------------------------------------------------
		// ///////////////////////////////////////////////////////////////////////////////////////////////
		
		// if (false)
		
		if (m_CombatComponent) // if (initialize && m_bInitialize && m_CombatComponent)
		{
			SCR_AIUtilityComponent utilityComponent = m_ChimeraAIAgent.m_UtilityComponent;
			
			EAISkill skill = CRX_AIHierarchy.SetRank(m_Entity, m_iAgentIndex, m_iAgentsCount, utilityComponent, m_CombatComponent);
			
			if (m_eRankType == CRX_EAIRankType.VANILLA)
			{
				m_fAimAccuracyErrorOriginal = 1.0;
				
				m_eRank = CRX_ECharacterRank.PRIVATE;
			}
			else
				m_fAimAccuracyErrorOriginal = SCR_AIGetAimErrorOffset.GetAimError(skill);
			
			m_CombatComponent.SetAimingSkillBase(m_fAimAccuracyErrorOriginal);
		}
		
		if (m_CharacterControllerComponent)
		{
			if (m_bIsEditMode)
			{
				//------------------------------------------------------------------------------------------------
				m_fPerceptionSafe += m_fPerceptionModifier;
				m_fPerceptionVigilant += m_fPerceptionModifier;
				m_fPerceptionAlerted += m_fPerceptionModifier;
				m_fPerceptionThreatened += m_fPerceptionModifier;
			}
			
			if (m_fPerceptionSafe < 0)
				m_fPerceptionSafe = 0;
			else if (m_fPerceptionSafe > 3.0)
				m_fPerceptionSafe = 3.0;
			
			if (m_fPerceptionVigilant < 0)
				m_fPerceptionVigilant = 0;
			else if (m_fPerceptionVigilant > 5.0)
				m_fPerceptionVigilant = 5.0;
			
			if (m_fPerceptionAlerted < 0)
				m_fPerceptionAlerted = 0;
			else if (m_fPerceptionAlerted > 5.0)
				m_fPerceptionAlerted = 5.0;
			
			if (m_fPerceptionThreatened < 0)
				m_fPerceptionThreatened = 0;
			else if (m_fPerceptionThreatened > 5.0)
				m_fPerceptionThreatened = 5.0;
			
			#ifdef WORKBENCH
			// SCR_AIDebugVisualization.VisualizeMessage(m_Entity, "SCR_AIInfoComponent > PerceptionSafe" + " > " + m_fPerceptionSafe, EAIDebugCategory.NONE, 3, Color.White, 13, true);
			#endif
			
			//------------------------------------------------------------------------------------------------
			m_fAimAccuracyError = m_fAimAccuracyErrorOriginal;
			
			m_fAimAccuracyError += m_fAimAccuracyErrorModifier;
			
			if (m_fAimAccuracyError < 0)
				m_fAimAccuracyError = 0;
			else if (m_fAimAccuracyError > 3.0)
				m_fAimAccuracyError = 3.0;
			
			#ifdef WORKBENCH
			// SCR_AIDebugVisualization.VisualizeMessage(m_Entity, "SCR_AIInfoComponent > AimAccuracyError" + " > " + m_fAimAccuracyError, EAIDebugCategory.NONE, 3, Color.White, 13, true);
			#endif
			
			//------------------------------------------------------------------------------------------------
			if (m_eStance == ECharacterStance.AUTONOMOUS)
				m_bStanceChangeAutonomous = true;
			else
			{
				m_bStanceChangeAutonomous = false;
				
				ECharacterStanceChange stanceChange = m_eStance;
				
				stanceChange++;
				
				m_CharacterControllerComponent.SetStanceChange(stanceChange);
			}
			
			//------------------------------------------------------------------------------------------------
			if (m_eMovementType == CRX_EAIMovementType.AUTONOMOUS)
			{
				m_bMovementTypeAutonomous = true;
				
				m_eMovementType = CRX_EAIMovementType.RUN;
			}
			else
			{
				m_bMovementTypeAutonomous = false;
				
				AIBaseMovementComponent baseMovementComponent = m_ChimeraAIAgent.GetMovementComponent();
				
				if (baseMovementComponent)
				{
					AICharacterMovementComponent characterMovementComponent = AICharacterMovementComponent.Cast(baseMovementComponent);
					
					if (characterMovementComponent)
						characterMovementComponent.SetMovementTypeWanted(m_eMovementType);
				}
			}
			
			#ifdef WORKBENCH
			// SCR_AIDebugVisualization.VisualizeMessage(m_Entity, "SCR_AIInfoComponent > MovementType" + " > " + m_bMovementTypeAutonomous + " > " + SCR_Enum.GetEnumName(CRX_EAIMovementType, m_eMovementType), EAIDebugCategory.NONE, 3, Color.White, 13, true);
			#endif
			
			//------------------------------------------------------------------------------------------------
			if (m_eWeaponRaised == CRX_EAIWeaponRaised.RAISE)
				m_CharacterControllerComponent.SetWeaponRaised(true);
			else
				m_CharacterControllerComponent.SetWeaponRaised(false);
			
			//------------------------------------------------------------------------------------------------
			m_CharacterControllerComponent.SetDisableWeaponControls(m_bDisableWeaponControls);
			
			//------------------------------------------------------------------------------------------------
			m_CharacterControllerComponent.SetDisableMovementControls(m_bDisableMovementControls);
			
			//------------------------------------------------------------------------------------------------
			if (m_eFlashlightState == CRX_EAIFlashlightState.ON || m_eFlashlightState == CRX_EAIFlashlightState.OFF)
				CRX_AIUtilities.ToggleFlashlightState(m_eFlashlightState, m_GadgetManagerComponent);
			
			//------------------------------------------------------------------------------------------------
			if (m_Group)
			{
				AIAgent leaderAgent = m_Group.GetLeaderAgent();
				
				if (leaderAgent && leaderAgent == m_ChimeraAIAgent)
				{
					if (m_GroupUtilityComponent)
					{
						AIGroupMovementComponent groupMovementComponent = m_GroupUtilityComponent.GetGroupMovementComponent();
						
						if (groupMovementComponent)
						{
							if (m_eMovementType == CRX_EAIMovementType.AUTONOMOUS)
								groupMovementComponent.SetGroupCharactersWantedMovementType(EMovementType.RUN);
							else
								groupMovementComponent.SetGroupCharactersWantedMovementType(m_eMovementType);
						}
					}
				}
			}
			
			//------------------------------------------------------------------------------------------------
			switch (m_eSimulationState)
			{
				case CRX_EAISimulationState.ON:         m_ChimeraAIAgent.SetPermanentLOD(0);  break;
				case CRX_EAISimulationState.OFF:        m_ChimeraAIAgent.SetPermanentLOD(10); break;
				case CRX_EAISimulationState.AUTONOMOUS: m_ChimeraAIAgent.SetPermanentLOD(-1); break;
			}
		}
		
		#ifdef WORKBENCH
		// SCR_AIDebugVisualization.VisualizeMessage(m_Entity, "SCR_AIInfoComponent > CRX_AIInfoComponentInitializeDelayed", EAIDebugCategory.NONE, 3, Color.SpringGreen, 10, true);
		#endif
		
		m_bInitialize = false;
	}
	
//------------------------------------------------------------------------------------------------
//! Functionality
	
	//------------------------------------------------------------------------------------------------
	//! Fireteam
	SCR_AIGroupFireteam GetFireteam()
	{
		return m_Fireteam;
	}
	
	void SetFireteam(SCR_AIGroupFireteam fireteam)
	{
		m_Fireteam = fireteam;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Spawn Position Origin
	vector GetSpawnPositionOrigin()
	{
		return m_vSpawnPositionOrigin;
	}
	
	//------------------------------------------------------------------------------------------------
	EAISkill GetHierarchySkill()
	{
		return m_eHierarchySkill;
	}
	
	void SetHierarchySkill(EAISkill hierarchySkill)
	{
		m_eHierarchySkill = hierarchySkill;
	}
	
	//------------------------------------------------------------------------------------------------
	CRX_ECharacterRank GetHierarchyRank()
	{
		return m_eHierarchyRank;
	}
	
	void SetHierarchyRank(CRX_ECharacterRank hierarchyRank)
	{
		m_eHierarchyRank = hierarchyRank;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Rocket Launcher Suppression
	bool GetRocketLauncherSuppression()
	{
		return m_bRocketLauncherSuppression;
	}
	
	void SetRocketLauncherSuppression(bool rocketLauncherSuppression)
	{
		if (rocketLauncherSuppression && m_bRocketLauncherSuppression)
			return;
		
		m_bRocketLauncherSuppression = rocketLauncherSuppression;
		
		if (rocketLauncherSuppression)
		{
			float rocketLauncherSuppressionFallbackDelay = 7000.0;
			
			GetGame().GetCallqueue().CallLater(RocketLauncherSuppressionFallbackDelayed, rocketLauncherSuppressionFallbackDelay, false);
		}
	}
	
	void RocketLauncherSuppressionFallbackDelayed()
	{
		if (m_bRocketLauncherSuppression)
		{
			m_bRocketLauncherSuppression = false;
			
			#ifdef WORKBENCH
			// SCR_AIDebugVisualization.VisualizeMessage(m_Entity, "SCR_AIInfoComponent > RocketLauncherSuppressionFallbackDelayed > Fallback", EAIDebugCategory.NONE, 3, Color.Red, 13, true);
			#endif
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Is In Building
	bool GetIsInBuilding()
	{
		return m_bIsInBuilding;
	}
	
	void SetIsInBuilding(bool isInBuilding)
	{
		m_bIsInBuilding = isInBuilding;
	}
	
	IEntity GetBuildingEntity()
	{
		return m_BuildingEntity;
	}
	
	void SetBuildingEntity(IEntity buildingEntity)
	{
		m_BuildingEntity = buildingEntity;
	}
	
	float GetIsInBuildingTimeout()
	{
		return m_fIsInBuildingTimeout;
	}
	
	void SetIsInBuildingTimeout(float isInBuildingTimeout)
	{
		m_fIsInBuildingTimeout = isInBuildingTimeout;
	}
	
//! Functionality
//------------------------------------------------------------------------------------------------
	
//------------------------------------------------------------------------------------------------
//! Character
	
	//------------------------------------------------------------------------------------------------
	//! Rank
	CRX_EAIRank GetRank()
	{
		return m_eRank;
	}
	
	void SetRank(CRX_EAIRank rank)
	{
		m_eRank = rank;
	}
	
	CRX_EAIRankType GetRankType()
	{
		return m_eRankType;
	}
	
	void SetRankType(CRX_EAIRankType rankType)
	{
		m_eRankType = rankType;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Stance
	override ECharacterStance GetStance()
	{
		return m_eStance;
	}
	
	override void SetStance(ECharacterStance stance)
	{
		m_eStance = stance;
	}
	
	bool GetStanceChangeAutonomous()
	{
		return m_bStanceChangeAutonomous;
	}
	
	void SetStanceChangeAutonomous(bool stanceChangeAutonomous)
	{
		m_bStanceChangeAutonomous = stanceChangeAutonomous;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Rearm
	CRX_EAIRearmType GetRearmType()
	{
		return m_eRearmType;
	}
	
	void SetRearmType(CRX_EAIRearmType rearmType)
	{
		m_eRearmType = rearmType;
	}
	
	//! Perception
	float GetPerceptionSafe()
	{
		return m_fPerceptionSafe;
	}
	
	void SetPerceptionSafe(float perceptionSafe)
	{
		m_fPerceptionSafe = perceptionSafe;
	}
	
	float GetPerceptionVigilant()
	{
		return m_fPerceptionVigilant;
	}
	
	void SetPerceptionVigilant(float perceptionVigilant)
	{
		m_fPerceptionVigilant = perceptionVigilant;
	}
	
	float GetPerceptionAlerted()
	{
		return m_fPerceptionAlerted;
	}
	
	void SetPerceptionAlerted(float perceptionAlerted)
	{
		m_fPerceptionAlerted = perceptionAlerted;
	}
	
	float GetPerceptionThreatened()
	{
		return m_fPerceptionThreatened;
	}
	
	void SetPerceptionThreatened(float perceptionThreatened)
	{
		m_fPerceptionThreatened = perceptionThreatened;
	}
	
	float GetPerceptionModifier()
	{
		return m_fPerceptionModifier;
	}
	
	void SetPerceptionModifier(float perceptionModifier)
	{
		m_fPerceptionModifier = perceptionModifier;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Aim Accuracy Error
	float GetAimAccuracyError()
	{
		return m_fAimAccuracyError;
	}
	
	void SetAimAccuracyError(float aimAccuracyError)
	{
		m_fAimAccuracyError = aimAccuracyError;
	}
	
	float GetAimAccuracyErrorOriginal()
	{
		return m_fAimAccuracyErrorOriginal;
	}
	
	void SetAimAccuracyErrorOriginal(float aimAccuracyErrorOriginal)
	{
		m_fAimAccuracyErrorOriginal = aimAccuracyErrorOriginal;
		
		m_CombatComponent.SetAimingSkillBase(aimAccuracyErrorOriginal);
	}
	
	float GetAimAccuracyErrorModifier()
	{
		return m_fAimAccuracyErrorModifier;
	}
	
	void SetAimAccuracyErrorModifier(float aimAccuracyErrorModifier)
	{
		m_fAimAccuracyErrorModifier = aimAccuracyErrorModifier;
	}
	
//! Character
//------------------------------------------------------------------------------------------------
	
//------------------------------------------------------------------------------------------------
//! Character Utilities
	
	//------------------------------------------------------------------------------------------------
	//! Flee Chance
	int GetFleeChance()
	{
		return m_iFleeChance;
	}
	
	void SetFleeChance(int fleeChance)
	{
		m_iFleeChance = fleeChance;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Hold Position
	bool IsHoldPosition()
	{
		return m_bHoldPosition;
	}
	
	bool GetHoldPosition()
	{
		return m_bHoldPosition;
	}
	
	void SetHoldPosition(bool holdPosition)
	{
		m_bHoldPosition = holdPosition;
	}
	
	int GetHoldPositionRadius()
	{
		return m_iHoldPositionRadius;
	}
	
	void SetHoldPositionRadius(int holdPositionRadius)
	{
		m_iHoldPositionRadius = holdPositionRadius;
	}
	
	vector GetHoldPositionOrigin()
	{
		return m_vHoldPositionOrigin;
	}
	
	void SetHoldPositionOrigin(vector holdPositionOrigin)
	{
		m_vHoldPositionOrigin = holdPositionOrigin;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Idle Observe
	bool GetIdleObserve()
	{
		return m_bIdleObserve;
	}
	
	void SetIdleObserve(bool idleObserve)
	{
		m_bIdleObserve = idleObserve;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Weapon Fire Mode
	CRX_EAIWeaponFireMode GetWeaponFireMode()
	{
		return m_eWeaponFireMode;
	}
	
	void SetWeaponFireMode(CRX_EAIWeaponFireMode weaponFireMode)
	{
		m_eWeaponFireMode = weaponFireMode;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Force Stay In Vehicle
	bool GetForceStayInVehicle()
	{
		return m_bForceStayInVehicle;
	}
	
	void SetForceStayInVehicle(bool forceStayInVehicle)
	{
		m_bForceStayInVehicle = forceStayInVehicle;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Flashlight State
	CRX_EAIFlashlightState GetFlashlightState()
	{
		return m_eFlashlightState;
	}
	
	void SetFlashlightState(CRX_EAIFlashlightState flashlightState)
	{
		m_eFlashlightState = flashlightState;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Simulation State
	CRX_EAISimulationState GetSimulationState()
	{
		return m_eSimulationState;
	}
	
	void SetSimulationState(CRX_EAISimulationState simulationState)
	{
		m_eSimulationState = simulationState;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Danger Reaction Chance
	int GetDangerReactionChance()
	{
		return m_iDangerReactionChance;
	}
	
	void SetDangerReactionChance(int dangerReactionChance)
	{
		m_iDangerReactionChance = dangerReactionChance;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Disable Weapon Controls
	bool GetDisableWeaponControls()
	{
		return m_bDisableWeaponControls;
	}
	
	void SetDisableWeaponControls(bool disableWeaponControls)
	{
		m_bDisableWeaponControls = disableWeaponControls;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Disable Movement Controls
	bool GetDisableMovementControls()
	{
		return m_bDisableMovementControls;
	}
	
	void SetDisableMovementControls(bool disableMovementControls)
	{
		m_bDisableMovementControls = disableMovementControls;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Attack Reaction Delay Modifier
	float GetAttackReactionDelayModifier()
	{
		return m_fAttackReactionDelayModifier;
	}
	
	void SetAttackReactionDelayModifier(float attackReactionDelayModifier)
	{
		m_fAttackReactionDelayModifier = attackReactionDelayModifier;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Magazine Consumption Chance
	int GetMagazineConsumptionChance()
	{
		return m_iMagazineConsumptionChance;
	}
	
	void SetMagazineConsumptionChance(int magazineConsumptionChance)
	{
		m_iMagazineConsumptionChance = magazineConsumptionChance;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Weapon Raised
	CRX_EAIWeaponRaised GetWeaponRaised()
	{
		return m_eWeaponRaised;
	}
	
	void SetWeaponRaised(CRX_EAIWeaponRaised weaponRaised)
	{
		m_eWeaponRaised = weaponRaised;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Current Item In Hands Entity
	IEntity GetCurrentItemInHandsEntity()
	{
		return m_CurrentItemInHandsEntity;
	}
	
	void SetCurrentItemInHandsEntity(IEntity currentItemInHandsEntity)
	{
		m_CurrentItemInHandsEntity = currentItemInHandsEntity;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Movement Type
	override CRX_EAIMovementType GetMovementType()
	{
		return m_eMovementType;
	}
	
	override void SetMovementType(EMovementType mode)
	{
		CRX_EAIMovementType movementType = mode;
		
		m_eMovementType = movementType;
		
		if (movementType == CRX_EAIMovementType.AUTONOMOUS)
			return;
		
		#ifdef WORKBENCH
		// SCR_AIDebugVisualization.VisualizeMessage(m_Entity, "SCR_AIInfoComponent > SetMovementTypeState" + " > " + SCR_Enum.GetEnumName(EMovementType, GetMovementType()), EAIDebugCategory.NONE, 3, Color.Orange, 10, true);
		#endif
	}
	
	bool GetMovementTypeAutonomous()
	{
		return m_bMovementTypeAutonomous;
	}
	
	void SetMovementTypeAutonomous(bool movementTypeAutonomous)
	{
		m_bMovementTypeAutonomous = movementTypeAutonomous;
	}
	
//! Character Utilities
//------------------------------------------------------------------------------------------------
	
}