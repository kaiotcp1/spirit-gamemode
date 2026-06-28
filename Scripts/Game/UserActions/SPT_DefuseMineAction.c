class SPT_DefuseMineAction : ScriptedUserAction
{
	protected ref SPT_MineDefusalUIContext m_DefusalContext;

	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		SCR_HintManagerComponent hintMgr = SCR_HintManagerComponent.GetInstance();
		if (hintMgr)
			hintMgr.ShowCustom("[SPT] DefuseMineAction triggered on " + pOwnerEntity.ClassName());

		SPT_MineDefusalComponent defComp = SPT_MineDefusalComponent.Cast(pOwnerEntity.FindComponent(SPT_MineDefusalComponent));
		if (!defComp)
		{
			Print("[SPT_DefuseMineAction] No SPT_MineDefusalComponent found on entity");
			return;
		}

		SPT_UIManagerComponent uiManager = SPT_UIManagerComponent.Cast(pUserEntity.FindComponent(SPT_UIManagerComponent));
		SPT_MineDefusalUIContext defContext;

		if (uiManager)
		{
			defContext = SPT_MineDefusalUIContext.Cast(uiManager.GetContext(SPT_MineDefusalUIContext));
		}

		if (!defContext)
		{
			m_DefusalContext = new SPT_MineDefusalUIContext();
			m_DefusalContext.SetLayout("{D6F41B39A8C27E10}UI/Layouts/MineDefusal.layout");
			m_DefusalContext.Init(pUserEntity, null);
			defContext = m_DefusalContext;
		}

		RplComponent mineRpl = RplComponent.Cast(pOwnerEntity.FindComponent(RplComponent));
		if (!mineRpl)
		{
			Print("[SPT_DefuseMineAction] No RplComponent on entity");
			return;
		}

		defContext.SetMine(defComp, mineRpl.Id());

		int playerId = SCR_PossessingManagerComponent.GetPlayerIdFromControlledEntity(pUserEntity);
		defComp.RequestStartDefusal(playerId);
	}

	override bool GetActionNameScript(out string outName)
	{
		outName = "Defuse Mine";
		return true;
	}

	override bool CanBeShownScript(IEntity user)
	{
		IEntity owner = GetOwner();
		if (!owner)
			return false;

		SPT_MineDefusalComponent defComp = SPT_MineDefusalComponent.Cast(
			owner.FindComponent(SPT_MineDefusalComponent)
		);
		return defComp && !defComp.IsDefused();
	}

	override bool CanBePerformedScript(IEntity user)
	{
		IEntity owner = GetOwner();
		if (!owner)
			return false;

		SPT_MineDefusalComponent defComp = SPT_MineDefusalComponent.Cast(
			owner.FindComponent(SPT_MineDefusalComponent)
		);
		return defComp && !defComp.IsDefused() && !defComp.IsBeingDefused();
	}

	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}
}
