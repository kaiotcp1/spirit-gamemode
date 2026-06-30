//! Base territorial compartilhada por HQs aliados e objetivos inimigos.
//! Nao deve ser colocada diretamente em entidades.
class SPT_WarfareNodeComponentClass : ScriptComponentClass
{
}

class SPT_WarfareNodeComponent : ScriptComponent
{
	[Attribute("", desc: "ID estavel e unico obrigatorio do ponto.")]
	protected string m_sPointId;

	[Attribute("", desc: "Nome de exibicao opcional. Se vazio, usa o ID.")]
	protected string m_sDisplayName;

	[Attribute("CUSTOM", UIWidgets.ComboBox, "Tipo visual da area.", "", ParamEnumArray.FromEnum(SPT_EWarfarePointType))]
	protected SPT_EWarfarePointType m_ePointType;

	[Attribute("150", desc: "Raio territorial em metros.")]
	protected float m_fRadius;

	string GetPointId() { return m_sPointId; }
	void SetPointId(string id) { m_sPointId = id; }

	string GetDisplayName() { return m_sDisplayName; }
	void SetDisplayName(string name) { m_sDisplayName = name; }

	SPT_EWarfarePointType GetPointType() { return m_ePointType; }
	void SetPointType(SPT_EWarfarePointType type) { m_ePointType = type; }

	float GetRadius() { return m_fRadius; }
	void SetRadius(float radius) { m_fRadius = radius; }

	bool IsHQ() { return false; }
	bool CountsForVictory() { return false; }
}
