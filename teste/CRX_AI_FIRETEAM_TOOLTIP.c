  /* ------------------------------------------------------------------------------------------------ /
 / CRX-EAI Base Class ( #CRX-EAIBaseClass )                                                          /
/ ------------------------------------------------------------------------------------------------ */

//------------------------------------------------------------------------------------------------
[BaseContainerProps(), BaseContainerCustomTitleField("m_sDisplayName")]
class CRX_AIFireteamTooltipDetail : SCR_EntityTooltipDetail
{
	protected TextWidget m_Text;
	
	protected int m_iFireteamTextColor[8] =
	{
		Color.RED,
		Color.GREEN,
		Color.BLUE,
		Color.CYAN,
		Color.MAGENTA,
		Color.DARK_YELLOW,
		Color.DODGER_BLUE,
		Color.VIOLET
	};
	
	protected string m_sFireteamColorId[8] =
	{
		"Red",
		"Green",
		"Blue",
		"Cyan",
		"Echo",
		"Foxtrott",
		"Golf",
		"Hotel"
	};
	
	protected SCR_ChimeraAIAgent m_ChimeraAIAgent;
	
	// protected SCR_AIInfoComponent m_InfoComponent;
	
	protected const int m_iFireteamColorIdCount = 8;
	
	protected const int m_iFireteamTextColorCount = 8;
	
	// protected SCR_AIGroupInfoComponent m_GroupInfoComponent;
	
	protected ref array<ref SCR_AIGroupFireteam> m_aFireteams;
	
	protected SCR_AIInfoComponent m_InfoComponent;
	
	protected SCR_AIGroupFireteam m_CurrentFireteam;
	
	protected SCR_AIGroupUtilityComponent m_GroupUtilityComponent;
	
	//------------------------------------------------------------------------------------------------
	override void UpdateDetail(SCR_EditableEntityComponent entity)
	{
		if (m_Text && m_InfoComponent)
		{
			if (m_InfoComponent.GetFireteam() != m_CurrentFireteam)
				SetAIFireteamTooltipDetailText(m_InfoComponent.GetFireteam());
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected void SetAIFireteamTooltipDetailText(SCR_AIGroupFireteam fireteam)
	{
		// return;
		
		if (true)
		
		// if (m_InfoComponent)
		{
			// SCR_AIGroupFireteam fireteam = m_ChimeraAIAgent.GetFireteam();
			
			// Print(fireteam);
			
			m_CurrentFireteam = fireteam;
			
			if (fireteam)
			{
				IEntity firstMemberEntity = fireteam.GetFirstMemberEntity();
				
				// Print("CRX_AIFireteamTooltipDetail > SetAIFireteamTooltipDetailText" > firstMemberEntity);
			}
			
			if (fireteam && m_GroupUtilityComponent)
			{
				SCR_AIGroupFireteamManager groupFireteamManager = m_GroupUtilityComponent.m_FireteamMgr;
				
				if (groupFireteamManager)
				{
					m_aFireteams = groupFireteamManager.GetFireteams();
					
					if (m_aFireteams.IsEmpty())
						return;
					
					int fireteamId = m_aFireteams.Find(fireteam);
					
					if (fireteamId == -1)
						return;
					else
					{
						if (fireteamId > 7)
							fireteamId = 7;
					}
					
					array<AIAgent> fireteamMembers = {};
					
					fireteam.GetMembers(fireteamMembers);
					
					int fireteamMembersCount = fireteamMembers.Count();
					
					int fireteamMemberId = fireteamMembers.Find(m_ChimeraAIAgent);
					
					fireteamMemberId++;
					
					string fireteamColorId = m_sFireteamColorId[fireteamId];
					
					if (fireteamMemberId == 1)
						fireteamColorId = (fireteamColorId + ": " + "Leader");
					else
						fireteamColorId = (fireteamColorId + ": " + "Member");
					
					int fireteamTextColor = m_iFireteamTextColor[fireteamId % m_iFireteamTextColorCount];
					
					string fireteamText = string.Format("%1: %2%3:%4", fireteamColorId, "#", fireteamMemberId, fireteamMembersCount);
					
					fireteamText = string.Format("%1: %2:%3", fireteamColorId, fireteamMemberId, fireteamMembersCount);
					
					m_Text.SetText(fireteamText);
					
					m_Text.SetColorInt(fireteamTextColor);
				}
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	override bool InitDetail(SCR_EditableEntityComponent entity, Widget widget)
	{
		if (entity.GetPlayerID() > 0)
			return false;
		
		// return false;
		
		m_Text = TextWidget.Cast(widget);
		
		if (m_Text)
		{
			SCR_AIGroupFireteam fireteam;
			
			SCR_EditableCharacterComponent editableCharacterComponent = SCR_EditableCharacterComponent.Cast(entity);
			
			if (editableCharacterComponent)
			{
				GenericEntity owner = editableCharacterComponent.GetOwner();
				
				SCR_EditableGroupComponent editableGroupComponent = SCR_EditableGroupComponent.Cast(entity.GetParentEntity());
				
				if (owner && editableGroupComponent)
				{
					AIAgent agent = CRX_AIEditableEntityType.GetAIAgent(entity);
					
					if (agent)
						m_ChimeraAIAgent = SCR_ChimeraAIAgent.Cast(agent);
					
//					SCR_ChimeraCharacter chimeraCharacter = SCR_ChimeraCharacter.Cast(owner);
//					
//					if (chimeraCharacter)
//					{
//						fireteam = m_ChimeraAIAgent.GetFireteam();
//						
//						// fireteam = chimeraCharacter.GetFireteam();
//						
//						// Print("CRX_AIFireteamTooltipDetail > InitDetail > SCR_ChimeraCharacter" + " > " + fireteam);
//					}
					
					// m_InfoComponent = editableCharacterComponent.GetAIInfoComponent();
					
					if (m_ChimeraAIAgent)
					
					// if (m_InfoComponent)
					{
						fireteam = m_ChimeraAIAgent.GetFireteam();
						
						// fireteam = m_InfoComponent.GetFireteam();
						
						SCR_EditableGroupComponent editableEntityComponent = SCR_EditableGroupComponent.Cast(entity.GetParentEntity());
						
						if (editableEntityComponent)
						{
							owner = editableEntityComponent.GetOwner();
							
							if (owner)
							{
								SCR_AIGroup group = SCR_AIGroup.Cast(owner);
								
								if (group)
								{
									m_GroupUtilityComponent = SCR_AIGroupUtilityComponent.Cast(group.FindComponent(SCR_AIGroupUtilityComponent));
									
									if (m_GroupUtilityComponent)
									{
										SCR_AIGroupFireteamManager groupFireteamManager = m_GroupUtilityComponent.m_FireteamMgr;
										
										if (groupFireteamManager)
										{
											m_aFireteams = groupFireteamManager.GetFireteams();
											
											// Print("CRX_AIFireteamTooltipDetail > InitDetail" + " > " + fireteam + " > " + m_aFireteams);
										}
									}
								}
							}
						}
						
						SetAIFireteamTooltipDetailText(fireteam);
						
						return true;
					}
				}
				
				return false;
			}
			
			AIAgent agent = CRX_AIEditableEntityType.GetAIAgent(entity);
			
			if (agent)
			{
				m_ChimeraAIAgent = SCR_ChimeraAIAgent.Cast(agent);
				
				if (m_ChimeraAIAgent)
				{
					// m_InfoComponent = m_ChimeraAIAgent.m_InfoComponent;
					
					m_GroupUtilityComponent = m_ChimeraAIAgent.m_GroupUtilityComponent;
					
					if (m_GroupUtilityComponent)
					{
						SetAIFireteamTooltipDetailText(fireteam);
						
						return true;
					}
				}
			}
			
			SCR_EditableGroupComponent group = SCR_EditableGroupComponent.Cast(entity);
			
			if (group)
			{
				m_GroupUtilityComponent = SCR_AIGroupUtilityComponent.Cast(group.GetOwner().FindComponent(SCR_AIGroupUtilityComponent));
				
				if (m_GroupUtilityComponent)
				{
					// Print("Client" + " > " + m_GroupUtilityComponent);
					
					SetAIFireteamTooltipDetailText(fireteam);
					
					return true;
				}
			}
			else
			{
				if (entity.GetPlayerID() > 0)
					return false;
				
				group = SCR_EditableGroupComponent.Cast(entity.GetParentEntity());
				
				if (group)
				{
					m_GroupUtilityComponent = SCR_AIGroupUtilityComponent.Cast(group.GetOwner().FindComponent(SCR_AIGroupUtilityComponent));
					
					if (m_GroupUtilityComponent)
					{
						// Print("Client" + " > " + m_GroupUtilityComponent);
						
						SetAIFireteamTooltipDetailText(fireteam);
						
						return true;
					}
				}
			}
		}
		
		return false;
	}
}