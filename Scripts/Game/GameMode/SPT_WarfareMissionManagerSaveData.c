[BaseContainerProps()]
class SPT_WarfareGameModeSaveDataClass : EPF_EntitySaveDataClass
{
}

[EDF_DbName("SPT_WarfareGameMode")]
class SPT_WarfareGameModeSaveData : EPF_EntitySaveData
{
}

[EPF_ComponentSaveDataType(SPT_WarfareMissionManagerComponent), BaseContainerProps()]
class SPT_WarfareMissionManagerSaveDataClass : EPF_ComponentSaveDataClass
{
}

[EDF_DbName("SPT_WarfareMissionManager")]
class SPT_WarfareMissionManagerSaveData : EPF_ComponentSaveData
{
	ref array<string> m_aCompletedPointIds;

	override EPF_EReadResult ReadFrom(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
	{
		SPT_WarfareMissionManagerComponent manager = SPT_WarfareMissionManagerComponent.Cast(component);
		if (!manager)
			return EPF_EReadResult.ERROR;

		m_aCompletedPointIds = new array<string>();
		manager.CopyCompletedPointIds(m_aCompletedPointIds);
		return EPF_EReadResult.OK;
	}

	override EPF_EApplyResult ApplyTo(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
	{
		SPT_WarfareMissionManagerComponent manager = SPT_WarfareMissionManagerComponent.Cast(component);
		if (!manager)
			return EPF_EApplyResult.ERROR;

		manager.RestoreCompletedPointIds(m_aCompletedPointIds);
		return EPF_EApplyResult.OK;
	}

	override bool Equals(notnull EPF_ComponentSaveData other)
	{
		SPT_WarfareMissionManagerSaveData otherData = SPT_WarfareMissionManagerSaveData.Cast(other);
		if (!otherData || !m_aCompletedPointIds || !otherData.m_aCompletedPointIds)
			return false;
		if (m_aCompletedPointIds.Count() != otherData.m_aCompletedPointIds.Count())
			return false;

		foreach (string pointId : m_aCompletedPointIds)
		{
			if (otherData.m_aCompletedPointIds.Find(pointId) < 0)
				return false;
		}

		return true;
	}
}
