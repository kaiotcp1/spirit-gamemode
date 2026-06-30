//! Substitui um alvo destruido pelo prefab de ruina configurado.
[ComponentEditorProps(category: "SPT/Warfare/Mission", description: "Define a ruina usada apos a destruicao do alvo.")]
class SPT_WarfareMissionRuinTargetComponentClass : ScriptComponentClass
{
}

class SPT_WarfareMissionRuinTargetComponent : ScriptComponent
{
	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Prefab de ruina criado na mesma transformacao do alvo.", params: "et")]
	protected ResourceName m_rRuinPrefab;

	void ReplaceWithRuin()
	{
		if (!Replication.IsServer() || m_rRuinPrefab.IsEmpty())
			return;

		IEntity owner = GetOwner();
		if (!owner || owner.IsDeleted())
			return;

		Resource resource = Resource.Load(m_rRuinPrefab);
		if (!resource)
		{
			Print(string.Format("[SPT_WarfareMission] Ruina invalida | prefab=%1", m_rRuinPrefab), LogLevel.ERROR);
			return;
		}

		vector transform[4];
		owner.GetWorldTransform(transform);

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform = transform;
		IEntity ruin = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
		if (!ruin)
		{
			Print(string.Format("[SPT_WarfareMission] Falha ao criar ruina | prefab=%1", m_rRuinPrefab), LogLevel.ERROR);
			return;
		}

		SCR_EntityHelper.DeleteEntityAndChildren(owner);
	}
}
