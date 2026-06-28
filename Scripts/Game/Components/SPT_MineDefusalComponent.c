class SPT_MineDefusalComponentClass : ScriptComponentClass
{
}

class SPT_MineDefusalComponent : ScriptComponent
{
	[Attribute("5", desc: "Number of wires to display")]
	int m_iWireCount;

	[Attribute("45.0", desc: "Time limit in seconds to defuse the mine")]
	float m_fTimerSeconds;

	[Attribute("1", desc: "Requires an EOD tool to defuse")]
	bool m_bRequiresTool;

	[Attribute("0", desc: "Enable debug logging and hints for this mine")]
	bool m_bDebugMode;

	[Attribute("Mine", desc: "Display name of the mine type for the UI")]
	string m_sMineType;

	protected int m_iCorrectWire;
	protected int m_iPlayerId;
	[RplProp()]
	protected float m_fRemainingTime;
	protected bool m_bIsBeingDefused;
	protected bool m_bDefused;
	protected float m_fInteractionCheckTimer;
	protected bool m_bInteractionHintShown;
	protected EntityID m_TargetMineId;
	protected bool m_bUsesProxy;

	ref ScriptInvoker m_OnDefusalResult;

	void SPT_MineDefusalComponent(IEntityComponentSource src, IEntity ent, IEntity parent)
	{
		m_iPlayerId = -1;
		m_bIsBeingDefused = false;
		m_bDefused = false;
		m_fInteractionCheckTimer = 0;
		m_bInteractionHintShown = false;
		m_OnDefusalResult = new ScriptInvoker();
	}

	//! Associates a proxy component with the real vanilla mine.
	//! This is server-only state; all state-changing operations run on authority.
	void SetTargetMine(IEntity mine)
	{
		if (!mine)
			return;

		m_TargetMineId = mine.GetID();
		m_bUsesProxy = true;

		string prefabName = mine.ClassName();
		EntityPrefabData prefabData = mine.GetPrefabData();
		if (prefabData)
			prefabName = FilePath.StripPath(prefabData.GetPrefabName());

		if (prefabName != "")
			m_sMineType = prefabName;
	}

	override void EOnFrame(IEntity owner, float timeSlice)
	{
		super.EOnFrame(owner, timeSlice);

		if (m_bDefused)
			return;

		m_fInteractionCheckTimer = m_fInteractionCheckTimer + timeSlice;
		if (m_fInteractionCheckTimer >= 0.5)
		{
			m_fInteractionCheckTimer = 0;
			CheckPlayerProximity();
		}
	}

	protected void CheckPlayerProximity()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return;

		vector minePos = owner.GetOrigin();
		IEntity localPlayer = SCR_PlayerController.GetLocalControlledEntity();
		if (!localPlayer)
		{
			m_bInteractionHintShown = false;
			return;
		}

		vector playerPos = localPlayer.GetOrigin();
		float distance = vector.Distance(minePos, playerPos);

		if (distance < 3.0)
		{
			if (!m_bInteractionHintShown)
			{
				m_bInteractionHintShown = true;

				SCR_HintManagerComponent hintMgr = SCR_HintManagerComponent.GetInstance();
				if (hintMgr)
				{
					string hintMsg = "Pressione [Interagir] para desarmar " + m_sMineType;
					if (m_bRequiresTool)
						hintMsg = hintMsg + " (Kit EOD necessario)";
					hintMgr.ShowCustom(hintMsg);
				}

				if (m_bDebugMode)
					Print(string.Format("[SPT_MineDefusal] Jogador no alcance | Fios: %1 | Temporizador: %2s | Kit: %3",
						m_iWireCount, m_fTimerSeconds, m_bRequiresTool));
			}
		}
		else
		{
			m_bInteractionHintShown = false;
		}
	}

	void StartDefusal(int playerId)
	{
		if (m_bDefused)
			return;

		if (m_bIsBeingDefused)
			return;

		if (m_iWireCount < 2)
			m_iWireCount = 2;

		m_iCorrectWire = Math.RandomInt(0, m_iWireCount - 1);
		if (m_iCorrectWire < 0)
			m_iCorrectWire = 0;

		m_fRemainingTime = m_fTimerSeconds;
		m_bIsBeingDefused = true;
		m_iPlayerId = playerId;
		Replication.BumpMe();

		DebugLog(string.Format("Desarme iniciado pelo jogador %1 | FioCorreto: %2/%3 | Temporizador: %4s",
			playerId, m_iCorrectWire, m_iWireCount, m_fRemainingTime));

		GetGame().GetCallqueue().CallLater(TimerTick, 1000, true);
	}

	void RequestStartDefusal(int playerId)
	{
		if (Replication.IsServer())
		{
			RpcAsk_StartDefusal(playerId);
			return;
		}

		Rpc(RpcAsk_StartDefusal, playerId);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_StartDefusal(int playerId)
	{
		IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		IEntity owner = GetOwner();
		if (!player || !owner)
			return;

		if (vector.Distance(player.GetOrigin(), owner.GetOrigin()) > 4.0)
			return;

		StartDefusal(playerId);
	}

	void OnWireSelected(int wireIndex)
	{
		if (!m_bIsBeingDefused)
			return;

		m_bIsBeingDefused = false;

		if (wireIndex == m_iCorrectWire)
		{
			Defuse();
		}
		else
		{
			DebugLog(string.Format("Fio errado selecionado: %1 (correto era %2)", wireIndex, m_iCorrectWire));
			Detonate();
		}
	}

	void RequestSelectWire(int wireIndex)
	{
		if (wireIndex < 0 || wireIndex >= m_iWireCount)
			return;

		if (Replication.IsServer())
		{
			OnWireSelected(wireIndex);
			return;
		}

		Rpc(RpcAsk_SelectWire, wireIndex);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SelectWire(int wireIndex)
	{
		if (wireIndex < 0 || wireIndex >= m_iWireCount)
			return;

		OnWireSelected(wireIndex);
	}

	void CancelDefusal()
	{
		if (!m_bIsBeingDefused)
			return;

		m_bIsBeingDefused = false;
		m_iPlayerId = -1;
		m_fRemainingTime = 0;
		Replication.BumpMe();
		GetGame().GetCallqueue().Remove(TimerTick);

		DebugLog("Desarme cancelado pelo jogador");
	}

	void RequestCancelDefusal()
	{
		if (Replication.IsServer())
		{
			CancelDefusal();
			return;
		}

		Rpc(RpcAsk_CancelDefusal);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_CancelDefusal()
	{
		CancelDefusal();
	}

	protected void TimerTick()
	{
		if (!m_bIsBeingDefused)
			return;

		m_fRemainingTime = m_fRemainingTime - 1.0;
		Replication.BumpMe();

		if (m_fRemainingTime <= 0)
		{
			m_bIsBeingDefused = false;
			DebugLog("Temporizador expirado - detonando");
			Detonate();
		}
	}

	protected void Defuse()
	{
		m_bDefused = true;
		m_bIsBeingDefused = false;
		m_iPlayerId = -1;

		GetGame().GetCallqueue().Remove(TimerTick);

		IEntity mine = GetTargetMine();
		if (mine)
		{
			SCR_PressureTriggerComponent pressureTrigger = SCR_PressureTriggerComponent.Cast(
				mine.FindComponent(SCR_PressureTriggerComponent)
			);
			if (pressureTrigger)
			{
				pressureTrigger.DisarmTrigger();
			}
			else
			{
				DamageManagerComponent damageManager = DamageManagerComponent.Cast(
					mine.FindComponent(DamageManagerComponent)
				);
				if (damageManager)
					damageManager.EnableDamageHandling(false);
			}
		}

		DebugLog("Mina desarmada com sucesso");
		ShowResultHint(true);
		RpcDo_DefusalResult(true);
		DeleteDefusedMine(mine);
		DeleteProxyDelayed();
	}

	protected void DeleteDefusedMine(IEntity mine)
	{
		if (!mine || mine.IsDeleted())
			return;

		SCR_EntityHelper.DeleteEntityAndChildren(mine);
	}

	protected void Detonate()
	{
		m_bDefused = true;
		m_bIsBeingDefused = false;
		m_iPlayerId = -1;

		GetGame().GetCallqueue().Remove(TimerTick);

		IEntity mine = GetTargetMine();
		if (mine)
		{
			SCR_PressureTriggerComponent pressureTrigger = SCR_PressureTriggerComponent.Cast(
				mine.FindComponent(SCR_PressureTriggerComponent)
			);
			if (pressureTrigger)
			{
				pressureTrigger.TriggerManuallyServer();
			}
			else
			{
				DamageManagerComponent damageManager = DamageManagerComponent.Cast(
					mine.FindComponent(DamageManagerComponent)
				);
				if (damageManager)
					damageManager.SetHealthScaled(0);
			}
		}

		DebugLog("Mina detonada");
		ShowResultHint(false);
		RpcDo_DefusalResult(false);
		DeleteProxyDelayed();
	}

	protected IEntity GetTargetMine()
	{
		if (m_bUsesProxy)
		{
			IEntity target = GetGame().GetWorld().FindEntityByID(m_TargetMineId);
			if (target && !target.IsDeleted())
				return target;
		}

		// Components authored directly on custom mine prefabs do not need a proxy.
		return GetOwner();
	}

	protected void DeleteProxyDelayed()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return;

		if (!m_bUsesProxy)
			return;

		GetGame().GetCallqueue().CallLater(DeleteProxy, 5000, false);
	}

	protected void DeleteProxy()
	{
		IEntity owner = GetOwner();
		if (owner && !owner.IsDeleted())
			SCR_EntityHelper.DeleteEntityAndChildren(owner);
	}

	protected void ShowResultHint(bool success)
	{
		SCR_HintManagerComponent hintMgr = SCR_HintManagerComponent.GetInstance();
		if (!hintMgr)
			return;

		if (success)
			hintMgr.ShowCustom("MINA DESARMADA - Fio cortado corretamente");
		else
			hintMgr.ShowCustom("MINA DETONADA - Fio errado!");
	}

	void RpcDo_DefusalResult(bool success)
	{
		// Rpc() does not execute the receiver on a listen-server host.
		RpcDo_DefusalResult_Impl(success);
		Rpc(RpcDo_DefusalResult_Impl, success);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_DefusalResult_Impl(bool success)
	{
		if (m_OnDefusalResult)
			m_OnDefusalResult.Invoke(success);
	}

	bool IsBeingDefused()
	{
		return m_bIsBeingDefused;
	}

	bool IsDefused()
	{
		return m_bDefused;
	}

	bool RequiresTool()
	{
		return m_bRequiresTool;
	}

	int GetWireCount()
	{
		return m_iWireCount;
	}

	float GetRemainingTime()
	{
		return m_fRemainingTime;
	}

	string GetMineType()
	{
		return m_sMineType;
	}

	protected void DebugLog(string message)
	{
		if (!m_bDebugMode)
			return;

		IEntity owner = GetOwner();
		vector pos = "0 0 0";
		if (owner)
			pos = owner.GetOrigin();

		Print(string.Format("[SPT_MineDefusal] %1 | Pos: %2 | Fios: %3 | Correto: %4 | Temporiz: %5 | Kit: %6 | Desarmada: %7 | Ativo: %8",
			message, pos, m_iWireCount, m_iCorrectWire, m_fTimerSeconds, m_bRequiresTool, m_bDefused, m_bIsBeingDefused));
	}
}
