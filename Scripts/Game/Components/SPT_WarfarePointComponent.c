//! Componente colocado no World Editor em cada area participante do Warfare.
//! Define os metadados de um ponto estrategico: ID, tipo, raio, links e
//! configuracoes de captura/vitoria.
[ComponentEditorProps(category: "SPT/Warfare", description: "Define um ponto estrategico para o modo SPT Warfare.")]
class SPT_WarfarePointComponentClass : ScriptComponentClass
{
}

class SPT_WarfarePointComponent : ScriptComponent
{
	//-----------------------------------------------------------------------
	// ATRIBUTOS
	//-----------------------------------------------------------------------

	[Attribute("", desc: "ID estavel e unico do ponto. Deixe vazio para derivar do descritor de mapa mais proximo.")]
	protected string m_sPointId;

	[Attribute("", desc: "Nome de exibicao opcional. Se vazio, herda do descritor de mapa.")]
	protected string m_sDisplayName;

	[Attribute("CUSTOM", UIWidgets.ComboBox, "Tipo da area.", "", ParamEnumArray.FromEnum(SPT_EWarfarePointType))]
	protected SPT_EWarfarePointType m_ePointType;

	[Attribute("150", desc: "Raio de ataque/captura em metros.")]
	protected float m_fRadius;

	[Attribute("", desc: "IDs dos pontos vizinhos (separados por virgula) para formar o grafo territorial.")]
	protected string m_sNeighborIds;

	[Attribute("0", desc: "Marque como verdadeiro se este ponto for um HQ inicial.")]
	protected bool m_bIsHQ;

	[Attribute("1", desc: "Se falso, este ponto nao conta para a condicao de vitoria.")]
	protected bool m_bCountsForVictory;

	[Attribute("1", desc: "Quando verdadeiro, este componente sobrescreve qualquer deteccao automatica do descritor de mapa.")]
	protected bool m_bManualOverride;

	[Attribute("150", desc: "Distancia maxima para vincular este componente a um descritor de mapa automatico.")]
	protected float m_fAutoLinkRadius;

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

	string GetNeighborIdsRaw() { return m_sNeighborIds; }

	//! Retorna os IDs de vizinhos como um array de strings,
	//! separando a string por virgulas.
	void GetNeighborIds(notnull array<string> outIds)
	{
		outIds.Clear();
		if (m_sNeighborIds.IsEmpty())
			return;

		m_sNeighborIds.Split(",", outIds, false);
		// Remove espacos em branco de cada ID
		for (int i = outIds.Count() - 1; i >= 0; i--)
		{
			outIds[i] = outIds[i].Trim();
			if (outIds[i].IsEmpty())
				outIds.Remove(i);
		}
	}

	bool IsHQ() { return m_bIsHQ; }
	bool CountsForVictory() { return m_bCountsForVictory; }
	bool IsManualOverride() { return m_bManualOverride; }
	float GetAutoLinkRadius() { return m_fAutoLinkRadius; }
}
