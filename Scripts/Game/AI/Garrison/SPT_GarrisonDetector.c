// version date 06-14-26
// SPT AI Garrison






// a finished spot to put a soldier, where to stand and which way to look
class SPT_GarrisonPost
{
	vector m_Pos;
	vector m_Facing;

	void SPT_GarrisonPost(vector pos, vector facing)
	{
		m_Pos = pos;
		m_Facing = facing;
	}
}

// something worth watching; exterior ones face outside, route ones are internal approaches
class SPT_GarrisonOpening
{
	vector m_Pos;
	vector m_Normal;
	bool m_Exterior;
	bool m_Route;

	void SPT_GarrisonOpening(vector pos, vector normal)
	{
		m_Pos = pos;
		m_Normal = normal;
		m_Exterior = false;
		m_Route = false;
	}
}

class SPT_GarrisonDetector
{
	protected const float BUILDING_SEARCH_RADIUS = 25.0;
	protected const float GRID_SPACING_M         = 1.5;
	protected const float Y_SLICE_SPACING_M      = 2.5;
	protected const float FLOOR_OFFSET_M         = 1.0;
	protected const vector NAVMESH_SEARCH_HALF   = "2.5 1.5 2.5";
	protected const float  SNAP_DEDUP_SQ         = 0.25;
	protected const float ROOF_PROBE_HEIGHT_M    = 50.0;
	protected const float ROOF_PROBE_START_M     = 0.3;
	protected const float DOOR_CLEARANCE_M       = 2.0;
	protected const float OCCUPIED_CLEARANCE_M   = 2.0;
	protected const float DOOR_FLOOR_TOL_M       = 2.0;
	protected const float CONNECT_DIST_M         = 2.2;
	protected const float CONNECT_Y_M            = 3.5;
	protected const float FLOOR_WEIGHT           = 2.5;
	protected const int   FPS_JITTER_K           = 3;
	protected const float MIN_POST_SPACING_M     = 1.8;

	protected const float LOS_H_M                = 1.5;
	protected const float LOS_STOP_SHORT_M       = 0.7;
	protected const float EXT_RAY_M              = 3.0;
	protected const float MAX_WATCH_M            = 25.0;
	protected const float W_EXT                  = 1.0;
	protected const float W_APPROACH             = 0.6;
	protected const float W_NEAR                 = 0.4;
	protected const float W_ROUTE                = 0.65;

	protected const float ROUTE_HDIST_M          = 2.0;
	protected const float ROUTE_HDIST_MIN_M      = 0.6;
	protected const float ROUTE_STEP_MIN_M       = 0.4;
	protected const float ROUTE_STEP_MAX_M       = 2.5;
	protected const float ROUTE_DEDUP_M          = 4.5;
	protected const float ROUTE_CLAIM_M          = 3.0;

	protected const float UNDERGROUND_SLOP_M     = 0.5;
	protected const float FLOOR_PROBE_M          = 0.6;
	protected const float MAX_FLOOR_GAP_M        = 0.35;
	protected const float MIN_HEADROOM_M         = 1.8;
	protected const float CLEAR_RADIUS_M         = 0.4;
	protected const float CHEST_H_M              = 1.2;
	protected const float ENCLOSE_RAY_M          = 16.0;
	protected const float ENCLOSE_H_M            = 1.5;
	protected const int   ENCLOSE_MIN_HITS       = 4;
	// slack on the authored interior box so a floor sample right at the box edge still counts as inside
	protected const float INTERIOR_MARGIN_M      = 0.3;

	protected const bool  USE_INTERIOR_OBB_GATE  = false;

	protected static ref array<IEntity> s_QueryAccumulator;

	static ref array<ref SPT_GarrisonOpening> s_DebugOpenings;
	static ref array<vector> s_DebugContained;
	static string s_DebugSummary;

	// Exposes the detector's building discovery to dynamic world garrisons.
	// Results are structural-shell centers with duplicates removed.
	static void CollectBuildingCenters(
		BaseWorld world,
		vector center,
		float radius,
		out array<vector> outCenters)
	{
		outCenters = {};
		if (!world || radius <= 0)
			return;

		array<IEntity> buildings = {};
		s_QueryAccumulator = buildings;
		world.QueryEntitiesBySphere(center, radius, BuildingFilterCallback, null, EQueryEntitiesFlags.STATIC);
		s_QueryAccumulator = null;

		foreach (IEntity building : buildings)
		{
			IEntity shell = ResolveStructuralShell(building);
			if (!shell)
				continue;

			vector mins, maxs;
			shell.GetWorldBounds(mins, maxs);
			vector shellCenter = (mins + maxs) * 0.5;

			bool duplicate = false;
			foreach (vector existingCenter : outCenters)
			{
				if (HorizontalDistSq(existingCenter, shellCenter) < 4.0)
				{
					duplicate = true;
					break;
				}
			}

			if (!duplicate)
				outCenters.Insert(shellCenter);
		}
	}

	// the whole pipeline: picks & distribs
	static SPT_EGarrisonDetectResult DetectPosts(
		BaseWorld world,
		vector center,
		AIPathfindingComponent pathing,
		int desiredCount,
		notnull array<vector> excluded,
		out array<ref SPT_GarrisonPost> outPosts)
	{
		outPosts = {};

		if (!world || !pathing)
			return SPT_EGarrisonDetectResult.NO_BUILDING;

		array<IEntity> buildings = {};
		FindBuildings(world, center, buildings);
		if (buildings.IsEmpty())
			return SPT_EGarrisonDetectResult.NO_BUILDING;

		IEntity target = ResolveStructuralShell(NearestBuilding(buildings, center));
		if (!target)
			return SPT_EGarrisonDetectResult.NO_BUILDING;

		vector mins, maxs;
		target.GetWorldBounds(mins, maxs);

		array<vector> snapped = {};
		SampleNavmesh(pathing, mins, maxs, snapped);
		if (snapped.IsEmpty())
			return SPT_EGarrisonDetectResult.NAVMESH_NOT_READY;

		// keep only the spots that are actually inside, on a floor, and have room to stand
		array<vector> valid = {};
		foreach (vector p : snapped)
		{
			if (USE_INTERIOR_OBB_GATE && !IsInsideInteriorVolume(target, p))
				continue;
			if (!IsUnderRoof(world, target, p))
				continue;
			if (IsUnderground(world, p))
				continue;
			if (!HasClearStandingSpace(world, target, p))
				continue;
			if (!HasWallsAround(world, target, p))
				continue;
			valid.Insert(p);
		}
		if (valid.IsEmpty())
			return SPT_EGarrisonDetectResult.NO_POSITIONS;

		array<vector> doors = {};
		CollectBuildingDoors(target, doors);
		vector buildingCenter = (mins + maxs) * 0.5;

		// drop anything on a floor with no door near its height, probably an unreachable level
		array<vector> reachable = {};
		FilterToDoorLevels(valid, doors, reachable);

		// keep only the cluster connected to the click, so a back shed across the yard isn't garrisoned
		array<vector> contained = {};
		KeepCenterComponent(world, target, reachable, center, contained);

		// don't park anyone right in a doorway, and don't reuse already-occupied spots
		array<vector> spaced = {};
		FilterByClearance(contained, doors, DOOR_CLEARANCE_M, spaced);

		array<vector> available = {};
		FilterByClearance(spaced, excluded, OCCUPIED_CLEARANCE_M, available);
		if (available.IsEmpty())
			return SPT_EGarrisonDetectResult.NO_POSITIONS;

		// spread the chosen posts as far apart as we can so they cover the place, not huddle
		array<vector> positions = {};
		FarthestPointSample(available, buildingCenter, desiredCount, positions);
		if (positions.IsEmpty())
			return SPT_EGarrisonDetectResult.NO_POSITIONS;

		// work out what each man should watch, doors, windows, stairs, and the likely approach
		array<ref SPT_GarrisonOpening> openings = {};
		CollectOpenings(world, target, buildingCenter, openings);
		DetectRoutes(contained, openings);
		vector approach = ApproachDir(openings);

		s_DebugOpenings = openings;
		s_DebugContained = contained;

		array<vector> facings = {};
		AssignFacings(world, positions, openings, buildingCenter, approach, facings);

		for (int i = 0; i < positions.Count(); i++)
			outPosts.Insert(new SPT_GarrisonPost(positions[i], facings[i]));

		return SPT_EGarrisonDetectResult.OK;
	}

	// just the building's bounding box, used to know which navmesh tiles to wait on
	static bool ResolveBuildingBounds(BaseWorld world, vector center, out vector mins, out vector maxs)
	{
		mins = vector.Zero;
		maxs = vector.Zero;
		if (!world)
			return false;

		array<IEntity> buildings = {};
		FindBuildings(world, center, buildings);
		if (buildings.IsEmpty())
			return false;

		IEntity target = ResolveStructuralShell(NearestBuilding(buildings, center));
		if (!target)
			return false;

		target.GetWorldBounds(mins, maxs);
		return true;
	}

	// every destructible structure within reach of the click
	protected static void FindBuildings(BaseWorld world, vector origin, out array<IEntity> result)
	{
		s_QueryAccumulator = result;
		world.QueryEntitiesBySphere(origin, BUILDING_SEARCH_RADIUS, BuildingFilterCallback, null, EQueryEntitiesFlags.STATIC);
		s_QueryAccumulator = null;
	}

	protected static bool BuildingFilterCallback(IEntity entity)
	{
		if (!entity)
			return true;
		SCR_DestructibleBuildingComponent comp = SCR_DestructibleBuildingComponent.Cast(
			entity.FindComponent(SCR_DestructibleBuildingComponent));
		if (comp && s_QueryAccumulator)
			s_QueryAccumulator.Insert(entity);
		return true;
	}

	protected static IEntity NearestBuilding(notnull array<IEntity> buildings, vector origin)
	{
		IEntity nearest;
		float nearestDist = float.MAX;
		foreach (IEntity b : buildings)
		{
			if (!b)
				continue;
			float d = vector.Distance(origin, b.GetOrigin());
			if (d < nearestDist)
			{
				nearestDist = d;
				nearest = b;
			}
		}
		return nearest;
	}

	// we may have grabbed a chair or a prop, climb to the actual walls-and-roof piece
	protected static IEntity ResolveStructuralShell(IEntity picked)
	{
		if (!picked)
			return null;

		IEntity root = picked.GetRootParent();
		if (!root)
			root = picked;

		if (IsFurniturePiece(root) || !HasOwnGeometry(root))
		{
			IEntity better = FindShellChild(root);
			if (better)
				return better;
		}

		return root;
	}

	// crude name check, furniture isn't the shell we want to garrison into
	protected static bool IsFurniturePiece(IEntity entity)
	{
		if (!entity)
			return false;
		EntityPrefabData data = entity.GetPrefabData();
		if (!data)
			return false;
		string path = data.GetPrefabName();
		return path.Contains("/Furniture/") || path.Contains("_Furniture");
	}

	protected static bool HasOwnGeometry(IEntity entity)
	{
		if (!entity)
			return false;
		return entity.GetVObject() != null && entity.GetPhysics() != null;
	}

	protected static IEntity FindShellChild(IEntity root)
	{
		if (!root)
			return null;
		IEntity child = root.GetChildren();
		while (child)
		{
			SCR_DestructibleBuildingComponent comp = SCR_DestructibleBuildingComponent.Cast(
				child.FindComponent(SCR_DestructibleBuildingComponent));
			if (comp && !IsFurniturePiece(child) && HasOwnGeometry(child))
				return child;
			child = child.GetSibling();
		}
		return null;
	}

	// grid the building at a few floor heights and snap each point to navmesh, so only standable spots
	protected static void SampleNavmesh(AIPathfindingComponent pathing, vector mins, vector maxs, out array<vector> snapped)
	{
		snapped = {};

		float xSize = maxs[0] - mins[0];
		float zSize = maxs[2] - mins[2];
		float ySize = maxs[1] - mins[1];
		int xSteps = Math.Floor(xSize / GRID_SPACING_M);
		int zSteps = Math.Floor(zSize / GRID_SPACING_M);

		int floorCount = Math.Floor(ySize / Y_SLICE_SPACING_M);
		if (floorCount < 1)
			floorCount = 1;

		for (int yi = 0; yi < floorCount; yi++)
		{
			float sliceY = mins[1] + yi * Y_SLICE_SPACING_M + FLOOR_OFFSET_M;
			if (sliceY >= maxs[1])
				continue;

			for (int xi = 0; xi <= xSteps; xi++)
			{
				for (int zi = 0; zi <= zSteps; zi++)
				{
					vector gridPt = Vector(
						mins[0] + xi * GRID_SPACING_M,
						sliceY,
						mins[2] + zi * GRID_SPACING_M);

					vector snappedPt;
					if (!pathing.GetClosestPositionOnNavmesh(gridPt, NAVMESH_SEARCH_HALF, snappedPt))
						continue;

					bool dup = false;
					foreach (vector s : snapped)
					{
						if (vector.DistanceSq(s, snappedPt) < SNAP_DEDUP_SQ)
						{
							dup = true;
							break;
						}
					}
					if (!dup)
						snapped.Insert(snappedPt);
				}
			}
		}
	}

	// fire a ray straight up, if it hits something there's a roof over this spot
	protected static bool IsUnderRoof(BaseWorld world, IEntity building, vector point)
	{
		TraceParam p = new TraceParam();
		p.Start = point + Vector(0, ROOF_PROBE_START_M, 0);
		p.End = p.Start + Vector(0, ROOF_PROBE_HEIGHT_M, 0);
		p.Flags = TraceFlags.ENTS;
		p.LayerMask = EPhysicsLayerDefs.Projectile;

		float ratio = world.TraceMove(p, null);
		return ratio < 0.99;
	}

	// reject spots under terrain, with no floor beneath, or no headroom; keeps us out of cellars
	protected static bool IsUnderground(BaseWorld world, vector point)
	{
		float terrainY = world.GetSurfaceY(point[0], point[2]);
		if (point[1] < terrainY - UNDERGROUND_SLOP_M)
			return true;

		vector floorHit;
		vector downStart = point + Vector(0, 0.1, 0);
		float fDown = TraceSeg(world, downStart, downStart - Vector(0, FLOOR_PROBE_M + 0.1, 0), floorHit);
		if (fDown >= 1.0)
			return true;
		if ((point[1] - floorHit[1]) > MAX_FLOOR_GAP_M)
			return true;

		vector ceilHit;
		vector upStart = floorHit + Vector(0, 0.05, 0);
		float fUp = TraceSeg(world, upStart, upStart + Vector(0, MIN_HEADROOM_M, 0), ceilHit);
		if (fUp < 1.0)
			return true;

		return false;
	}

	// feel around chest height in 8 directions, make sure he isn't jammed into a wall corner
	protected static bool HasClearStandingSpace(BaseWorld world, IEntity shell, vector point)
	{
		vector chest = point + Vector(0, CHEST_H_M, 0);
		float step = Math.PI * 2.0 / 8.0;
		for (int i = 0; i < 8; i++)
		{
			float a = i * step;
			vector dir = Vector(Math.Cos(a), 0, Math.Sin(a));

			TraceParam p = new TraceParam();
			p.Start = chest;
			p.End = chest + dir * CLEAR_RADIUS_M;
			p.Flags = TraceFlags.ENTS;
			p.LayerMask = EPhysicsLayerDefs.Projectile;
			p.Exclude = shell;

			if (world.TraceMove(p, null) < 0.99)
				return false;
		}
		return true;
	}

	// enclosed by THIS building? only this building's own walls count, not the neighbours' (kills exterior-wall posts in dense villages)
	protected static bool HasWallsAround(BaseWorld world, IEntity building, vector point)
	{
		IEntity buildingRoot = building.GetRootParent();
		if (!buildingRoot)
			buildingRoot = building;

		vector eye = point + Vector(0, ENCLOSE_H_M, 0);
		float step = Math.PI * 2.0 / 8.0;
		int hits = 0;
		for (int i = 0; i < 8; i++)
		{
			float a = i * step;
			vector dir = Vector(Math.Cos(a), 0, Math.Sin(a));

			TraceParam p = new TraceParam();
			p.Start = eye;
			p.End = eye + dir * ENCLOSE_RAY_M;
			p.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
			p.LayerMask = EPhysicsLayerDefs.Projectile;
			float frac = world.TraceMove(p, null);
			if (frac >= 1.0)
				continue;

			// only this building's own walls count, neighbour walls don't enclose us
			if (IsPartOfBuilding(p.TraceEnt, buildingRoot))
				hits++;
		}
		return hits >= ENCLOSE_MIN_HITS;
	}

	// true if the hit entity belongs to the target building (shares its root)
	protected static bool IsPartOfBuilding(IEntity hit, IEntity buildingRoot)
	{
		if (!hit || !buildingRoot)
			return false;
		IEntity hitRoot = hit.GetRootParent();
		if (!hitRoot)
			hitRoot = hit;
		return hitRoot == buildingRoot;
	}

	// test the point against the building's authored interior boxes (local frame); no boxes returns TRUE so the ray heuristics stay in charge
	protected static bool IsInsideInteriorVolume(IEntity shell, vector point)
	{
		SCR_DestructibleBuildingComponent comp = SCR_DestructibleBuildingComponent.Cast(
			shell.FindComponent(SCR_DestructibleBuildingComponent));
		if (!comp)
			return true;

		array<ref SCR_InteriorBoundingBox> boxes = comp.SPT_GetInteriorBoxes();
		if (!boxes || boxes.IsEmpty())
			return true;

		vector mat[4];
		shell.GetWorldTransform(mat);
		vector local = point.InvMultiply4(mat);

		foreach (SCR_InteriorBoundingBox box : boxes)
		{
			if (!box)
				continue;
			vector center;
			if (!box.GetCenter(center))
				continue;
			vector half = box.GetScale() * 0.5;
			vector mn = center - half;
			vector mx = center + half;
			float m = INTERIOR_MARGIN_M;
			if (local[0] >= mn[0] - m && local[0] <= mx[0] + m
				&& local[1] >= mn[1] - m && local[1] <= mx[1] + m
				&& local[2] >= mn[2] - m && local[2] <= mx[2] + m)
				return true;
		}
		return false;
	}

	// one ray; gives back how far it got and where it stopped
	protected static float TraceSeg(BaseWorld world, vector from, vector to, out vector outHit)
	{
		TraceParam p = new TraceParam();
		p.Start = from;
		p.End = to;
		p.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		p.LayerMask = EPhysicsLayerDefs.Projectile;
		float frac = world.TraceMove(p, null);
		outHit = vector.Lerp(from, to, frac);
		return frac;
	}

	// every door position in the building, used as floor-level anchors and to avoid blocking them
	protected static void CollectBuildingDoors(IEntity target, out array<vector> outDoors)
	{
		outDoors = {};
		IEntity root = target.GetRootParent();
		if (!root)
			root = target;
		WalkForDoors(root, outDoors, 0);
	}

	protected static void WalkForDoors(IEntity entity, notnull array<vector> outDoors, int depth)
	{
		if (!entity || depth > 10)
			return;
		BaseDoorComponent door = BaseDoorComponent.Cast(entity.FindComponent(BaseDoorComponent));
		if (door)
		{
			outDoors.Insert(entity.GetOrigin());
		}
		IEntity child = entity.GetChildren();
		while (child)
		{
			WalkForDoors(child, outDoors, depth + 1);
			child = child.GetSibling();
		}
	}

	// drop points too close to a marker (door or taken spot); if that empties all, keep them all instead
	protected static void FilterByClearance(notnull array<vector> points, notnull array<vector> markers, float clearance, out array<vector> outKept)
	{
		outKept = {};
		if (markers.IsEmpty())
		{
			outKept.Copy(points);
			return;
		}

		float clr2 = clearance * clearance;
		foreach (vector p : points)
		{
			bool clear = true;
			foreach (vector m : markers)
			{
				if (HorizontalDistSq(p, m) < clr2)
				{
					clear = false;
					break;
				}
			}
			if (clear)
				outKept.Insert(p);
		}

		if (outKept.IsEmpty())
			outKept.Copy(points);
	}

	// only trust floors that have a door near their height, keeps us off unreachable mezzanines
	protected static void FilterToDoorLevels(notnull array<vector> points, notnull array<vector> doors, out array<vector> outKept)
	{
		outKept = {};
		if (doors.IsEmpty())
		{
			outKept.Copy(points);
			return;
		}

		foreach (vector p : points)
		{
			foreach (vector d : doors)
			{
				float dy = p[1] - d[1];
				if (dy < 0)
					dy = -dy;
				if (dy <= DOOR_FLOOR_TOL_M)
				{
					outKept.Insert(p);
					break;
				}
			}
		}

		if (outKept.IsEmpty())
			outKept.Copy(points);
	}

	// union-find: blob nearby spots, keep only the blob nearest the click, so we don't spill next door
	protected static void KeepCenterComponent(BaseWorld world, IEntity target, notnull array<vector> points, vector center, out array<vector> outKept)
	{
		outKept = {};
		int n = points.Count();
		if (n == 0)
			return;

		array<int> parent = {};
		parent.Resize(n);
		for (int i = 0; i < n; i++)
			parent[i] = i;

		float link2 = CONNECT_DIST_M * CONNECT_DIST_M;
		for (int i = 0; i < n; i++)
		{
			for (int j = i + 1; j < n; j++)
			{
				float dy = points[i][1] - points[j][1];
				if (dy < 0)
					dy = -dy;
				if (dy > CONNECT_Y_M)
					continue;
				if (HorizontalDistSq(points[i], points[j]) > link2)
					continue;

				vector mid = (points[i] + points[j]) * 0.5;
				if (!IsUnderRoof(world, target, mid))
					continue;

				Union(parent, i, j);
			}
		}

		int nearest = 0;
		float best = float.MAX;
		for (int i = 0; i < n; i++)
		{
			float d = WeightedDistSq(points[i], center);
			if (d < best)
			{
				best = d;
				nearest = i;
			}
		}

		int keepRoot = Find(parent, nearest);
		for (int i = 0; i < n; i++)
		{
			if (Find(parent, i) == keepRoot)
				outKept.Insert(points[i]);
		}

		if (outKept.IsEmpty())
			outKept.Copy(points);
	}

	// greedily pick spots farthest from the ones already chosen, so the squad fans out, not clusters
	protected static void FarthestPointSample(notnull array<vector> candidates, vector center, int want, out array<vector> outPicked)
	{
		outPicked = {};
		int c = candidates.Count();
		if (c == 0)
			return;

		float minSpace2 = MIN_POST_SPACING_M * MIN_POST_SPACING_M;

		int seed = 0;
		float far = -1.0;
		for (int i = 0; i < c; i++)
		{
			float d = WeightedDistSq(candidates[i], center);
			if (d > far)
			{
				far = d;
				seed = i;
			}
		}

		array<bool> taken = {};
		taken.Resize(c);
		array<float> minDist = {};
		minDist.Resize(c);
		for (int i = 0; i < c; i++)
		{
			taken[i] = false;
			minDist[i] = WeightedDistSq(candidates[i], candidates[seed]);
		}
		outPicked.Insert(candidates[seed]);
		taken[seed] = true;

		while (outPicked.Count() < want)
		{
			int pick = TopKFarthest(minDist, taken, FPS_JITTER_K, minSpace2);
			if (pick < 0)
				break;
			outPicked.Insert(candidates[pick]);
			taken[pick] = true;
			for (int i = 0; i < c; i++)
			{
				if (taken[i])
					continue;
				float d = WeightedDistSq(candidates[i], candidates[pick]);
				if (d < minDist[i])
					minDist[i] = d;
			}
		}
	}

	// pick one of the k farthest at random, so identical layouts don't always fill the exact same spots
	protected static int TopKFarthest(notnull array<float> minDist, notnull array<bool> taken, int k, float minSpace2)
	{
		array<int> top = {};
		array<float> topVal = {};
		for (int i = 0; i < minDist.Count(); i++)
		{
			if (taken[i] || minDist[i] < minSpace2)
				continue;

			float d = minDist[i];
			int pos = top.Count();
			while (pos > 0 && topVal[pos - 1] < d)
				pos = pos - 1;
			if (pos >= k)
				continue;

			top.InsertAt(i, pos);
			topVal.InsertAt(d, pos);
			if (top.Count() > k)
			{
				top.Remove(top.Count() - 1);
				topVal.Remove(topVal.Count() - 1);
			}
		}

		if (top.IsEmpty())
			return -1;
		return top[Math.RandomInt(0, top.Count())];
	}

	// distance that counts vertical gaps more, so different floors read as genuinely far apart
	protected static float WeightedDistSq(vector a, vector b)
	{
		float dx = a[0] - b[0];
		float dy = (a[1] - b[1]) * FLOOR_WEIGHT;
		float dz = a[2] - b[2];
		return dx * dx + dy * dy + dz * dz;
	}

	// can this post see that opening? stops short of the target so the aperture itself doesn't block
	protected static bool LosClear(BaseWorld world, vector a, vector b)
	{
		vector start = a + Vector(0, LOS_H_M, 0);
		vector end = b + Vector(0, LOS_H_M, 0);
		float dist = vector.Distance(start, end);
		if (dist <= LOS_STOP_SHORT_M)
			return true;

		vector dir = (end - start) * (1.0 / dist);
		end = end - dir * LOS_STOP_SHORT_M;

		TraceParam p = new TraceParam();
		p.Start = start;
		p.End = end;
		p.Flags = TraceFlags.ENTS;
		p.LayerMask = EPhysicsLayerDefs.Projectile;

		return world.TraceMove(p, null) >= 0.99;
	}

	// union-find with path compression
	protected static int Find(notnull array<int> parent, int i)
	{
		while (parent[i] != i)
		{
			parent[i] = parent[parent[i]];
			i = parent[i];
		}
		return i;
	}

	protected static void Union(notnull array<int> parent, int a, int b)
	{
		int ra = Find(parent, a);
		int rb = Find(parent, b);
		if (ra != rb)
			parent[ra] = rb;
	}

	// hand out facings: stairs claimed by their nearest man, then best-scoring visible openings, leftovers face out
	protected static void AssignFacings(BaseWorld world, notnull array<vector> posts, notnull array<ref SPT_GarrisonOpening> openings, vector center, vector approach, out array<vector> outFacings)
	{
		int np = posts.Count();
		int no = openings.Count();

		outFacings = {};
		outFacings.Resize(np);
		for (int i = 0; i < np; i++)
			outFacings[i] = OutwardDir(posts[i], center);

		if (no == 0 || np == 0)
			return;

		array<float> score = {};
		score.Resize(np * no);
		for (int i = 0; i < np; i++)
		{
			for (int o = 0; o < no; o++)
			{
				if (LosClear(world, posts[i], openings[o].m_Pos))
					score[i * no + o] = ScoreEdge(posts[i], openings[o], approach);
				else
					score[i * no + o] = -1.0;
			}
		}

		array<bool> postUsed = {};
		postUsed.Resize(np);
		for (int i = 0; i < np; i++)
			postUsed[i] = false;

		array<bool> covered = {};
		covered.Resize(no);
		for (int o = 0; o < no; o++)
			covered[o] = false;

		// stairwells first, whoever's nearest watches the way up
		float claim2 = ROUTE_CLAIM_M * ROUTE_CLAIM_M;
		for (int o = 0; o < no; o++)
		{
			if (!openings[o].m_Route)
				continue;

			int bestPost = -1;
			float best = claim2;
			for (int i = 0; i < np; i++)
			{
				if (postUsed[i])
					continue;
				float d = HorizontalDistSq(posts[i], openings[o].m_Pos);
				if (d < best)
				{
					best = d;
					bestPost = i;
				}
			}
			if (bestPost < 0)
				continue;

			if (openings[o].m_Normal.Length() > 0.001)
				outFacings[bestPost] = openings[o].m_Normal;
			else
				outFacings[bestPost] = FaceDir(posts[bestPost], openings[o].m_Pos);
			postUsed[bestPost] = true;
			covered[o] = true;
		}

		// then greedily pair the best remaining post-to-opening matches until we run out
		while (true)
		{
			int bestPost = -1;
			int bestOpening = -1;
			float best = 0.0;
			for (int o = 0; o < no; o++)
			{
				if (covered[o])
					continue;
				for (int i = 0; i < np; i++)
				{
					if (postUsed[i])
						continue;
					float s = score[i * no + o];
					if (s > best)
					{
						best = s;
						bestPost = i;
						bestOpening = o;
					}
				}
			}
			if (bestPost < 0)
				break;

			outFacings[bestPost] = FaceDir(posts[bestPost], openings[bestOpening].m_Pos);
			postUsed[bestPost] = true;
			covered[bestOpening] = true;
		}

		// anyone still without a job watches the best opening he can see, even if it's shared
		for (int i = 0; i < np; i++)
		{
			if (postUsed[i])
				continue;
			int bestO = -1;
			float best = 0.0;
			for (int o = 0; o < no; o++)
			{
				float s = score[i * no + o];
				if (s > best)
				{
					best = s;
					bestO = o;
				}
			}
			if (bestO >= 0)
				outFacings[i] = FaceDir(posts[i], openings[bestO].m_Pos);
		}
	}

	// score a post watching an opening, favours exterior, approach-facing, and close ones
	protected static float ScoreEdge(vector post, SPT_GarrisonOpening opening, vector approach)
	{
		float ext = 0.2;
		if (opening.m_Exterior)
			ext = 1.0;
		else if (opening.m_Route)
			ext = W_ROUTE;

		float align = 0.0;
		if (approach.Length() > 0.001)
		{
			float a = vector.Dot(opening.m_Normal, approach);
			if (a > 0.0)
				align = a;
		}

		float d = vector.Distance(post, opening.m_Pos);
		float near = 1.0 - d / MAX_WATCH_M;
		if (near < 0.0)
			near = 0.0;

		return W_EXT * ext + W_APPROACH * align + W_NEAR * near;
	}

	protected static vector FaceDir(vector pos, vector target)
	{
		vector dir = target - pos;
		dir[1] = 0;
		if (dir.Length() < 0.001)
			return vector.Zero;
		dir.Normalize();
		return dir;
	}

	// face away from the building's middle, the fallback when there's nothing better to watch
	protected static vector OutwardDir(vector pos, vector center)
	{
		vector outward = pos - center;
		outward[1] = 0;
		if (outward.Length() < 0.001)
			return vector.Zero;
		outward.Normalize();
		return outward;
	}

	// gather doors + windows, flip each normal outward, and flag the ones that open to outside
	protected static void CollectOpenings(BaseWorld world, IEntity target, vector center, out array<ref SPT_GarrisonOpening> outOpenings)
	{
		outOpenings = {};
		IEntity root = target.GetRootParent();
		if (!root)
			root = target;
		WalkForOpenings(root, outOpenings, 0);

		foreach (SPT_GarrisonOpening op : outOpenings)
		{
			vector toOut = op.m_Pos - center;
			toOut[1] = 0;
			vector n = op.m_Normal;
			n[1] = 0;
			if (n.Length() < 0.001)
				n = toOut;
			else if (vector.Dot(n, toOut) < 0)
				n = n * -1.0;
			if (n.Length() > 0.001)
				n.Normalize();
			op.m_Normal = n;

			op.m_Exterior = IsExteriorFacing(world, op);
		}
	}

	// recurse the building's parts, pulling out door origins and window hitzones as openings
	protected static void WalkForOpenings(IEntity entity, notnull array<ref SPT_GarrisonOpening> outOpenings, int depth)
	{
		if (!entity || depth > 10)
			return;

		BaseDoorComponent door = BaseDoorComponent.Cast(entity.FindComponent(BaseDoorComponent));
		if (door)
			outOpenings.Insert(new SPT_GarrisonOpening(entity.GetOrigin(), vector.Zero));
		else
		{
			HitZoneContainerComponent hzc = HitZoneContainerComponent.Cast(entity.FindComponent(HitZoneContainerComponent));
			if (hzc)
			{
				array<HitZone> hitzones = {};
				hzc.GetAllHitZones(hitzones);
				foreach (HitZone hz : hitzones)
				{
					SCR_WindowHitZone win = SCR_WindowHitZone.Cast(hz);
					if (!win)
						continue;

					IEntity owner = entity;
					HitZoneContainerComponent winContainer = win.GetHitZoneContainer();
					if (winContainer && winContainer.GetOwner())
						owner = winContainer.GetOwner();

					vector wm[4];
					owner.GetWorldTransform(wm);
					if (!AlreadyHaveOpening(outOpenings, wm[3]))
						outOpenings.Insert(new SPT_GarrisonOpening(wm[3], wm[2]));
				}
			}
		}

		IEntity child = entity.GetChildren();
		while (child)
		{
			WalkForOpenings(child, outOpenings, depth + 1);
			child = child.GetSibling();
		}
	}

	protected static bool AlreadyHaveOpening(notnull array<ref SPT_GarrisonOpening> have, vector p)
	{
		foreach (SPT_GarrisonOpening h : have)
		{
			if (vector.DistanceSq(h.m_Pos, p) < 0.25)
				return true;
		}
		return false;
	}

	// shoot a short ray out through the opening, if nothing's there it opens to the outside
	protected static bool IsExteriorFacing(BaseWorld world, SPT_GarrisonOpening op)
	{
		if (op.m_Normal.Length() < 0.001)
			return false;
		vector start = op.m_Pos + op.m_Normal * 0.3 + Vector(0, 0.3, 0);
		vector hit;
		float frac = TraceSeg(world, start, start + op.m_Normal * EXT_RAY_M, hit);
		return frac >= 0.99;
	}

	// average of the exterior openings, a rough guess at which way trouble comes from
	protected static vector ApproachDir(notnull array<ref SPT_GarrisonOpening> openings)
	{
		vector sum = vector.Zero;
		foreach (SPT_GarrisonOpening op : openings)
		{
			if (op.m_Exterior)
				sum = sum + op.m_Normal;
		}
		sum[1] = 0;
		if (sum.Length() < 0.001)
			return vector.Zero;
		sum.Normalize();
		return sum;
	}

	// spot stairwells: a standable point with another a step below and to the side, so someone watches them
	protected static void DetectRoutes(notnull array<vector> points, notnull array<ref SPT_GarrisonOpening> openings)
	{
		int n = points.Count();
		float hd2 = ROUTE_HDIST_M * ROUTE_HDIST_M;
		float hmin2 = ROUTE_HDIST_MIN_M * ROUTE_HDIST_MIN_M;

		for (int i = 0; i < n; i++)
		{
			vector down = vector.Zero;
			bool stairTop = false;
			for (int j = 0; j < n; j++)
			{
				if (i == j)
					continue;
				float step = points[i][1] - points[j][1];
				if (step < ROUTE_STEP_MIN_M || step > ROUTE_STEP_MAX_M)
					continue;
				float hsq = HorizontalDistSq(points[i], points[j]);
				if (hsq < hmin2 || hsq > hd2)
					continue;
				down = points[j] - points[i];
				stairTop = true;
				break;
			}
			if (!stairTop)
				continue;
			if (NearExistingRoute(openings, points[i]))
				continue;

			down[1] = 0;
			if (down.Length() > 0.001)
				down.Normalize();
			SPT_GarrisonOpening op = new SPT_GarrisonOpening(points[i], down);
			op.m_Route = true;
			openings.Insert(op);
		}
	}

	protected static bool NearExistingRoute(notnull array<ref SPT_GarrisonOpening> openings, vector p)
	{
		float r2 = ROUTE_DEDUP_M * ROUTE_DEDUP_M;
		foreach (SPT_GarrisonOpening o : openings)
		{
			if (o.m_Route && vector.DistanceSq(o.m_Pos, p) < r2)
				return true;
		}
		return false;
	}

	protected static float HorizontalDistSq(vector a, vector b)
	{
		float dx = a[0] - b[0];
		float dz = a[2] - b[2];
		return dx * dx + dz * dz;
	}

	protected static string YRange(notnull array<vector> pts)
	{
		if (pts.IsEmpty())
			return "[-]";
		float mn = float.MAX;
		float mx = -float.MAX;
		foreach (vector p : pts)
		{
			if (p[1] < mn)
				mn = p[1];
			if (p[1] > mx)
				mx = p[1];
		}
		return "[" + mn + ".." + mx + "]";
	}
}

enum SPT_EGarrisonDetectResult
{
	OK,
	NO_BUILDING,
	NAVMESH_NOT_READY,
	NO_POSITIONS
}
