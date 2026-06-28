// data da versao 06-14-26
// SPT AI Garrison – bloqueio de fumaca para grupos guarnecidos

// Impede que grupos guarnecidos utilizem cobertura de fumaca, pois eles devem permanecer
// estaticos no interior das construcoes. Grupos nao-guarnecidos seguem o comportamento padrao.
modded class SCR_AIActivitySmokeCoverFeature
{
	// Verifica se o grupo esta guarnecido; se estiver, cancela a execucao da fumaca
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
