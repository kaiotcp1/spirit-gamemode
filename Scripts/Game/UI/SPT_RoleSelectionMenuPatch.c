//! Corrige a assinatura incompatível registrada pelo menu vanilla 1.7.
//! GetOnButtonFocused fornece apenas Faction, enquanto ShowFactionPlayerList
//! também declara um bool opcional. ScriptInvoker não aplica parâmetros default.
modded class SCR_RoleSelectionMenu
{
	override protected void HookEvents()
	{
		super.HookEvents();

		if (!m_FactionRequestUIHandler)
			return;

		m_FactionRequestUIHandler.GetOnButtonFocused().Remove(ShowFactionPlayerList);
		m_FactionRequestUIHandler.GetOnButtonFocused().Insert(SPT_ShowFactionPlayerList);
	}

	protected void SPT_ShowFactionPlayerList(Faction faction)
	{
		ShowFactionPlayerList(faction, true);
	}
}
