// version date 06-13-26
// SPT AI Garrison


class SPT_AIGarrisonHelper
{
	static const float DEFAULT_RADIUS_M = 50.0;

	static void GarrisonGroup(SCR_AIGroup group, vector center, float radius)
	{
		if (!group)
			return;
		SPT_GarrisonManager.Get().Garrison(group, center, radius);
	}

	static void UngarrisonGroup(SCR_AIGroup group)
	{
		if (!group)
			return;
		SPT_GarrisonManager.Get().Ungarrison(group);
	}
}
