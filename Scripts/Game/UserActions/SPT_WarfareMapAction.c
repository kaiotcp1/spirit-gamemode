//! Acao do jogador para abrir o mapa Warfare com icones coloridos.
//! Mostra todos os pontos estrategicos e seus estados atuais.
[UserAction]
class SPT_WarfareMapAction : ScriptedUserAction
{
	//-----------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pOwnerEntity)
			return;

		SPT_UIManagerComponent uiManager = SPT_UIManagerComponent.Cast(
			pOwnerEntity.FindComponent(SPT_UIManagerComponent));
		if (!uiManager)
			return;

		uiManager.ShowContext(SPT_WarfareMapUIContext);
	}

	//-----------------------------------------------------------------------
	override bool CanBeShown(IEntity pOwnerEntity)
	{
		SPT_WarfareGameModeComponent warfare = SPT_WarfareGameModeComponent.GetInstance();
		if (!warfare)
			return false;

		int total = warfare.GetTotalPointCount();
		return total > 0;
	}

	//-----------------------------------------------------------------------
	override bool CanBePerformed(IEntity pOwnerEntity)
	{
		return CanBeShown(pOwnerEntity);
	}

	//-----------------------------------------------------------------------
	override string GetText() { return "Mapa de Guerra"; }
}
