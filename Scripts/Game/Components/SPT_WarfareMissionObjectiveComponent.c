//! Marca a entidade destrutivel que conclui a missao da composicao.
//! Suporta tanto destruicao por dano (tiros/explosivos) quanto
//! destruicao programatica via TriggerDestruction (ex: acao do jogador).
[ComponentEditorProps(category: "SPT/Warfare/Mission", description: "Objetivo destrutivel de uma missao Warfare.")]
class SPT_WarfareMissionObjectiveComponentClass : ScriptComponentClass
{
}

class SPT_WarfareMissionObjectiveComponent : ScriptComponent
{
	protected string m_sPointId;
	protected bool m_bDestroyedByAction;

	void Initialize(string pointId)
	{
		m_sPointId = pointId;
		m_bDestroyedByAction = false;
	}

	string GetPointId()
	{
		return m_sPointId;
	}

	//! Chamado por SPT_AmmoCacheDestructionComponent apos o timer de explosao.
	void TriggerDestruction()
	{
		m_bDestroyedByAction = true;
	}

	bool IsDestroyed()
	{
		// Destruicao programatica (acao do jogador)
		if (m_bDestroyedByAction)
			return true;

		IEntity owner = GetOwner();
		if (!owner || owner.IsDeleted())
			return true;

		DamageManagerComponent damageManager = DamageManagerComponent.Cast(
			owner.FindComponent(DamageManagerComponent));
		if (!damageManager)
			return false;

		return damageManager.GetHealthScaled() <= 0.0;
	}
}
