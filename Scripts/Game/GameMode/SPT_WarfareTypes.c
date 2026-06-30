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

//! Tipos de missao opcionais vinculados a um WarfarePoint.
enum SPT_EWarfareMissionType
{
	NONE,
	//! Destruir um cache de municao composto por 4 caixas de equipamento.
	//! Usa o prefab vanilla EquipmentBoxStack_US_01_V4_covered (4x).
	//! GUID: {09321D1470C3625E}
	DESTROY_AMMO_CACHE,
	//! Sabotar uma instalacao de radio/antena.
	SABOTAGE_COMMUNICATIONS,
	//! Destruir uma instalacao de armazenamento de combustivel.
	DESTROY_FUEL_DEPOT,
	//! Coletar documentos ou inteligencia inimiga.
	STEAL_INTELLIGENCE,
	//! Eliminar um oficial inimigo representado por um objetivo com DamageManager.
	ELIMINATE_OFFICER,
	//! Ativar e defender um transmissor durante um intervalo configuravel.
	DEFEND_TRANSMITTER
}

//! Estado independente da captura territorial.
enum SPT_EWarfareMissionState
{
	INACTIVE,
	ACTIVE,
	COMPLETED
}

//! Mapeamento central entre o tipo selecionado no editor e sua composicao.
class SPT_WarfareMissionPrefabRegistry
{
	static ResourceName GetScenarioPrefab(SPT_EWarfareMissionType missionType)
	{
		switch (missionType)
		{
			case SPT_EWarfareMissionType.DESTROY_AMMO_CACHE:
				return "{69BC100100000000}Prefabs/Warfare/Missions/SPT_DestroyAmmoCacheScenario.et";
			case SPT_EWarfareMissionType.SABOTAGE_COMMUNICATIONS:
				return "{69D1000100000000}Prefabs/Warfare/Missions/SPT_SabotageCommunicationsScenario.et";
			case SPT_EWarfareMissionType.DESTROY_FUEL_DEPOT:
				return "{69D1000200000000}Prefabs/Warfare/Missions/SPT_DestroyFuelDepotScenario.et";
			case SPT_EWarfareMissionType.STEAL_INTELLIGENCE:
				return "{69D1000300000000}Prefabs/Warfare/Missions/SPT_StealIntelligenceScenario.et";
			case SPT_EWarfareMissionType.ELIMINATE_OFFICER:
				return "{69D1000500000000}Prefabs/Warfare/Missions/SPT_EliminateOfficerScenario.et";
			case SPT_EWarfareMissionType.DEFEND_TRANSMITTER:
				return "{69D1000400000000}Prefabs/Warfare/Missions/SPT_DefendTransmitterScenario.et";
		}

		return "";
	}
}

//! Configuracao copiada do componente colocado no mapa.
class SPT_WarfareMissionConfig : Managed
{
	bool m_bEnabled;
	SPT_EWarfareMissionType m_eType;
	ResourceName m_rScenarioPrefab;
	float m_fActivationDistance = 800.0;
	float m_fTerrainSearchRadius = 100.0;
	float m_fMaximumSlopeDegrees = 12.0;
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

	//! Configuracao de missao hostil. Permanece nula para HQs.
	ref SPT_WarfareMissionConfig m_MissionConfig;

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
