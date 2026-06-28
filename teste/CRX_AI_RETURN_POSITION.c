  /* ------------------------------------------------------------------------------------------------ /
 / CRX-EAI Class ( #CRX-EAIClass )                                                                   /
/ ------------------------------------------------------------------------------------------------ */

//------------------------------------------------------------------------------------------------
class CRX_AIReturnToFormation : DecoratorScripted
{
	protected static const string PORT_OUT_RETURN_TO_FORMATION_ORIGIN = "ReturnToFormationOriginOut";
	
	//------------------------------------------------------------------------------------------------
	protected AIGroup m_Group;
	protected IEntity m_AgentEntity;
	protected SCR_ChimeraAIAgent m_ChimeraAIAgent;
	
	protected SCR_AIInfoComponent m_InfoComponent;
	protected AIFormationComponent m_FormationComponent;
	protected SCR_AIGroupInfoComponent m_GroupInfoComponent;
	
	//------------------------------------------------------------------------------------------------
	override protected void OnInit(AIAgent owner)
	{
		m_Group = owner.GetParentGroup();
		
		m_AgentEntity = owner.GetControlledEntity();
		
		m_ChimeraAIAgent = SCR_ChimeraAIAgent.Cast(owner);
		
		if (m_ChimeraAIAgent)
			m_InfoComponent = m_ChimeraAIAgent.m_InfoComponent;
		else
			m_InfoComponent = SCR_AIInfoComponent.Cast(owner.FindComponent(SCR_AIInfoComponent));
		
		if (m_Group)
		{
			m_FormationComponent = m_Group.GetFormationComponent();
			
			// m_FormationComponent = AIFormationComponent.Cast(m_Group.FindComponent(AIFormationComponent));
			
			m_GroupInfoComponent = SCR_AIGroupInfoComponent.Cast(m_Group.FindComponent(SCR_AIGroupInfoComponent));
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected override bool TestFunction(AIAgent owner)
	{
		if (m_InfoComponent)
		{
			bool returnToFormation;
			
			// returnToFormation = m_InfoComponent.GetReturnToFormation();
			
			vector returnToFormationOrigin = SCR_AICombatMoveUtils.MovementTypePosition(m_Group, m_AgentEntity, m_ChimeraAIAgent, m_FormationComponent, m_GroupInfoComponent);
			
			if (returnToFormationOrigin == vector.Zero)
			{
				returnToFormationOrigin = m_AgentEntity.GetOrigin();
				
				#ifdef WORKBENCH
				// SCR_AIDebugVisualization.VisualizeMessage(owner.GetControlledEntity(), "CRX_AIReturnToFormation > TestFunction > Leader", EAIDebugCategory.NONE, 3, Color.Orange, 10, true);
				#endif
				
				if (m_Group)
				{
//					IEntity leaderEntity = m_Group.GetLeaderEntity();
//					
//					if (leaderEntity)
//						returnToFormationOrigin = leaderEntity.GetOrigin();
				}
			}
			
			vector moveIndividuallyOrigin;
			
			// moveIndividuallyOrigin = m_InfoComponent.GetMoveIndividuallyOrigin();
			
			if (moveIndividuallyOrigin == vector.Zero)
			{
				
			}
			else
			{
				returnToFormation = true;
				
				returnToFormationOrigin = moveIndividuallyOrigin;
			}
			
			SetVariableOut(PORT_OUT_RETURN_TO_FORMATION_ORIGIN, returnToFormationOrigin);
			
			if (returnToFormation)
			{
				#ifdef WORKBENCH
				// SCR_AIDebugVisualization.VisualizeMessage(owner.GetControlledEntity(), "CRX_AIReturnToFormation > TestFunction > true", EAIDebugCategory.NONE, 3, Color.Orange, 10, true);
				#endif
				
				return true;
			}
			else
			{
				#ifdef WORKBENCH
				// SCR_AIDebugVisualization.VisualizeMessage(owner.GetControlledEntity(), "CRX_AIReturnToFormation > TestFunction > false", EAIDebugCategory.NONE, 3, Color.Orange, 10, true);
				#endif
				
				return false;
			}
		}
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	protected static ref TStringArray s_aVarsOut = {
		PORT_OUT_RETURN_TO_FORMATION_ORIGIN
	};
	
	override TStringArray GetVariablesOut()
    {
        return s_aVarsOut;
    }
	
	//------------------------------------------------------------------------------------------------
	static override bool VisibleInPalette()
    {
        return true;
    }
	
	//------------------------------------------------------------------------------------------------
	protected static override string GetOnHoverDescription()
	{
		return "CRX_AIReturnToFormation.c: Returns true if return to formation.";
	}
}