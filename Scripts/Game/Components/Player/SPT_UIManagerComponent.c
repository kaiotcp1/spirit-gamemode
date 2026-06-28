class SPT_UIManagerComponentClass : ScriptComponentClass
{
}

class SPT_UIManagerComponent : ScriptComponent
{
	[Attribute("", UIWidgets.Object)]
	ref array<ref SPT_UIContext> m_aContexts;

	protected SCR_CharacterControllerComponent m_Controller;

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (SCR_Global.IsEditMode())
			return;

		SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(owner.FindComponent(SCR_CharacterControllerComponent));
		if (controller)
		{
			m_Controller = controller;
			m_Controller.m_OnControlledByPlayer.Insert(OnControlledByPlayer);
			m_Controller.m_OnPlayerDeath.Insert(OnPlayerDeath);
		}

		foreach (SPT_UIContext context : m_aContexts)
		{
			context.Init(owner, this);
		}
	}

	void ShowContext(typename typeName)
	{
		foreach (SPT_UIContext context : m_aContexts)
		{
			if (context.ClassName() == typeName.ToString())
			{
				context.ShowLayout();
				break;
			}
		}
	}

	SPT_UIContext GetContext(typename typeName)
	{
		foreach (SPT_UIContext context : m_aContexts)
		{
			if (context.ClassName() == typeName.ToString())
				return context;
		}
		return null;
	}

	override void EOnFrame(IEntity owner, float timeSlice)
	{
		GetGame().GetInputManager().ActivateContext("SPT_GeneralContext");

		foreach (SPT_UIContext context : m_aContexts)
		{
			context.EOnFrame(owner, timeSlice);
		}
	}

	protected void OnControlledByPlayer(IEntity owner, bool controlled)
	{
		GetGame().GetCallqueue().CallLater(AfterControlledByPlayer, 0, false, owner, controlled);
	}

	protected void AfterControlledByPlayer(IEntity owner, bool controlled)
	{
		if (!controlled)
		{
			ClearEventMask(owner, EntityEvent.FRAME);
			foreach (SPT_UIContext context : m_aContexts)
			{
				context.UnregisterInputs();
			}
		}
		else if (owner)
		{
			SetEventMask(owner, EntityEvent.FRAME);
			foreach (SPT_UIContext context : m_aContexts)
			{
				context.OnControlledByPlayer();
				context.RegisterInputs();
			}
		}
	}

	protected void OnPlayerDeath()
	{
		foreach (SPT_UIContext context : m_aContexts)
		{
			context.CloseLayout();
			context.UnregisterInputs();
		}
	}

	void RpcAsk_StartDefusal(RplId mineRplId)
	{
		Rpc(RpcAsk_StartDefusal_Impl, mineRplId);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_StartDefusal_Impl(RplId mineRplId)
	{
		IEntity mineEntity = ResolveRplId(mineRplId);
		if (!mineEntity)
			return;

		SPT_MineDefusalComponent defComp = SPT_MineDefusalComponent.Cast(mineEntity.FindComponent(SPT_MineDefusalComponent));
		if (!defComp)
			return;

		int playerId = SCR_PossessingManagerComponent.GetPlayerIdFromControlledEntity(GetOwner());
		defComp.StartDefusal(playerId);
	}

	void RpcAsk_SelectWire(RplId mineRplId, int wireIndex)
	{
		Rpc(RpcAsk_SelectWire_Impl, mineRplId, wireIndex);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SelectWire_Impl(RplId mineRplId, int wireIndex)
	{
		IEntity mineEntity = ResolveRplId(mineRplId);
		if (!mineEntity)
			return;

		SPT_MineDefusalComponent defComp = SPT_MineDefusalComponent.Cast(mineEntity.FindComponent(SPT_MineDefusalComponent));
		if (!defComp)
			return;

		defComp.OnWireSelected(wireIndex);
	}

	void RpcAsk_CancelDefusal(RplId mineRplId)
	{
		Rpc(RpcAsk_CancelDefusal_Impl, mineRplId);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_CancelDefusal_Impl(RplId mineRplId)
	{
		IEntity mineEntity = ResolveRplId(mineRplId);
		if (!mineEntity)
			return;

		SPT_MineDefusalComponent defComp = SPT_MineDefusalComponent.Cast(mineEntity.FindComponent(SPT_MineDefusalComponent));
		if (!defComp)
			return;

		defComp.CancelDefusal();
	}

	protected IEntity ResolveRplId(RplId rplId)
	{
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(rplId));
		if (!rpl)
			return null;
		return rpl.GetEntity();
	}
}
