// data da versao 06-13-26
// SPT AI Garrison – auxiliar estatico para guarnecer/desguarnecer grupos de IA

// Ponte estatica simples para o SPT_GarrisonManager, usada por acoes de usuario e scripts externos
class SPT_AIGarrisonHelper
{
	static const float DEFAULT_RADIUS_M = 50.0;

	// Encaminha um pedido de guarnicao de grupo para o gerenciador central
	static void GarrisonGroup(
		SCR_AIGroup group,
		vector center,
		float radius,
		ResourceName patrolWaypointPrefab = ResourceName.Empty)
	{
		if (!group)
			return;
		SPT_GarrisonManager.Get().Garrison(group, center, radius, patrolWaypointPrefab);
	}

	// Encaminha um pedido de liberacao de grupo (desguarnecer) para o gerenciador central
	static void UngarrisonGroup(SCR_AIGroup group)
	{
		if (!group)
			return;
		SPT_GarrisonManager.Get().Ungarrison(group);
	}
}
