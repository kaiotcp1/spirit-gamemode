//! Marca a entidade cuja conclusao encerra a missao da composicao.
//! Suporta conclusao programatica e eliminacao por DamageManager.
[ComponentEditorProps(category: "SPT/Warfare/Mission", description: "Objetivo principal de uma missao Warfare.")]
class SPT_WarfareMissionObjectiveComponentClass : ScriptComponentClass
{
}

class SPT_WarfareMissionObjectiveComponent : ScriptComponent
{
	protected string m_sPointId;
	protected bool m_bCompletedByAction;

	void Initialize(string pointId)
	{
		m_sPointId = pointId;
		m_bCompletedByAction = false;
	}

	string GetPointId()
	{
		return m_sPointId;
	}

	//! Conclui objetivos controlados por interacao.
	void CompleteObjective()
	{
		m_bCompletedByAction = true;
	}

	//! Compatibilidade com o controlador AmmoCache existente.
	void TriggerDestruction()
	{
		CompleteObjective();
	}

	bool IsCompleted()
	{
		if (m_bCompletedByAction)
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

	//! Compatibilidade com chamadas anteriores.
	bool IsDestroyed()
	{
		return IsCompleted();
	}
}
