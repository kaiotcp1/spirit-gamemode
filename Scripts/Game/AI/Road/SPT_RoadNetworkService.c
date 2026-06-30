class SPT_RoadPath : Managed
{
	int id;
	ref array<vector> points = new array<vector>();
	float length;
	float width;
}

//! Runtime server-only access to the engine road network.
//! The Workbench JSON generator remains available for diagnostics, but convoy
//! placement queries BaseRoad data directly and has no external file dependency.
class SPT_RoadNetworkService
{
	protected static RoadNetworkManager GetRoadNetwork()
	{
		ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld)
			return null;
		return aiWorld.GetRoadNetworkManager();
	}

	static bool IsAvailable()
	{
		return GetRoadNetwork() != null;
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
		if (count < 1 || maximumDistance < 0)
			return false;

		RoadNetworkManager roadNetwork = GetRoadNetwork();
		if (!roadNetwork)
			return false;

		float minimumSq = minimumDistance * minimumDistance;
		float maximumSq = maximumDistance * maximumDistance;
		vector boundsMinimum = target - Vector(maximumDistance, 10000.0, maximumDistance);
		vector boundsMaximum = target + Vector(maximumDistance, 10000.0, maximumDistance);
		array<BaseRoad> candidates = {};
		roadNetwork.GetRoadsInAABB(boundsMinimum, boundsMaximum, candidates);

		while (!candidates.IsEmpty())
		{
			int candidateIndex = Math.RandomInt(0, candidates.Count());
			BaseRoad candidate = candidates[candidateIndex];
			candidates.Remove(candidateIndex);
			if (!candidate || candidate.GetWidth() < 4.0)
				continue;

			array<vector> roadPoints = {};
			candidate.GetPoints(roadPoints);
			if (roadPoints.Count() < 2)
				continue;

			if (TryBuildFormation(
				roadPoints,
				candidate.GetWidth(),
				target,
				minimumSq,
				maximumSq,
				count,
				spacing,
				positions))
				return true;
			positions.Clear();
		}

		return false;
	}

	protected static bool TryBuildFormation(
		notnull array<vector> roadPoints,
		float roadWidth,
		vector target,
		float minimumSq,
		float maximumSq,
		int count,
		float spacing,
		out array<vector> positions)
	{
		int startIndex = -1;
		for (int i = 0; i < roadPoints.Count(); i++)
		{
			float distanceSq = vector.DistanceSqXZ(roadPoints[i], target);
			if (distanceSq >= minimumSq && distanceSq <= maximumSq)
			{
				startIndex = i;
				break;
			}
		}
		if (startIndex < 0)
			return false;

		int direction = 1;
		if (startIndex + 1 >= roadPoints.Count())
			direction = -1;

		positions.Insert(OffsetToLane(roadPoints, roadWidth, startIndex, direction));
		float accumulated;
		int pointIndex = startIndex;

		while (positions.Count() < count)
		{
			int nextIndex = pointIndex + direction;
			if (nextIndex < 0 || nextIndex >= roadPoints.Count())
				return false;

			accumulated += vector.DistanceXZ(roadPoints[pointIndex], roadPoints[nextIndex]);
			pointIndex = nextIndex;
			if (accumulated < spacing)
				continue;

			positions.Insert(OffsetToLane(roadPoints, roadWidth, pointIndex, direction));
			accumulated = 0;
		}

		return true;
	}

	protected static vector OffsetToLane(
		notnull array<vector> roadPoints,
		float roadWidth,
		int pointIndex,
		int direction)
	{
		int lookIndex = pointIndex + direction;
		if (lookIndex < 0 || lookIndex >= roadPoints.Count())
			lookIndex = pointIndex - direction;

		vector forward = roadPoints[lookIndex] - roadPoints[pointIndex];
		forward = forward.Normalized();
		vector right = Vector(-forward[2], 0, forward[0]);
		vector result = roadPoints[pointIndex] + right * roadWidth * 0.25;
		result[1] = result[1] + 0.3;
		return result;
	}
}
