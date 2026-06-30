//! Marca um objeto filho de uma composicao para ajuste individual ao terreno.
[ComponentEditorProps(category: "SPT/Warfare/Mission", description: "Ajusta este objeto decorativo ao terreno quando o cenario de missao e criado.")]
class SPT_WarfareMissionPlacementComponentClass : ScriptComponentClass
{
}

class SPT_WarfareMissionPlacementComponent : ScriptComponent
{
	[Attribute("0", desc: "Offset vertical aplicado depois de encontrar o solo.")]
	protected float m_fVerticalOffset;

	[Attribute("0", desc: "Inclina o objeto conforme a aproximacao local do terreno. Desative para barracas e construcoes.")]
	protected bool m_bAlignToSlope;

	void SnapToTerrain(BaseWorld world)
	{
		IEntity owner = GetOwner();
		if (!owner || !world)
			return;

		vector position = owner.GetOrigin();
		float centerY = world.GetSurfaceY(position[0], position[2]);
		position[1] = centerY + m_fVerticalOffset;
		owner.SetOrigin(position);

		if (!m_bAlignToSlope)
			return;

		const float SAMPLE_DISTANCE = 1.0;
		float leftY = world.GetSurfaceY(position[0] - SAMPLE_DISTANCE, position[2]);
		float rightY = world.GetSurfaceY(position[0] + SAMPLE_DISTANCE, position[2]);
		float backY = world.GetSurfaceY(position[0], position[2] - SAMPLE_DISTANCE);
		float frontY = world.GetSurfaceY(position[0], position[2] + SAMPLE_DISTANCE);

		vector angles = owner.GetAngles();
		angles[0] = Math.Atan2(backY - frontY, SAMPLE_DISTANCE * 2.0) / Math.DEG2RAD;
		angles[2] = Math.Atan2(rightY - leftY, SAMPLE_DISTANCE * 2.0) / Math.DEG2RAD;
		owner.SetAngles(angles);
	}
}
