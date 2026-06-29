class SPT_RoadPath : Managed
{
	int id;
	ref array<vector> points = new array<vector>();
	float length;
	float width;
}

class SPT_RoadNetworkDataset : Managed
{
	ref array<ref SPT_RoadPath> roads = new array<ref SPT_RoadPath>();

	bool Load(string path)
	{
		if (path.IsEmpty())
			return false;

		JsonLoadContext context = new JsonLoadContext();
		if (!context.LoadFromFile(path))
			return false;

		return context.ReadValue("roads", roads) && !roads.IsEmpty();
	}
}

//! Runtime server-only view of the Workbench-generated road dataset.
//! The JSON uses the same top-level "roads" and point arrays as the
//! Freedom dataset, while the SPT runtime intentionally loads only the
//! fields needed to form a convoy.
class SPT_RoadNetworkService
{
	protected static ref SPT_RoadNetworkDataset s_Dataset;

	static bool Load(string path)
	{
		SPT_RoadNetworkDataset dataset = new SPT_RoadNetworkDataset();
		if (!dataset.Load(path))
		{
			s_Dataset = null;
			Print(string.Format("[SPT_RoadNetwork] Dataset indisponivel: %1", path), LogLevel.WARNING);
			return false;
		}

		s_Dataset = dataset;
		Print(string.Format("[SPT_RoadNetwork] Dataset carregado | caminho=%1 | estradas=%2",
			path, dataset.roads.Count()));
		return true;
	}

	static bool IsAvailable()
	{
		return s_Dataset && !s_Dataset.roads.IsEmpty();
	}

	static bool FindConvoyPositions(
		vector target,
		float minimumDistance,
		float maximumDistance,
		int count,
		float spacing,
		out array<vector> positions)
	{
		positions.Clear();
		if (!IsAvailable() || count < 1)
			return false;

		float minimumSq = minimumDistance * minimumDistance;
		float maximumSq = maximumDistance * maximumDistance;
		array<ref SPT_RoadPath> candidates = {};

		foreach (SPT_RoadPath road : s_Dataset.roads)
		{
			if (!road || road.points.Count() < 2 || road.width < 4.0)
				continue;

			foreach (vector point : road.points)
			{
				float distanceSq = vector.DistanceSqXZ(point, target);
				if (distanceSq >= minimumSq && distanceSq <= maximumSq)
				{
					candidates.Insert(road);
					break;
				}
			}
		}

		while (!candidates.IsEmpty())
		{
			int candidateIndex = Math.RandomInt(0, candidates.Count());
			SPT_RoadPath candidate = candidates[candidateIndex];
			candidates.Remove(candidateIndex);
			if (TryBuildFormation(candidate, target, minimumSq, maximumSq, count, spacing, positions))
				return true;
			positions.Clear();
		}

		return false;
	}

	protected static bool TryBuildFormation(
		notnull SPT_RoadPath road,
		vector target,
		float minimumSq,
		float maximumSq,
		int count,
		float spacing,
		out array<vector> positions)
	{
		int startIndex = -1;
		for (int i = 0; i < road.points.Count(); i++)
		{
			float distanceSq = vector.DistanceSqXZ(road.points[i], target);
			if (distanceSq >= minimumSq && distanceSq <= maximumSq)
			{
				startIndex = i;
				break;
			}
		}
		if (startIndex < 0)
			return false;

		int direction = 1;
		if (startIndex + 1 >= road.points.Count())
			direction = -1;

		vector lastPlacement = road.points[startIndex];
		positions.Insert(OffsetToLane(road, startIndex, direction));
		float accumulated;
		int pointIndex = startIndex;

		while (positions.Count() < count)
		{
			int nextIndex = pointIndex + direction;
			if (nextIndex < 0 || nextIndex >= road.points.Count())
				return false;

			accumulated += vector.DistanceXZ(road.points[pointIndex], road.points[nextIndex]);
			pointIndex = nextIndex;
			if (accumulated < spacing)
				continue;

			lastPlacement = road.points[pointIndex];
			positions.Insert(OffsetToLane(road, pointIndex, direction));
			accumulated = 0;
		}

		return true;
	}

	protected static vector OffsetToLane(notnull SPT_RoadPath road, int pointIndex, int direction)
	{
		int lookIndex = pointIndex + direction;
		if (lookIndex < 0 || lookIndex >= road.points.Count())
			lookIndex = pointIndex - direction;

		vector forward = road.points[lookIndex] - road.points[pointIndex];
		forward = forward.Normalized();
		vector right = Vector(-forward[2], 0, forward[0]);
		vector result = road.points[pointIndex] + right * road.width * 0.25;
		result[1] += 0.3;
		return result;
	}
}
