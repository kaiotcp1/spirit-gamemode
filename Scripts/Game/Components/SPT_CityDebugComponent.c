class SPT_CityDebugComponentClass : ScriptComponentClass
{
}

class SPT_CityDebugComponent : ScriptComponent
{
	[Attribute("0", desc: "Automatically show city hints when the game starts")]
	protected bool m_bShowOnStart;

	protected ref array<string> m_aCityNames;

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (SCR_Global.IsEditMode())
			return;

		if (m_bShowOnStart)
		{
			GetGame().GetCallqueue().CallLater(ShowAllCityNames, 3000, false);
		}
	}

	void ShowAllCityNames()
	{
		m_aCityNames = new array<string>();

		GetGame().GetWorld().QueryEntitiesBySphere(
			"0 0 0",
			99999999,
			ProcessCityEntity,
			FilterCityEntities,
			EQueryEntitiesFlags.STATIC
		);

		int cityCount = m_aCityNames.Count();

		Print(string.Format("[SPT_CityDebug] Found %1 cities", cityCount));

		SCR_HintManagerComponent hintManager = SCR_HintManagerComponent.GetInstance();
		if (!hintManager)
			return;

		if (cityCount == 0)
		{
			hintManager.ShowCustom("No cities found on this map");
			return;
		}

		string hintText = "Cities found: " + cityCount + "\n\n";
		for (int i = 0; i < cityCount; i++)
		{
			hintText = hintText + m_aCityNames[i] + "\n";
		}

		hintManager.ShowCustom(hintText);
	}

	protected bool FilterCityEntities(IEntity entity)
	{
		MapDescriptorComponent mapdesc = MapDescriptorComponent.Cast(entity.FindComponent(MapDescriptorComponent));
		if (!mapdesc)
			return false;

		int type = mapdesc.GetBaseType();
		if (type == EMapDescriptorType.MDT_NAME_CITY)
			return true;
		if (type == EMapDescriptorType.MDT_NAME_VILLAGE)
			return true;
		if (type == EMapDescriptorType.MDT_NAME_TOWN)
			return true;

		return false;
	}

	protected bool ProcessCityEntity(IEntity entity)
	{
		MapDescriptorComponent mapdesc = MapDescriptorComponent.Cast(entity.FindComponent(MapDescriptorComponent));
		if (!mapdesc)
			return true;

		string cityName = mapdesc.Item().GetDisplayName();
		m_aCityNames.Insert(cityName);

		return true;
	}
}
