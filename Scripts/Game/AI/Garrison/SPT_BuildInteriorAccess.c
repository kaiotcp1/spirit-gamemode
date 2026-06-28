// data da versao 06-14-26
// SPT AI Garrison – acesso interno para o detector de construcoes

// Expande SCR_DestructibleBuildingComponent para expor as caixas delimitadoras internas (interior boxes),
// permitindo que o SPT_GarrisonDetector as consulte diretamente
modded class SCR_DestructibleBuildingComponent
{
	// Exposicao publica da lista protegida de caixas internas para consumo do detector
	array<ref SCR_InteriorBoundingBox> SPT_GetInteriorBoxes()
	{
		return GetInteriorBoundingBoxes();
	}
}
