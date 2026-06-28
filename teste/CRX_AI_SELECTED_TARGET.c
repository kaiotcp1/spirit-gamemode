  /* ------------------------------------------------------------------------------------------------ /
 / CRX-EAI Class ( #CRX-EAIClass )                                                                   /
/ ------------------------------------------------------------------------------------------------ */

//------------------------------------------------------------------------------------------------
class CRX_AISelectedTarget : DecoratorScripted
{
	protected static const string PORT_OUT_BASE_TARGET = "BaseTargetOut";
	protected static const string PORT_OUT_TIME_SINCE_SEEN = "TimeSinceSeenOut";
	
	//------------------------------------------------------------------------------------------------
	protected IEntity m_Entity;
	
	//------------------------------------------------------------------------------------------------
	protected SCR_AICombatComponent m_CombatComponent;
	protected SCR_AIUtilityComponent m_UtilityComponent;
	protected AIBaseUtilityComponent m_BaseUtilityComponent;
	
	//------------------------------------------------------------------------------------------------
	override protected void OnInit(AIAgent owner)
	{
		m_Entity = owner.GetControlledEntity();
		
		m_UtilityComponent = SCR_AIUtilityComponent.Cast(owner.FindComponent(SCR_AIUtilityComponent));
		
		if (m_Entity)
			m_CombatComponent = SCR_AICombatComponent.Cast(m_Entity.FindComponent(SCR_AICombatComponent));
		
		m_BaseUtilityComponent = AIBaseUtilityComponent.Cast(owner.FindComponent(AIBaseUtilityComponent));
	}
	
	//------------------------------------------------------------------------------------------------
	protected override bool TestFunction(AIAgent owner)
	{
		float timeSinceSeen;
		
		bool selectedTarget;
		
		BaseTarget currentTarget = m_CombatComponent.GetCurrentTarget();
		
		if (currentTarget)
		{
			timeSinceSeen = currentTarget.GetTimeSinceSeen();
			
			if (timeSinceSeen > 3.0)
				selectedTarget = false;
			else
			{
				int selectedMuzzleId;
				
				BaseWeaponComponent selectedWeaponComponent;
				
				m_CombatComponent.GetSelectedWeapon(selectedWeaponComponent, selectedMuzzleId);
				
				if (selectedWeaponComponent)
					selectedTarget = true;
				else
				{
					currentTarget = null;
					
					selectedTarget = false;
				}
			}
		}
		
		SetVariableOut(PORT_OUT_BASE_TARGET, currentTarget);
		
		SetVariableOut(PORT_OUT_TIME_SINCE_SEEN, timeSinceSeen);
		
		return selectedTarget;
	}
	
	//------------------------------------------------------------------------------------------------
	protected static ref TStringArray s_aVarsOut =
	{
		PORT_OUT_BASE_TARGET,
		PORT_OUT_TIME_SINCE_SEEN
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
		return "CRX_AISelectedTarget: Returns true if target was selected.";
	}
}