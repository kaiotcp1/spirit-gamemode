// version date 06-14-26
// SPT AI Garrison


modded class SCR_DestructibleBuildingComponent
{
	// pull the protected box list up to where the detector can get at it
	array<ref SCR_InteriorBoundingBox> SPT_GetInteriorBoxes()
	{
		return GetInteriorBoundingBoxes();
	}
}
