//! Canal de requisicoes de missao anexado ao PlayerController, que pertence ao cliente.
[ComponentEditorProps(category: "SPT/Player", description: "Encaminha interacoes de missao do jogador para o servidor.")]
class SPT_PlayerMissionRpcComponentClass : ScriptComponentClass
{
}

class SPT_PlayerMissionRpcComponent : ScriptComponent
{
	void RequestDestroyAmmoCache(RplId scenarioRplId)
	{
		if (!scenarioRplId.IsValid())
			return;

		if (Replication.IsServer())
			RpcAsk_DestroyAmmoCache(scenarioRplId);
		else
			Rpc(RpcAsk_DestroyAmmoCache, scenarioRplId);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_DestroyAmmoCache(RplId scenarioRplId)
	{
		IEntity scenario = ResolveRplId(scenarioRplId);
		if (!scenario || scenario.IsDeleted())
			return;

		SPT_AmmoCacheDestructionComponent destruction = SPT_AmmoCacheDestructionComponent.Cast(
			scenario.FindComponent(SPT_AmmoCacheDestructionComponent));
		if (!destruction)
			return;

		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetOwner());
		if (!playerController)
			return;

		int playerId = playerController.GetPlayerId();
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return;

		IEntity player = playerManager.GetPlayerControlledEntity(playerId);
		if (!player || player.IsDeleted())
			return;

		destruction.TryStartDestruction(player);
	}

	protected IEntity ResolveRplId(RplId rplId)
	{
		RplComponent replication = RplComponent.Cast(Replication.FindItem(rplId));
		if (!replication)
			return null;

		return replication.GetEntity();
	}
}
