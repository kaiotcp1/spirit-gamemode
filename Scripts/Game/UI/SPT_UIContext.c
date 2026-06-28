class SPT_UIContext : ScriptAndConfig
{
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Layout to show", params: "layout")]
	ResourceName m_Layout;

	[Attribute(desc: "Input context name for this UI")]
	string m_sContextName;

	[Attribute("1", desc: "Hide HUD when this UI is active")]
	bool m_bHideHUDOnShow;

	protected IEntity m_Owner;
	protected bool m_bIsActive;
	protected SCR_CharacterControllerComponent m_Controller;
	protected InputManager m_InputManager;
	protected SPT_UIManagerComponent m_UIManager;
	protected Widget m_wRoot;
	protected string m_sPlayerID;
	protected int m_iPlayerID;

	void Init(IEntity owner, SPT_UIManagerComponent uimanager)
	{
		m_Owner = owner;
		m_InputManager = GetGame().GetInputManager();
		m_UIManager = uimanager;
		m_Controller = SCR_CharacterControllerComponent.Cast(owner.FindComponent(SCR_CharacterControllerComponent));
		PostInit();
	}

	bool IsActive()
	{
		return m_bIsActive;
	}

	void SetLayout(ResourceName layout)
	{
		m_Layout = layout;
	}

	void SetContextName(string contextName)
	{
		m_sContextName = contextName;
	}

	void PostInit()
	{
	}

	void OnControlledByPlayer()
	{
		int playerId = SCR_PossessingManagerComponent.GetPlayerIdFromControlledEntity(m_Owner);
		m_iPlayerID = playerId;
	}

	void RegisterInputs()
	{
	}

	void UnregisterInputs()
	{
	}

	void EOnFrame(IEntity owner, float timeSlice)
	{
		if (m_bIsActive)
		{
			if (m_sContextName != "")
				m_InputManager.ActivateContext(m_sContextName);

			if (m_bHideHUDOnShow)
			{
				SCR_HUDManagerComponent hud = GetGame().GetHUDManager();
				if (hud)
					hud.SetVisible(false);
			}

			OnActiveFrame(timeSlice);
		}
		OnFrame(timeSlice);
	}

	protected void OnActiveFrame(float timeSlice)
	{
	}

	protected void OnFrame(float timeSlice)
	{
	}

	bool CanShowLayout()
	{
		return true;
	}

	void ShowLayout()
	{
		if (!m_Layout)
			return;

		if (!CanShowLayout())
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		m_wRoot = workspace.CreateWidgets(m_Layout);

		Enable();
		OnShow();
	}

	void CloseLayout()
	{
		if (!m_wRoot)
			return;
		if (!m_bIsActive)
			return;

		m_wRoot.RemoveFromHierarchy();
		m_wRoot = null;

		if (m_bHideHUDOnShow)
		{
			SCR_HUDManagerComponent hud = GetGame().GetHUDManager();
			if (hud)
				hud.SetVisible(true);
		}

		Disable();
		OnClose();
	}

	protected void OnShow()
	{
	}

	protected void OnClose()
	{
	}

	void Enable()
	{
		m_bIsActive = true;
	}

	void Disable()
	{
		m_bIsActive = false;
	}

	void ShowHint(string text)
	{
		SCR_HintManagerComponent hintManager = SCR_HintManagerComponent.GetInstance();
		if (!hintManager)
			return;
		hintManager.ShowCustom(text);
	}
}
