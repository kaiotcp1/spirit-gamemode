//! Estados de um ponto Warfare durante a campanha de conquista territorial.
enum SPT_EWarfarePointState
{
	//! Area inimiga desconectada da frente azul. Nao pode ser atacada.
	LOCKED,
	//! Area inimiga conectada a frente, disponivel para ataque.
	FRONTLINE,
	//! Primeira baixa registrada; reforcos autorizados.
	UNDER_ATTACK,
	//! Guarnicao eliminada, aguardando presenca de jogador ou conexao territorial.
	CLEARED_WAITING,
	//! Capturada pelo jogador, mas reforcos inimigos ainda estao chegando.
	CAPTURED_DEFENDING,
	//! Area conquistada e batalha encerrada.
	CAPTURED
}

//! Tipos de area para pontos Warfare.
enum SPT_EWarfarePointType
{
	CITY,
	VILLAGE,
	MILITARY_BASE,
	AIRPORT,
	RUIN,
	PORT,
	FOREST,
	MOUNTAIN,
	FIELD,
	CUSTOM
}

//! Dados de configuracao de um ponto Warfare, extraidos do SPT_WarfarePointComponent
//! e cacheados pelo manager para consulta rapida.
class SPT_WarfarePointData : Managed
{
	//! ID estavel do ponto (string unica definida no componente ou derivada do descritor).
	string m_sPointId;

	//! Nome de exibicao do ponto.
	string m_sDisplayName;

	//! Tipo da area.
	SPT_EWarfarePointType m_ePointType;

	//! Centro do ponto no mundo.
	vector m_vCenter;

	//! Raio de ataque/captura em metros.
	float m_fRadius;

	//! IDs dos pontos vizinhos (links do grafo territorial).
	ref array<string> m_aNeighborIds;

	//! Verdadeiro se este ponto e um HQ inicial.
	bool m_bIsHQ;

	//! Verdadeiro se este ponto conta para a condicao de vitoria.
	bool m_bCountsForVictory;

	//! Verdadeiro se este ponto foi configurado manualmente (sobrescreve descritor automatico).
	bool m_bManualOverride;

	//! ID da localizacao de guarnicao associada (do SPT_WorldGarrisonManagerComponent).
	string m_sGarrisonLocationId;

	void SPT_WarfarePointData()
	{
		m_aNeighborIds = new array<string>();
		m_fRadius = 150.0;
		m_bCountsForVictory = true;
	}
}

//! Snapshot de estado de um ponto Warfare, enviado para clientes.
class SPT_WarfarePointStateSnapshot : Managed
{
	string m_sPointId;
	SPT_EWarfarePointState m_eState;
	bool m_bCaptured;
	int m_iGarrisonManpower;
	int m_iGarrisonInitialManpower;
	int m_iBattleWaveIndex;

	void SPT_WarfarePointStateSnapshot()
	{
		m_eState = SPT_EWarfarePointState.LOCKED;
	}
}
