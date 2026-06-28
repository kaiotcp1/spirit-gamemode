  /* ------------------------------------------------------------------------------------------------ /
 / CRX-EAI Class ( #CRX-EAIClass )                                                                   /
/ ------------------------------------------------------------------------------------------------ */

//------------------------------------------------------------------------------------------------
class CRX_AIHoldPosition : DecoratorScripted
{
	protected static const string PORT_OUT_HOLD_POSITION_ORIGIN = "HoldPositionOriginOut";
	
	//------------------------------------------------------------------------------------------------
	protected SCR_AIInfoComponent m_InfoComponent;
	
	//------------------------------------------------------------------------------------------------
	override void OnInit(AIAgent owner)
	{
		m_InfoComponent = SCR_AIInfoComponent.Cast(owner.FindComponent(SCR_AIInfoComponent));
	}
	
	//------------------------------------------------------------------------------------------------
	protected override bool TestFunction(AIAgent owner)
	{
		bool holdPosition = m_InfoComponent.GetHoldPosition();
		
		if (holdPosition)
		{
			vector holdPositionOrigin = m_InfoComponent.GetHoldPositionOrigin();
			
			SetVariableOut(PORT_OUT_HOLD_POSITION_ORIGIN, holdPositionOrigin);
			
			return true;
		}
		
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	protected static ref TStringArray s_aVarsOut = {
		PORT_OUT_HOLD_POSITION_ORIGIN
	};
	
	override TStringArray GetVariablesOut()
    {
        return s_aVarsOut;
    }
	
	//------------------------------------------------------------------------------------------------
	protected static override bool VisibleInPalette()
	{
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	protected static override string GetOnHoverDescription()
	{
		return "CRX_AIHoldPosition: Returns true if hold position is used.";
	}
}