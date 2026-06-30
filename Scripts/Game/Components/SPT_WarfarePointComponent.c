//! Componente colocado no World Editor em cada area participante do Warfare.
//! Define ID, tipo, raio e participacao na vitoria.
[ComponentEditorProps(category: "SPT/Warfare", description: "Define um ponto estrategico para o modo SPT Warfare.")]
class SPT_WarfarePointComponentClass : SPT_WarfareNodeComponentClass
{
}

class SPT_WarfarePointComponent : SPT_WarfareNodeComponent
{
	//-----------------------------------------------------------------------
	// ATRIBUTOS
	//-----------------------------------------------------------------------

	[Attribute("1", desc: "Se falso, este ponto nao conta para a condicao de vitoria.")]
	protected bool m_bCountsForVictory;

	[Attribute("1200", desc: "Distancia de spawn desta area em metros.", category: "Garrison Streaming")]
	protected float m_fGarrisonSpawnDistance;

	[Attribute("1600", desc: "Distancia de despawn desta area em metros. Deve ser maior que a distancia de spawn.", category: "Garrison Streaming")]
	protected float m_fGarrisonDespawnDistance;

	[Attribute("150", desc: "Histerese extra de despawn desta area.", category: "Garrison Streaming")]
	protected float m_fGarrisonDespawnHysteresis;

	[Attribute("225", desc: "Raio para procurar construcoes CQB.", category: "Garrison Streaming")]
	protected float m_fGarrisonBuildingSearchRadius;

	[Attribute("500", desc: "Raio de patrulha da area.", category: "Garrison Streaming")]
	protected float m_fGarrisonPatrolRadius;

	[Attribute("100", desc: "Budget total desta area. 0 = ilimitado.", category: "Garrison Streaming")]
	protected int m_iGarrisonLocationBudget;

	[Attribute("1", desc: "Preserva os sobreviventes ao descarregar esta area.", category: "Garrison Streaming")]
	protected bool m_bGarrisonEnableCaching;

	[Attribute("500", desc: "Intervalo entre spawns individuais em ms. 0 = todos no mesmo frame.", category: "Garrison Streaming")]
	protected int m_iGarrisonSpawnIntervalMs;

	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Grupos CQB desta area. Vazio = nenhum grupo CQB.", params: "et class=SCR_AIGroup", category: "Garrison Prefabs")]
	protected ref array<ResourceName> m_aGarrisonCQBGroupPrefabs;

	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Grupos de patrulha desta area. Vazio = nenhuma patrulha.", params: "et class=SCR_AIGroup", category: "Garrison Prefabs")]
	protected ref array<ResourceName> m_aGarrisonPatrolGroupPrefabs;

	[Attribute("{FFF9518F73279473}PrefabsEditable/Auto/AI/Waypoints/E_AIWaypoint_Move.et", UIWidgets.ResourceNamePicker, desc: "Waypoint usado pelas patrulhas desta area.", params: "et", category: "Garrison Prefabs")]
	protected ResourceName m_sGarrisonPatrolWaypointPrefab;

	[Attribute(SCR_EAIGroupFormation.Wedge.ToString(), UIWidgets.ComboBox, "Formacao das patrulhas desta area.", "", ParamEnumArray.FromEnum(SCR_EAIGroupFormation), category: "Garrison Prefabs")]
	protected SCR_EAIGroupFormation m_eGarrisonPatrolFormation;

	[Attribute(EMovementType.WALK.ToString(), UIWidgets.ComboBox, "Movimento das patrulhas desta area.", "", ParamEnumArray.FromEnum(EMovementType), category: "Garrison Prefabs")]
	protected EMovementType m_eGarrisonPatrolMovementType;

	[Attribute(EMovementType.RUN.ToString(), UIWidgets.ComboBox, "Movimento dos reforcos desta area ate o objetivo.", "", ParamEnumArray.FromEnum(EMovementType), category: "Garrison Battle")]
	protected EMovementType m_eBattleMovementType;

	[Attribute("1", desc: "Ativa batalha e reforcos nesta area.", category: "Garrison Battle")]
	protected bool m_bBattleEnabled;

	[Attribute("0", desc: "Atraso minimo da primeira onda em ms.", category: "Garrison Battle")]
	protected int m_iBattleInitialDelayMinMs;

	[Attribute("0", desc: "Atraso maximo da primeira onda em ms.", category: "Garrison Battle")]
	protected int m_iBattleInitialDelayMaxMs;

	[Attribute("0", desc: "Intervalo minimo entre ondas em ms.", category: "Garrison Battle")]
	protected int m_iBattleWaveDelayMinMs;

	[Attribute("0", desc: "Intervalo maximo entre ondas em ms.", category: "Garrison Battle")]
	protected int m_iBattleWaveDelayMaxMs;

	[Attribute("1000", desc: "Intervalo em ms entre grupos de reforco materializados pela fila. Cada grupo pode conter varias unidades.", category: "Garrison Battle")]
	protected int m_iBattleSpawnIntervalMs;

	[Attribute("16", desc: "Unidades minimas por onda.", category: "Garrison Battle")]
	protected int m_iBattleWaveUnitsMin;

	[Attribute("28", desc: "Unidades maximas por onda.", category: "Garrison Battle")]
	protected int m_iBattleWaveUnitsMax;

	[Attribute("0.35", desc: "Proporcao de sobreviventes para antecipar a proxima onda.", category: "Garrison Battle")]
	protected float m_fBattleWaveAliveThreshold;

	[Attribute("600", desc: "Distancia minima de spawn dos reforcos.", category: "Garrison Battle")]
	protected float m_fBattleSpawnDistanceMin;

	[Attribute("1000", desc: "Distancia maxima de spawn dos reforcos.", category: "Garrison Battle")]
	protected float m_fBattleSpawnDistanceMax;

	[Attribute("0.25", desc: "Peso CONCENTRATED.", category: "Garrison Battle")]
	protected float m_fBattleConcentratedWeight;

	[Attribute("0.35", desc: "Peso SPREADED.", category: "Garrison Battle")]
	protected float m_fBattleSpreadedWeight;

	[Attribute("0.4", desc: "Peso CONVOY.", category: "Garrison Battle")]
	protected float m_fBattleConvoyWeight;

	[Attribute("0", desc: "Chance de cada onda usar reforcos veiculares. 0 = somente o peso CONVOY; 1 = toda onda elegivel tenta spawnar veiculos.", category: "Garrison Battle")]
	protected float m_fBattleVehicleReinforcementChance;

	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Grupos de reforco desta area. Vazio = reutiliza os grupos de patrulha desta area.", params: "et class=SCR_AIGroup", category: "Garrison Battle")]
	protected ref array<ResourceName> m_aBattleGroupPrefabs;

	[Attribute("", desc: "Veiculos e grupos de tripulacao usados pelos comboios desta area.", category: "Garrison Battle")]
	protected ref array<ref SPT_BattleVehicleConfig> m_aBattleVehicles;

	[Attribute("0", desc: "Ativa uma missao independente da captura territorial neste ponto.", category: "Mission")]
	protected bool m_bMissionEnabled;

	[Attribute(SPT_EWarfareMissionType.NONE.ToString(), UIWidgets.ComboBox, "Tipo da missao.", "", ParamEnumArray.FromEnum(SPT_EWarfareMissionType), category: "Mission")]
	protected SPT_EWarfareMissionType m_eMissionType;

	[Attribute("{69BC100100000000}Prefabs/Warfare/Missions/SPT_DestroyAmmoCacheScenario.et", UIWidgets.ResourceNamePicker, desc: "Prefab composto replicavel contendo o ambiente e um objetivo de missao.", params: "et", category: "Mission")]
	protected ResourceName m_rMissionScenarioPrefab;

	[Attribute("800", desc: "Distancia em metros para ativar a missao quando um jogador se aproxima.", category: "Mission")]
	protected float m_fMissionActivationDistance;

	[Attribute("100", desc: "Raio em metros para buscar terreno adequado ao redor do WarfarePoint.", category: "Mission")]
	protected float m_fMissionTerrainSearchRadius;

	[Attribute("12", desc: "Inclinacao maxima aproximada do terreno em graus.", category: "Mission")]
	protected float m_fMissionMaximumSlopeDegrees;

	//-----------------------------------------------------------------------
	// GETTERS/SETTERS
	//-----------------------------------------------------------------------

	override bool IsHQ() { return false; }
	override bool CountsForVictory() { return m_bCountsForVictory; }

	SPT_WarfareMissionConfig CreateMissionConfig()
	{
		SPT_WarfareMissionConfig config = new SPT_WarfareMissionConfig();
		config.m_bEnabled = m_bMissionEnabled;
		config.m_eType = m_eMissionType;

		config.m_rScenarioPrefab = m_rMissionScenarioPrefab;

		config.m_fActivationDistance = Math.Max(m_fMissionActivationDistance, 1.0);
		config.m_fTerrainSearchRadius = Math.Max(m_fMissionTerrainSearchRadius, 0.0);
		config.m_fMaximumSlopeDegrees = Math.Clamp(m_fMissionMaximumSlopeDegrees, 0.0, 45.0);
		return config;
	}

	SPT_GarrisonLocationConfig CreateGarrisonConfig()
	{
		SPT_GarrisonLocationConfig config = new SPT_GarrisonLocationConfig();
		config.m_fSpawnDistance = m_fGarrisonSpawnDistance;
		config.m_fDespawnDistance = m_fGarrisonDespawnDistance;
		config.m_fDespawnHysteresis = m_fGarrisonDespawnHysteresis;
		config.m_fBuildingSearchRadius = m_fGarrisonBuildingSearchRadius;
		config.m_fPatrolRadius = m_fGarrisonPatrolRadius;
		config.m_iLocationBudget = m_iGarrisonLocationBudget;
		config.m_bEnableCaching = m_bGarrisonEnableCaching;
		config.m_iSpawnIntervalMs = m_iGarrisonSpawnIntervalMs;
		config.m_sPatrolWaypointPrefab = m_sGarrisonPatrolWaypointPrefab;
		config.m_ePatrolFormation = m_eGarrisonPatrolFormation;
		config.m_ePatrolMovementType = m_eGarrisonPatrolMovementType;
		config.m_eBattleMovementType = m_eBattleMovementType;
		config.m_bBattleEnabled = m_bBattleEnabled;
		config.m_iBattleInitialDelayMinMs = m_iBattleInitialDelayMinMs;
		config.m_iBattleInitialDelayMaxMs = m_iBattleInitialDelayMaxMs;
		config.m_iBattleWaveDelayMinMs = m_iBattleWaveDelayMinMs;
		config.m_iBattleWaveDelayMaxMs = m_iBattleWaveDelayMaxMs;
		config.m_iBattleSpawnIntervalMs = m_iBattleSpawnIntervalMs;
		config.m_iBattleWaveUnitsMin = m_iBattleWaveUnitsMin;
		config.m_iBattleWaveUnitsMax = m_iBattleWaveUnitsMax;
		config.m_fBattleWaveAliveThreshold = m_fBattleWaveAliveThreshold;
		config.m_fBattleSpawnDistanceMin = m_fBattleSpawnDistanceMin;
		config.m_fBattleSpawnDistanceMax = m_fBattleSpawnDistanceMax;
		config.m_fBattleConcentratedWeight = m_fBattleConcentratedWeight;
		config.m_fBattleSpreadedWeight = m_fBattleSpreadedWeight;
		config.m_fBattleConvoyWeight = m_fBattleConvoyWeight;
		config.m_fBattleVehicleReinforcementChance = m_fBattleVehicleReinforcementChance;

		if (m_aGarrisonCQBGroupPrefabs)
		{
			foreach (ResourceName cqbPrefab : m_aGarrisonCQBGroupPrefabs)
				config.m_aCQBGroupPrefabs.Insert(cqbPrefab);
		}

		if (m_aGarrisonPatrolGroupPrefabs)
		{
			foreach (ResourceName patrolPrefab : m_aGarrisonPatrolGroupPrefabs)
				config.m_aPatrolGroupPrefabs.Insert(patrolPrefab);
		}

		if (m_aBattleGroupPrefabs)
		{
			foreach (ResourceName battlePrefab : m_aBattleGroupPrefabs)
				config.m_aBattleGroupPrefabs.Insert(battlePrefab);
		}

		if (m_aBattleVehicles)
		{
			foreach (SPT_BattleVehicleConfig battleVehicle : m_aBattleVehicles)
				config.m_aBattleVehicles.Insert(battleVehicle);
		}

		return config;
	}
}
