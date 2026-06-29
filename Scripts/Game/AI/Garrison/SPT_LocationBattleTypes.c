enum SPT_EDeploymentStrategy
{
	CONCENTRATED,
	SPREADED,
	CONVOY
}

[BaseContainerProps()]
class SPT_BattleVehicleConfig
{
	[Attribute("", UIWidgets.ResourceNamePicker, "Prefab do veiculo usado pelo comboio.", "et")]
	ResourceName m_rVehiclePrefab;

	[Attribute("", UIWidgets.ResourceNamePicker, "Prefab do grupo que fornece motorista, tripulantes e passageiros.", "et class=SCR_AIGroup")]
	ResourceName m_rCrewGroupPrefab;
}

class SPT_BattleWave : Managed
{
	int m_iUnitBudget;
	int m_iDeployedUnits;
	int m_iPendingRequests;
	bool m_bDeploymentFinished;
	SPT_EDeploymentStrategy m_eStrategy;
}

class SPT_LocationBattleState : Managed
{
	bool m_bActive;
	bool m_bCancelled;
	int m_iTimerMs;
	int m_iCurrentWave = -1;
	int m_iGeneration;
	ref array<ref SPT_BattleWave> m_aWaves = new array<ref SPT_BattleWave>();
	ref array<EntityID> m_aVehicleIds = new array<EntityID>();

	void Reset()
	{
		m_bActive = false;
		m_bCancelled = false;
		m_iTimerMs = 0;
		m_iCurrentWave = -1;
		m_iGeneration++;
		m_aWaves.Clear();
		m_aVehicleIds.Clear();
	}
}
