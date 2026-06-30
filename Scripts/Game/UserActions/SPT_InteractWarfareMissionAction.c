//! Acao fina usada pelos objetivos de sabotagem, inteligencia e defesa.
class SPT_InteractWarfareMissionAction : ScriptedUserAction
{
	protected const float INTERACTION_COMPONENT_SEARCH_RADIUS = 15.0;
	protected SPT_WarfareMissionInteractionComponent m_FoundInteraction;

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
		IEntity searchOrigin = entity;
		while (entity)
		{
			SPT_WarfareMissionInteractionComponent interaction = SPT_WarfareMissionInteractionComponent.Cast(
				entity.FindComponent(SPT_WarfareMissionInteractionComponent));
			if (interaction)
				return interaction;
			entity = entity.GetParent();
		}

		if (!searchOrigin || !searchOrigin.GetWorld())
			return null;

		m_FoundInteraction = null;
		searchOrigin.GetWorld().QueryEntitiesBySphere(
			searchOrigin.GetOrigin(),
			INTERACTION_COMPONENT_SEARCH_RADIUS,
			CollectNearbyInteraction,
			FilterNearbyInteraction,
			EQueryEntitiesFlags.ALL);
		return m_FoundInteraction;
	}

	protected bool FilterNearbyInteraction(IEntity entity)
	{
		if (!entity || entity.IsDeleted())
			return false;

		return entity.FindComponent(SPT_WarfareMissionInteractionComponent) != null;
	}

	protected bool CollectNearbyInteraction(IEntity entity)
	{
		m_FoundInteraction = SPT_WarfareMissionInteractionComponent.Cast(
			entity.FindComponent(SPT_WarfareMissionInteractionComponent));
		return m_FoundInteraction != null;
	}
}
