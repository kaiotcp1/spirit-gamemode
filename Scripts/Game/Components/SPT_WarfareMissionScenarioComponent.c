//! Contrato do prefab-raiz de um cenario de missao.
[ComponentEditorProps(category: "SPT/Warfare/Mission", description: "Raiz de uma composicao de missao Warfare.")]
class SPT_WarfareMissionScenarioComponentClass : ScriptComponentClass
{
}

class SPT_WarfareMissionScenarioComponent : ScriptComponent
{
	//! Ajusta somente filhos explicitamente marcados, evitando mover entidades funcionais por acidente.
	void SnapMarkedEntitiesToTerrain()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return;

		SnapHierarchy(owner, owner.GetWorld());
	}

	protected void SnapHierarchy(IEntity entity, BaseWorld world)
	{
		if (!entity)
			return;

		SPT_WarfareMissionPlacementComponent placement = SPT_WarfareMissionPlacementComponent.Cast(
			entity.FindComponent(SPT_WarfareMissionPlacementComponent));
		if (placement)
			placement.SnapToTerrain(world);

		IEntity child = entity.GetChildren();
		while (child)
		{
			SnapHierarchy(child, world);
			child = child.GetSibling();
		}
	}

	SPT_WarfareMissionObjectiveComponent FindObjective()
	{
		return FindObjectiveInHierarchy(GetOwner());
	}

	protected SPT_WarfareMissionObjectiveComponent FindObjectiveInHierarchy(IEntity entity)
	{
		if (!entity)
			return null;

		SPT_WarfareMissionObjectiveComponent objective = SPT_WarfareMissionObjectiveComponent.Cast(
			entity.FindComponent(SPT_WarfareMissionObjectiveComponent));
		if (objective)
			return objective;

		IEntity child = entity.GetChildren();
		while (child)
		{
			objective = FindObjectiveInHierarchy(child);
			if (objective)
				return objective;
			child = child.GetSibling();
		}

		return null;
	}
}
