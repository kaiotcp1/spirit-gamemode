// version date 06-14-26
// SPT AI Garrison


modded class SCR_AIActivitySmokeCoverFeature
{
	override bool Execute(
		notnull SCR_AIGroupUtilityComponent groupUtility,
		vector targetPosition,
		SCR_AIActivitySmokeCoverFeatureProperties smokeCoverProperties,
		notnull array<AIAgent> avoidAgents,
		notnull array<AIAgent> excludeAgents,
		int maxPositionCount = 1,
		SCR_AIActivityBase contextActivity = null)
	{
		SCR_AIGroup group = groupUtility.m_Owner;
		if (group && SPT_GarrisonManager.Get().IsGroupGarrisoned(group))
			return false;

		return super.Execute(groupUtility, targetPosition, smokeCoverProperties, avoidAgents, excludeAgents, maxPositionCount, contextActivity);
	}
}
