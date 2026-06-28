class SPT_TestAction : ScriptedUserAction
{
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		SCR_HintManagerComponent hint = SCR_HintManagerComponent.GetInstance();
		if (hint)
			hint.ShowCustom("[SPT] AcaoTeste funciona em " + pOwnerEntity.ClassName());
	}

	override bool GetActionNameScript(out string outName)
	{
		outName = "SPT Acao de Teste";
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
