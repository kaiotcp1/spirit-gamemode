//! Estados de um ponto Warfare durante a campanha de conquista territorial.
enum SPT_EWarfarePointState
{
	//! Area inimiga disponivel para ataque.
	//! O valor 1 preserva compatibilidade com snapshots anteriores.
	FRONTLINE = 1,
	//! Primeira baixa registrada; reforcos autorizados.
	UNDER_ATTACK = 2,
	//! Guarnicao eliminada, aguardando presenca de jogador.
	CLEARED_WAITING = 3,
	//! Capturada pelo jogador, mas reforcos inimigos ainda estao chegando.
	CAPTURED_DEFENDING = 4,
	//! Area conquistada e batalha encerrada.
	CAPTURED = 5
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

class SPT_GarrisonLocationConfig : Managed
{
	float m_fSpawnDistance = 1200.0;
	float m_fDespawnDistance = 1600.0;
	float m_fDespawnHysteresis = 150.0;
	float m_fBuildingSearchRadius = 225.0;
	float m_fPatrolRadius = 500.0;
	int m_iLocationBudget = 100;
	bool m_bEnableCaching = true;
	int m_iSpawnIntervalMs = 500;
	ref array<ResourceName> m_aCQBGroupPrefabs = new array<ResourceName>();
	ref array<ResourceName> m_aPatrolGroupPrefabs = new array<ResourceName>();
	ResourceName m_sPatrolWaypointPrefab;
	SCR_EAIGroupFormation m_ePatrolFormation;
	EMovementType m_ePatrolMovementType;
	EMovementType m_eBattleMovementType = EMovementType.RUN;
	bool m_bBattleEnabled = true;
	int m_iBattleInitialDelayMinMs;
	int m_iBattleInitialDelayMaxMs;
	int m_iBattleWaveDelayMinMs;
	int m_iBattleWaveDelayMaxMs;
	int m_iBattleSpawnIntervalMs = 1000;
	int m_iBattleWaveUnitsMin = 16;
	int m_iBattleWaveUnitsMax = 28;
	float m_fBattleWaveAliveThreshold = 0.35;
	float m_fBattleSpawnDistanceMin = 600.0;
	float m_fBattleSpawnDistanceMax = 1000.0;
	float m_fBattleConcentratedWeight = 0.25;
	float m_fBattleSpreadedWeight = 0.35;
	float m_fBattleConvoyWeight = 0.4;
	float m_fBattleVehicleReinforcementChance;
	ref array<ResourceName> m_aBattleGroupPrefabs = new array<ResourceName>();
	ref array<ref SPT_BattleVehicleConfig> m_aBattleVehicles = new array<ref SPT_BattleVehicleConfig>();
}

//! Dados territoriais extraidos de um HQ aliado ou objetivo inimigo.
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

	//! Verdadeiro se este ponto e um HQ inicial.
	bool m_bIsHQ;

	//! Verdadeiro se este ponto conta para a condicao de vitoria.
	bool m_bCountsForVictory;

	//! ID da localizacao de guarnicao associada (do SPT_WorldGarrisonManagerComponent).
	string m_sGarrisonLocationId;

	//! Usado somente pelo preview do editor para destacar configuracao invalida.
	bool m_bPreviewInvalid;

	//! Configuracao hostil. Permanece nula para HQs.
	ref SPT_GarrisonLocationConfig m_GarrisonConfig;

	void SPT_WarfarePointData()
	{
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
		m_eState = SPT_EWarfarePointState.FRONTLINE;
	}
}
