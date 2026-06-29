//! Acao do jogador para abrir o mapa Warfare com icones coloridos.
//! Mostra todos os pontos estrategicos e seus estados atuais coloridos.
class SPT_WarfareMapAction : ScriptedUserAction
{
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		SCR_HintManagerComponent hintMgr = SCR_HintManagerComponent.GetInstance();
		if (hintMgr)
			hintMgr.ShowCustom("[SPT] Mapa de Guerra aberto!");

		SPT_UIManagerComponent uiManager = SPT_UIManagerComponent.Cast(pUserEntity.FindComponent(SPT_UIManagerComponent));
		SPT_WarfareMapUIContext ctx;

		if (uiManager)
		{
			ctx = SPT_WarfareMapUIContext.Cast(uiManager.GetContext(SPT_WarfareMapUIContext));
		}

		if (!ctx)
		{
			ctx = new SPT_WarfareMapUIContext();
			ctx.SetLayout("{D6F41B39A8C27E10}UI/Layouts/WarfareMap.layout");
			ctx.Init(pUserEntity, null);
		}

		ctx.ShowLayout();
	}

	override bool GetActionNameScript(out string outName)
	{
		outName = "Mapa de Guerra";
		return true;
	}

	override bool CanBeShownScript(IEntity user)
	{
		SPT_WarfareGameModeComponent warfare = SPT_WarfareGameModeComponent.GetInstance();
		if (!warfare)
			return false;

		int total = warfare.GetTotalPointCount();
		return total > 0;
	}

	override bool CanBePerformedScript(IEntity user)
	{
		return CanBeShownScript(user);
	}

	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}
}
