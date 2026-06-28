class SPT_MineDefusalUIContext : SPT_UIContext
{
	protected SPT_MineDefusalComponent m_MineDefComp;
	protected RplId m_MineRplId;
	protected bool m_bResultReceived;
	protected bool m_bTimerRunning;
	protected float m_fLocalTimer;
	protected float m_fTimerInterval;
	protected bool m_bPreviousMovementDisabled;
	protected bool m_bPreviousViewDisabled;
	protected bool m_bPreviousWeaponDisabled;
	protected bool m_bControlsLocked;

	protected ref array<string> m_aWireColors;

	void SPT_MineDefusalUIContext()
	{
		m_aWireColors = new array<string>();
		m_aWireColors.Insert("RED");
		m_aWireColors.Insert("BLUE");
		m_aWireColors.Insert("GREEN");
		m_aWireColors.Insert("YELLOW");
		m_aWireColors.Insert("BLACK");
	}

	void SetMine(SPT_MineDefusalComponent defComp, RplId mineRplId)
	{
		m_MineDefComp = defComp;
		m_MineRplId = mineRplId;

		if (m_MineDefComp)
		{
			m_MineDefComp.m_OnDefusalResult.Insert(OnDefusalResult);
		}

		ShowLayout();
		OnDefusalStarted();
	}

	void OnDefusalStarted()
	{
		m_bTimerRunning = true;
		m_fLocalTimer = m_MineDefComp.m_fTimerSeconds;
		m_fTimerInterval = 0;
	}

	void OnDefusalResult(bool success)
	{
		if (m_bResultReceived)
			return;

		m_bResultReceived = true;
		m_bTimerRunning = false;
		UnlockPlayerControls();

		string resultText;
		if (success)
		{
			resultText = "MINE DEFUSED";
			ShowHint("Mine successfully defused");
		}
		else
		{
			resultText = "MINE DETONATED";
			ShowHint("Mine detonated");
		}

		UpdateResultDisplay(resultText, success);
		DisableWireButtons();

		GetGame().GetCallqueue().CallLater(CloseLayoutDelayed, 2500, false);
	}

	void CloseLayoutDelayed()
	{
		CloseLayout();
	}

	override protected void OnShow()
	{
		if (!m_wRoot)
			return;

		m_bResultReceived = false;
		m_bTimerRunning = false;
		m_fLocalTimer = 0;

		SetupWireButtons();
		SetupDisplayText();
		LockPlayerControls();
		RegisterButtonHandlers();

		GetGame().GetCallqueue().CallLater(UpdateStandaloneContext, 100, true);
	}

	protected void UpdateStandaloneContext()
	{
		if (!m_bIsActive)
			return;

		CheckDirectKeyboardInput();
		EOnFrame(m_Owner, 0.1);
	}

	protected void RegisterButtonHandlers()
	{
		Widget button = m_wRoot.FindAnyWidget("WireButton_0");
		if (button)
			ButtonActionComponent.GetOnAction(button, true).Insert(OnWire0);

		button = m_wRoot.FindAnyWidget("WireButton_1");
		if (button)
			ButtonActionComponent.GetOnAction(button, true).Insert(OnWire1);

		button = m_wRoot.FindAnyWidget("WireButton_2");
		if (button)
			ButtonActionComponent.GetOnAction(button, true).Insert(OnWire2);

		button = m_wRoot.FindAnyWidget("WireButton_3");
		if (button)
			ButtonActionComponent.GetOnAction(button, true).Insert(OnWire3);

		button = m_wRoot.FindAnyWidget("WireButton_4");
		if (button)
			ButtonActionComponent.GetOnAction(button, true).Insert(OnWire4);

		button = m_wRoot.FindAnyWidget("BackButton");
		if (button)
			ButtonActionComponent.GetOnAction(button, true).Insert(OnBackClicked);
	}

	protected void UnregisterButtonHandlers()
	{
		if (!m_wRoot)
			return;

		Widget button = m_wRoot.FindAnyWidget("WireButton_0");
		if (button)
			ButtonActionComponent.GetOnAction(button).Remove(OnWire0);

		button = m_wRoot.FindAnyWidget("WireButton_1");
		if (button)
			ButtonActionComponent.GetOnAction(button).Remove(OnWire1);

		button = m_wRoot.FindAnyWidget("WireButton_2");
		if (button)
			ButtonActionComponent.GetOnAction(button).Remove(OnWire2);

		button = m_wRoot.FindAnyWidget("WireButton_3");
		if (button)
			ButtonActionComponent.GetOnAction(button).Remove(OnWire3);

		button = m_wRoot.FindAnyWidget("WireButton_4");
		if (button)
			ButtonActionComponent.GetOnAction(button).Remove(OnWire4);

		button = m_wRoot.FindAnyWidget("BackButton");
		if (button)
			ButtonActionComponent.GetOnAction(button).Remove(OnBackClicked);
	}

	protected void CheckDirectKeyboardInput()
	{
		if (Debug.KeyState(KeyCode.KC_1))
		{
			Debug.ClearKey(KeyCode.KC_1);
			SelectWire(0);
			return;
		}

		if (Debug.KeyState(KeyCode.KC_2))
		{
			Debug.ClearKey(KeyCode.KC_2);
			SelectWire(1);
			return;
		}

		if (Debug.KeyState(KeyCode.KC_3))
		{
			Debug.ClearKey(KeyCode.KC_3);
			SelectWire(2);
			return;
		}

		if (Debug.KeyState(KeyCode.KC_4))
		{
			Debug.ClearKey(KeyCode.KC_4);
			SelectWire(3);
			return;
		}

		if (Debug.KeyState(KeyCode.KC_5))
		{
			Debug.ClearKey(KeyCode.KC_5);
			SelectWire(4);
			return;
		}

		if (Debug.KeyState(KeyCode.KC_ESCAPE))
		{
			Debug.ClearKey(KeyCode.KC_ESCAPE);
			OnBackClicked();
		}
	}

	override protected void OnActiveFrame(float timeSlice)
	{
		if (!m_bIsActive)
			return;

		if (m_bTimerRunning && m_MineDefComp)
		{
			m_fLocalTimer = m_MineDefComp.GetRemainingTime();
			if (m_fLocalTimer < 0)
				m_fLocalTimer = 0;

			UpdateTimerDisplay();
		}
	}

	override protected void OnClose()
	{
		GetGame().GetCallqueue().Remove(UpdateStandaloneContext);
		UnregisterButtonHandlers();
		UnlockPlayerControls();
		Cleanup();
	}

	protected void LockPlayerControls()
	{
		if (!m_Controller || m_bControlsLocked)
			return;

		m_bPreviousMovementDisabled = m_Controller.GetDisableMovementControls();
		m_bPreviousViewDisabled = m_Controller.GetDisableViewControls();
		m_bPreviousWeaponDisabled = m_Controller.GetDisableWeaponControls();

		m_Controller.SetDisableMovementControls(true);
		m_Controller.SetDisableViewControls(true);
		m_Controller.SetDisableWeaponControls(true);
		m_bControlsLocked = true;
	}

	protected void UnlockPlayerControls()
	{
		if (!m_Controller || !m_bControlsLocked)
			return;

		m_Controller.SetDisableMovementControls(m_bPreviousMovementDisabled);
		m_Controller.SetDisableViewControls(m_bPreviousViewDisabled);
		m_Controller.SetDisableWeaponControls(m_bPreviousWeaponDisabled);
		m_bControlsLocked = false;
	}

	void SetupWireButtons()
	{
		if (!m_MineDefComp)
			return;

		int wireCount = m_MineDefComp.GetWireCount();

		for (int i = 0; i < 5; i++)
		{
			Widget wireBtn = m_wRoot.FindAnyWidget("WireButton_" + i);
			if (wireBtn)
			{
				wireBtn.SetVisible(i < wireCount);
			}

			TextWidget wireText = TextWidget.Cast(m_wRoot.FindAnyWidget("WireText_" + i));
			if (wireText)
			{
				string colorName;
				if (i < m_aWireColors.Count())
					colorName = string.Format("[%1]  %2 WIRE", i + 1, m_aWireColors[i]);
				else
					colorName = "WIRE " + (i + 1);

				wireText.SetText(colorName);
				wireText.SetVisible(i < wireCount);
			}
		}
	}

	void SetupDisplayText()
	{
		if (!m_MineDefComp)
			return;

		TextWidget typeText = TextWidget.Cast(m_wRoot.FindAnyWidget("TypeText"));
		if (typeText)
			typeText.SetText("Type: " + m_MineDefComp.GetMineType());

		TextWidget toolText = TextWidget.Cast(m_wRoot.FindAnyWidget("ToolText"));
		if (toolText)
		{
			if (m_MineDefComp.RequiresTool())
				toolText.SetText("Requires: EOD Toolkit");
			else
				toolText.SetText("No tool required");
		}

		m_fLocalTimer = m_MineDefComp.GetRemainingTime();
		UpdateTimerDisplay();
	}

	void UpdateTimerDisplay()
	{
		TextWidget timerText = TextWidget.Cast(m_wRoot.FindAnyWidget("TimerText"));
		if (timerText)
		{
			int seconds = Math.Round(m_fLocalTimer);
			timerText.SetText(string.Format("Time: %1s", seconds));
		}

		ProgressBarWidget timerBar = ProgressBarWidget.Cast(m_wRoot.FindAnyWidget("TimerBar"));
		if (timerBar && m_MineDefComp)
		{
			float totalTime = m_MineDefComp.m_fTimerSeconds;
			float progress;
			if (totalTime > 0)
				progress = (m_fLocalTimer / totalTime) * 100.0;
			else
				progress = 0;
			timerBar.SetCurrent(progress);
		}
	}

	void UpdateResultDisplay(string text, bool success)
	{
		TextWidget typeText = TextWidget.Cast(m_wRoot.FindAnyWidget("TypeText"));
		if (typeText)
			typeText.SetText(text);

		if (!success)
		{
			if (typeText)
				typeText.SetColor(Color.Red);
		}
	}

	void DisableWireButtons()
	{
		for (int i = 0; i < 5; i++)
		{
			Widget wireBtn = m_wRoot.FindAnyWidget("WireButton_" + i);
			if (wireBtn)
				wireBtn.SetVisible(false);
		}
	}

	void SelectWire(int wireIndex)
	{
		if (m_bResultReceived)
			return;

		if (!m_MineDefComp)
			return;

		DisableWireButtons();
		m_MineDefComp.RequestSelectWire(wireIndex);
	}

	void OnWire0()
	{
		SelectWire(0);
	}

	void OnWire1()
	{
		SelectWire(1);
	}

	void OnWire2()
	{
		SelectWire(2);
	}

	void OnWire3()
	{
		SelectWire(3);
	}

	void OnWire4()
	{
		SelectWire(4);
	}

	void OnBackClicked()
	{
		if (!m_bResultReceived && m_MineDefComp)
			m_MineDefComp.RequestCancelDefusal();

		CloseLayout();
	}

	void Cleanup()
	{
		if (m_MineDefComp)
		{
			m_MineDefComp.m_OnDefusalResult.Remove(OnDefusalResult);
		}

		m_MineDefComp = null;
		m_bResultReceived = false;
		m_bTimerRunning = false;
	}
}
