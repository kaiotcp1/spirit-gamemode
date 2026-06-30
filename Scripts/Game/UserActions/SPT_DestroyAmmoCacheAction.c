//! Acao de usuario para destruir um Ammo Cache.
//! Declarada no ActionsManagerComponent da raiz replicada do cenario.
//! As caixas vanilla permanecem somente como elementos visuais.
class SPT_DestroyAmmoCacheAction : ScriptedUserAction
{
	//-----------------------------------------------------------------------
	// ACAO
	//-----------------------------------------------------------------------

	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		SPT_AmmoCacheDestructionComponent destruction = FindDestructionComponent(pOwnerEntity);
		if (!destruction)
			return;

		IEntity scenario = destruction.GetOwner();
		RplComponent scenarioReplication = RplComponent.Cast(scenario.FindComponent(RplComponent));
		if (!scenarioReplication)
			return;

		PlayerController playerController = GetGame().GetPlayerController();
		if (!playerController)
			return;

		SPT_PlayerMissionRpcComponent missionRpc = SPT_PlayerMissionRpcComponent.Cast(
			playerController.FindComponent(SPT_PlayerMissionRpcComponent));
		if (!missionRpc)
		{
			Print("[SPT_AmmoCache] PlayerController sem SPT_PlayerMissionRpcComponent.", LogLevel.ERROR);
			return;
		}

		missionRpc.RequestDestroyAmmoCache(scenarioReplication.Id());
	}

	//-----------------------------------------------------------------------
	// INTERFACE
	//-----------------------------------------------------------------------

	override bool GetActionNameScript(out string outName)
	{
		outName = "Destruir Ammo Cache";
		return true;
	}

	override bool CanBeShownScript(IEntity user)
	{
		IEntity owner = GetOwner();
		if (!owner)
			return false;

		SPT_AmmoCacheDestructionComponent destruction = FindDestructionComponent(owner);
		if (!destruction)
			return false;

		if (destruction.IsDestroyed())
			return false;
		if (destruction.IsDestructionInProgress())
			return false;

		return true;
	}

	override bool CanBePerformedScript(IEntity user)
	{
		IEntity owner = GetOwner();
		if (!owner)
			return false;

		SPT_AmmoCacheDestructionComponent destruction = FindDestructionComponent(owner);
		if (!destruction)
			return false;

		if (destruction.IsDestroyed())
			return false;
		if (destruction.IsDestructionInProgress())
			return false;

		return true;
	}

	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	protected SPT_AmmoCacheDestructionComponent FindDestructionComponent(IEntity entity)
	{
		while (entity)
		{
			SPT_AmmoCacheDestructionComponent destruction = SPT_AmmoCacheDestructionComponent.Cast(
				entity.FindComponent(SPT_AmmoCacheDestructionComponent));
			if (destruction)
				return destruction;

			entity = entity.GetParent();
		}

		return null;
	}
}
