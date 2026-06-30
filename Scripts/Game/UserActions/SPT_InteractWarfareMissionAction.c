//! Acao fina usada pelos objetivos de sabotagem, inteligencia e defesa.
class SPT_InteractWarfareMissionAction : ScriptedUserAction
{
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		SPT_WarfareMissionInteractionComponent interaction = FindInteraction(pOwnerEntity);
		if (!interaction)
			return;

		IEntity scenario = interaction.GetOwner();
		RplComponent replication = RplComponent.Cast(scenario.FindComponent(RplComponent));
		PlayerController playerController = GetGame().GetPlayerController();
		if (!replication || !playerController)
			return;

		SPT_PlayerMissionRpcComponent missionRpc = SPT_PlayerMissionRpcComponent.Cast(
			playerController.FindComponent(SPT_PlayerMissionRpcComponent));
		if (missionRpc)
			missionRpc.RequestMissionInteraction(replication.Id());
	}

	override bool GetActionNameScript(out string outName)
	{
		SPT_WarfareMissionInteractionComponent interaction = FindInteraction(GetOwner());
		if (!interaction)
			return false;

		outName = interaction.GetActionName();
		return true;
	}

	override bool CanBeShownScript(IEntity user)
	{
		SPT_WarfareMissionInteractionComponent interaction = FindInteraction(GetOwner());
		if (!interaction)
			return false;

		return interaction.CanInteract();
	}

	override bool CanBePerformedScript(IEntity user)
	{
		return CanBeShownScript(user);
	}

	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	protected SPT_WarfareMissionInteractionComponent FindInteraction(IEntity entity)
	{
		while (entity)
		{
			SPT_WarfareMissionInteractionComponent interaction = SPT_WarfareMissionInteractionComponent.Cast(
				entity.FindComponent(SPT_WarfareMissionInteractionComponent));
			if (interaction)
				return interaction;
			entity = entity.GetParent();
		}

		return null;
	}
}
