//! Componente colocado no World Editor em cada area participante do Warfare.
//! Define ID, tipo, raio, ordem de captura e participacao na vitoria.
[ComponentEditorProps(category: "SPT/Warfare", description: "Define um ponto estrategico para o modo SPT Warfare.")]
class SPT_WarfarePointComponentClass : ScriptComponentClass
{
}

class SPT_WarfarePointComponent : ScriptComponent
{
	//-----------------------------------------------------------------------
	// ATRIBUTOS
	//-----------------------------------------------------------------------

	[Attribute("", desc: "ID estavel e unico obrigatorio do ponto.")]
	protected string m_sPointId;

	[Attribute("", desc: "Nome de exibicao opcional. Se vazio, usa o ID.")]
	protected string m_sDisplayName;

	[Attribute("CUSTOM", UIWidgets.ComboBox, "Tipo da area.", "", ParamEnumArray.FromEnum(SPT_EWarfarePointType))]
	protected SPT_EWarfarePointType m_ePointType;

	[Attribute("150", desc: "Raio de ataque/captura em metros.")]
	protected float m_fRadius;

	[Attribute("1", desc: "Ordem de captura. Use 0 para HQ SAFE, 1 para a primeira etapa inimiga, 2 para a segunda, etc.")]
	protected int m_iCaptureOrder;

	[Attribute("1", desc: "Se falso, este ponto nao conta para a condicao de vitoria.")]
	protected bool m_bCountsForVictory;

	//-----------------------------------------------------------------------
	// GETTERS/SETTERS
	//-----------------------------------------------------------------------

	string GetPointId() { return m_sPointId; }
	void SetPointId(string id) { m_sPointId = id; }

	string GetDisplayName() { return m_sDisplayName; }
	void SetDisplayName(string name) { m_sDisplayName = name; }

	SPT_EWarfarePointType GetPointType() { return m_ePointType; }
	void SetPointType(SPT_EWarfarePointType type) { m_ePointType = type; }

	float GetRadius() { return m_fRadius; }
	void SetRadius(float radius) { m_fRadius = radius; }

	int GetCaptureOrder() { return m_iCaptureOrder; }
	bool IsHQ() { return m_iCaptureOrder == 0; }
	bool CountsForVictory() { return m_bCountsForVictory; }
}
