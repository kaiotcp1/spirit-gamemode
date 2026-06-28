class SPT_ShowCityNamesAction : ScriptedUserAction
{
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		BaseGameMode gameMode = GetGame().GetGameMode();
		if (!gameMode)
			return;

		SPT_CityDebugComponent cityDebug = SPT_CityDebugComponent.Cast(
			gameMode.FindComponent(SPT_CityDebugComponent)
		);
		if (!cityDebug)
			return;

		cityDebug.ShowAllCityNames();
	}

	override bool GetActionNameScript(out string outName)
	{
		outName = "Show City Names";
		return true;
	}

	override bool CanBeShownScript(IEntity user)
	{
		return true;
	}

	override bool CanBePerformedScript(IEntity user)
	{
		return true;
	}

	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}
}
