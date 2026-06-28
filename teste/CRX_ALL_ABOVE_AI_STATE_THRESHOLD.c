  /* ------------------------------------------------------------------------------------------------ /
 / CRX-EAI Class ( #CRX-EAIClass )                                                                   /
/ ------------------------------------------------------------------------------------------------ */

enum CRX_EAIThreatState
{
	TOTAL,
	INJURY,
	SHOTSFIRED,
	SUPPRESSION
}

//------------------------------------------------------------------------------------------------
class CRX_AIIsAboveThreatStateThreshold : DecoratorScripted
{
	[Attribute(defvalue: "0", uiwidget: UIWidgets.ComboBox, desc: "Select threat state to check.", params: "", enums: ParamEnumArray.FromEnum(CRX_EAIThreatState))]
	protected CRX_EAIThreatState m_eThreatState;
	
	[Attribute(defvalue: "0", uiwidget: UIWidgets.EditBox, desc: "Select threat state threshold.", params: "0 1 0.001")]
	protected float m_fThreatStateThreshold;
	
	protected static const string PORT_OUT_THREAT_STATE = "ThreatStateOut";
	
	//------------------------------------------------------------------------------------------------
	protected SCR_AIUtilityComponent m_UtilityComponent;
	
	//------------------------------------------------------------------------------------------------
	override protected void OnInit(AIAgent owner)
	{
		m_UtilityComponent = SCR_AIUtilityComponent.Cast(owner.FindComponent(SCR_AIUtilityComponent));
	}
	
	//------------------------------------------------------------------------------------------------
	protected override bool TestFunction(AIAgent owner)
	{
		float threatState;
		
		switch (m_eThreatState)
		{
			case CRX_EAIThreatState.TOTAL:
			{
				threatState = m_UtilityComponent.m_ThreatSystem.GetThreatTotal(); break;
			}
			case CRX_EAIThreatState.INJURY:
			{
				threatState = m_UtilityComponent.m_ThreatSystem.GetThreatInjury(); break;
			}
			case CRX_EAIThreatState.SHOTSFIRED:
			{
				threatState = m_UtilityComponent.m_ThreatSystem.GetThreatShotsFired(); break;
			}
			case CRX_EAIThreatState.SUPPRESSION:
			{
				threatState = m_UtilityComponent.m_ThreatSystem.GetThreatSuppression(); break;
			}
		}
		
		SetVariableOut(PORT_OUT_THREAT_STATE, threatState);
		
		if (threatState > m_fThreatStateThreshold)
			return true;
		
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	protected static ref TStringArray s_aVarsOut =
	{
		PORT_OUT_THREAT_STATE
	};
	
	override TStringArray GetVariablesOut()
	{
		return s_aVarsOut;
	}
	
	//------------------------------------------------------------------------------------------------
	static override string GetOnHoverDescription()
	{
		return "CRX_AIIsAboveThreatStateThreshold.c: Returns true if selected threat state is above selected threshold.";
	}
	
	//------------------------------------------------------------------------------------------------
	override string GetNodeMiddleText()
	{
		return "Threat State: " + typename.EnumToString(CRX_EAIThreatState, m_eThreatState) + "\n" + "Threshold: " + m_fThreatStateThreshold;
	}
}