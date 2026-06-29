#ifdef WORKBENCH

class SPT_RoadSourcePoint : Managed
{
	vector m_vPosition;
	vector m_vTangentIn;
	vector m_vTangentOut;
}

//! Workbench-only generator for the compact SPT road dataset. It intentionally
//! serializes only road width and world spline points required at runtime.
class SPT_RoadNetworkGenerator
{
	protected string m_sOutputPath;
	protected WorldEditorAPI m_API;
	protected ref array<IEntity> m_aRoadEntities = new array<IEntity>();
	protected ref array<ref SPT_RoadPath> m_aRoads = new array<ref SPT_RoadPath>();

	void SetOutputPath(string outputPath)
	{
		m_sOutputPath = outputPath;
	}

	bool Run()
	{
		WorldEditor editor = Workbench.GetModule(WorldEditor);
		if (!editor || m_sOutputPath.IsEmpty())
			return false;

		m_API = editor.GetApi();
		BaseWorld world = m_API.GetWorld();
		vector minimum;
		vector maximum;
		world.GetBoundBox(minimum, maximum);
		world.QueryEntitiesByAABB(minimum, maximum, ScanRoad);

		foreach (IEntity entity : m_aRoadEntities)
			LoadRoad(entity);

		if (m_aRoads.IsEmpty())
		{
			Print("[SPT_RoadNetwork] Nenhuma estrada valida foi encontrada.", LogLevel.ERROR);
			return false;
		}

		JsonSaveContext context = new JsonSaveContext();
		context.WriteValue("roads", m_aRoads);
		bool saved = context.SaveToFile(m_sOutputPath);
		Print(string.Format("[SPT_RoadNetwork] Geracao concluida | estradas=%1 | caminho=%2 | salvo=%3",
			m_aRoads.Count(), m_sOutputPath, saved));
		return saved;
	}

	protected bool ScanRoad(IEntity entity)
	{
		if (RoadEntity.Cast(entity))
			m_aRoadEntities.Insert(entity);
		return true;
	}

	protected void LoadRoad(notnull IEntity entity)
	{
		IEntitySource source = m_API.EntityToSource(entity);
		if (!source || !source.IsVariableSet("SplinePoints"))
			return;

		float width = 1.0;
		string widthText;
		source.Get("Width", widthText);
		if (!widthText.IsEmpty())
			width = widthText.ToFloat();

		BaseContainerList pointContainers = source.GetObjectArray("SplinePoints");
		if (!pointContainers || pointContainers.Count() < 2)
			return;

		vector transform[4];
		entity.GetWorldTransform(transform);
		array<ref SPT_RoadSourcePoint> sourcePoints = {};
		for (int sourceIndex = 0; sourceIndex < pointContainers.Count(); sourceIndex++)
		{
			BaseContainer pointContainer = pointContainers.Get(sourceIndex);
			SPT_RoadSourcePoint sourcePoint = new SPT_RoadSourcePoint();
			vector localPosition;
			pointContainer.Get("Position", localPosition);
			sourcePoint.m_vPosition = localPosition.Multiply4(transform);

			if (pointContainer.IsVariableSet("Data"))
			{
				BaseContainerList data = pointContainer.GetObjectArray("Data");
				for (int dataIndex = 0; dataIndex < data.Count(); dataIndex++)
				{
					BaseContainer entry = data.Get(dataIndex);
					if (entry.GetClassName() != "SplinePointData")
						continue;
					entry.Get("InTangent", sourcePoint.m_vTangentIn);
					entry.Get("OutTangent", sourcePoint.m_vTangentOut);
				}
			}

			sourcePoint.m_vTangentIn *= -0.35;
			sourcePoint.m_vTangentOut *= 0.35;
			sourcePoints.Insert(sourcePoint);
		}

		SPT_RoadPath road = new SPT_RoadPath();
		road.id = m_aRoads.Count();
		road.width = width;
		vector lastInserted;
		for (int segment = 1; segment < sourcePoints.Count(); segment++)
		{
			SPT_RoadSourcePoint a = sourcePoints[segment - 1];
			SPT_RoadSourcePoint b = sourcePoints[segment];
			for (int sample = 0; sample <= 50; sample++)
			{
				if (segment > 1 && sample == 0)
					continue;
				float t = sample / 50.0;
				vector worldPosition = CubicBezier(
					a.m_vPosition,
					a.m_vPosition + a.m_vTangentOut,
					b.m_vPosition + b.m_vTangentIn,
					b.m_vPosition,
					t);
				worldPosition[1] = m_API.GetWorld().GetSurfaceY(worldPosition[0], worldPosition[2]);
				if (!road.points.IsEmpty() && sample < 50 && vector.DistanceXZ(lastInserted, worldPosition) < 10.0)
					continue;
				if (!road.points.IsEmpty())
					road.length += vector.DistanceXZ(lastInserted, worldPosition);
				road.points.Insert(worldPosition);
				lastInserted = worldPosition;
			}
		}

		if (road.points.Count() >= 2)
			m_aRoads.Insert(road);
	}

	protected vector CubicBezier(vector a, vector controlA, vector controlB, vector b, float t)
	{
		float inverse = 1.0 - t;
		return a * inverse * inverse * inverse
			+ controlA * 3.0 * inverse * inverse * t
			+ controlB * 3.0 * inverse * t * t
			+ b * t * t * t;
	}
}

[WorkbenchToolAttribute(
	category: "SPT",
	name: "SPT Road Network",
	description: "Gera o dataset viario usado por ondas de comboio.",
	awesomeFontCode: 0xF63C
)]
class SPT_RoadNetworkTool : WorldEditorTool
{
	[Attribute("$profile:/SPT_RoadNetwork.json", desc: "Caminho de saida do dataset.", category: "Road network")]
	protected string m_sOutputPath;

	[ButtonAttribute("Generate network")]
	protected void GenerateNetwork()
	{
		SPT_RoadNetworkGenerator generator = new SPT_RoadNetworkGenerator();
		generator.SetOutputPath(m_sOutputPath);
		generator.Run();
	}
}

#endif
