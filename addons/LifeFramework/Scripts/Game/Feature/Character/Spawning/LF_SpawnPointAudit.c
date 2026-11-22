// LF_SpawnPointAudit.c - every point a script puts a player body at, measured the way the first-spawn
// bed was (#3904): the gap between the point and the floor under it, whether the point is over a prop
// mesh, and whether a standing body fits. A float-in-the-air spawn is a placement bug the player sees as
// a drop (or a body standing on a counter), and no check measured one until the bed wake (#3912).
//
// Points collected (the loaded world only; nothing here spawns or moves anything):
//   first-spawn beds   the wake point and the stand point of each bed
//   hospital beds      the respawn sit point and the stand point of each bed
//   house spawns       every authored inside point and the outside point of each live house
//   jail               the cell (where a sentence puts the prisoner) and the release point
//   spawn points       every SCR_SpawnPoint (the role spawns LF_SpawnLogic falls back to)
// Quests and events place no player: the only other teleports are the debug grid teleport (a grid cell by
// design) and the NPC spots (residents are snapped by LF_ResidentSpawner.OnFloor). The verdict is one
// pure function (Judge) so a test can name every failing case without a world.

//------------------------------------------------------------------------------------------------
//! One measured point.
class LF_SpawnPointRow
{
	string m_sKind;
	string m_sName;
	vector m_vPoint;
	bool m_bFloor;
	float m_fGap;
	bool m_bOverProp;
	bool m_bRoom;
	//! What the room box found in the way (a name), empty when there is room or the engine names none.
	string m_sBlocker;
	//! Empty when the point passes, else why it fails.
	string m_sVerdict;
}

//------------------------------------------------------------------------------------------------
class LF_SpawnPointAudit
{
	static const string FEATURE = "SpawnPoint";
	//! Feet within this of the floor (the placement plan's 5 cm; a body rests 2.5 cm over the probed floor).
	static const float TOLERANCE = 0.05;
	//! The floor probe starts this far above the point and reaches this far below it: a point written a
	//! storey off must fail as a gap, not read as "no floor".
	static const float PROBE_UP = 0.5;
	static const float PROBE_REACH = 5.0; // lf-fixed: how far a point a storey off is still measured as a gap
	//! An entity under the point counts as a prop mesh between these heights (a rug is floor, a wall or
	//! building is not a prop) and when no side is longer than PROP_MAX_FOOTPRINT.
	static const float PROP_MIN_HEIGHT = 0.15;
	static const float PROP_MAX_HEIGHT = 1.5;
	static const float PROP_MAX_FOOTPRINT = 4.0; // lf-fixed: larger than this is a building, not a prop

	//------------------------------------------------------------------------------------------------
	//! The verdict, with no world. Empty string: the point passes.
	static string Judge(bool floorFound, float gap, bool overProp, bool room)
	{
		if (!floorFound)
			return "no floor under the point";

		if (overProp)
			return "over a prop mesh";

		if (Math.AbsFloat(gap) > TOLERANCE)
			return string.Format("%1 m %2 the floor", Math.AbsFloat(gap), GapWord(gap));

		if (!room)
			return "no room for a standing body";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	protected static string GapWord(float gap)
	{
		if (gap > 0)
			return "above";

		return "below";
	}

	//------------------------------------------------------------------------------------------------
	//! True when `hit` is a loose prop a body could be standing on: furniture-sized, not flat floor
	//! dressing and not a building. Terrain (no entity) is never a prop.
	static bool IsPropMesh(IEntity hit)
	{
		if (!hit)
			return false;

		vector mins, maxs;
		hit.GetWorldBounds(mins, maxs);
		vector size = maxs - mins;
		if (size[1] < PROP_MIN_HEIGHT || size[1] > PROP_MAX_HEIGHT)
			return false;

		return size[0] <= PROP_MAX_FOOTPRINT && size[2] <= PROP_MAX_FOOTPRINT;
	}

	//------------------------------------------------------------------------------------------------
	//! Measures one point: the floor under it (every layer, characters and trees seen through), the
	//! gap, the prop test and the standing room. `ignore` is an entity the room test sees through.
	static LF_SpawnPointRow Measure(string kind, string name, vector point, IEntity ignore = null)
	{
		LF_SpawnPointRow row = new LF_SpawnPointRow();
		row.m_sKind = kind;
		row.m_sName = name;
		row.m_vPoint = point;
		row.m_bRoom = true;
		if (!GetGame() || !GetGame().GetWorld())
		{
			row.m_sVerdict = "no world";
			return row;
		}

		IEntity hit;
		float floorY = LF_PlaceSnap.Down(null, point + Vector(0, PROBE_UP, 0), PROBE_UP + PROBE_REACH, -1, hit);
		row.m_bFloor = floorY > -1e8;
		if (row.m_bFloor)
		{
			row.m_fGap = point[1] - floorY;
			row.m_bOverProp = IsPropMesh(hit);
		}

		IEntity blocker;
		row.m_bRoom = LF_BedWakePose.BodyFitsBy(point, ignore, blocker);
		if (blocker)
			row.m_sBlocker = BlockerName(blocker);
		row.m_sVerdict = Judge(row.m_bFloor, row.m_fGap, row.m_bOverProp, row.m_bRoom);
		return row;
	}

	//------------------------------------------------------------------------------------------------
	protected static string BlockerName(IEntity blocker)
	{
		string text = blocker.GetName();
		EntityPrefabData data = blocker.GetPrefabData();
		if (data)
			text = text + " " + data.GetPrefabName();

		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! One line per point in the log, the same fields for every kind.
	static void Log(notnull LF_SpawnPointRow row)
	{
		string verdict = "ok";
		if (!row.m_sVerdict.IsEmpty())
			verdict = "FAIL " + row.m_sVerdict;

		LF_Debug.Info(FEATURE, string.Format("%1 %2 at %3: gap %4 m, prop %5, room %6 %7: %8", row.m_sKind, row.m_sName, row.m_vPoint, row.m_fGap, row.m_bOverProp, row.m_bRoom, row.m_sBlocker, verdict));
	}

	//------------------------------------------------------------------------------------------------
	//! Every point of every source in the loaded world, measured. Server or host.
	static void Collect(notnull array<ref LF_SpawnPointRow> outRows)
	{
		CollectBeds(outRows);
		CollectHouses(outRows);
		CollectJail(outRows);
		CollectSpawnPoints(outRows);
	}

	//------------------------------------------------------------------------------------------------
	protected static void CollectBeds(notnull array<ref LF_SpawnPointRow> outRows)
	{
		array<LF_FirstSpawnBedComponent> firstBeds = {};
		LF_FirstSpawnBeds.GetBeds(firstBeds);
		foreach (LF_FirstSpawnBedComponent first : firstBeds)
		{
			IEntity bed = first.GetOwner();
			vector wake[4];
			first.GetWakeTransform(wake);
			outRows.Insert(Measure("first-spawn-wake", bed.GetName(), wake[3], bed));
			outRows.Insert(Measure("first-spawn-stand", bed.GetName(), first.GetStandPosition(), bed));
		}

		array<LF_HospitalBedComponent> hospitalBeds = {};
		LF_HospitalBedComponent.GetBeds(hospitalBeds);
		foreach (LF_HospitalBedComponent hospital : hospitalBeds)
		{
			IEntity hospitalBed = hospital.GetOwner();
			vector sit[4];
			hospital.GetSitTransform(sit);
			outRows.Insert(Measure("hospital-wake", hospitalBed.GetName(), sit[3], hospitalBed));
			outRows.Insert(Measure("hospital-stand", hospitalBed.GetName(), hospital.GetStandPosition(), hospitalBed));
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void CollectHouses(notnull array<ref LF_SpawnPointRow> outRows)
	{
		map<string, LF_HouseManagerComponent> houses = LF_HouseManagerComponent.Live();
		foreach (string id, LF_HouseManagerComponent house : houses)
		{
			if (!house || !house.GetOwner())
				continue;

			LF_HouseSpawnComponent spawn = LF_Component<LF_HouseSpawnComponent>.Find(house.GetOwner());
			if (!spawn)
				continue;

			array<vector> points = {};
			array<string> names = {};
			spawn.GetAllPoints(points, names);
			foreach (int i, vector point : points)
				outRows.Insert(Measure("house", string.Format("%1 %2", id, names[i]), point));
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void CollectJail(notnull array<ref LF_SpawnPointRow> outRows)
	{
		if (!LF_Jail.HasCell())
			return;

		outRows.Insert(Measure("jail-cell", "cell", LF_Jail.GetCellOrigin()));
		outRows.Insert(Measure("jail-release", "release", LF_Jail.GetReleasePoint()));
	}

	//------------------------------------------------------------------------------------------------
	protected static void CollectSpawnPoints(notnull array<ref LF_SpawnPointRow> outRows)
	{
		array<SCR_SpawnPoint> points = SCR_SpawnPoint.GetSpawnPoints();
		if (!points)
			return;

		foreach (SCR_SpawnPoint point : points)
		{
			if (!point)
				continue;

			vector position, rotation;
			point.GetPositionAndRotation(position, rotation);
			outRows.Insert(Measure("spawn-point", point.GetName(), position, point));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The failing rows' reasons joined, for a one-line answer; empty when every point passes.
	static string Failures(notnull array<ref LF_SpawnPointRow> rows, out int failing)
	{
		failing = 0;
		string text;
		foreach (LF_SpawnPointRow row : rows)
		{
			if (row.m_sVerdict.IsEmpty())
				continue;

			failing++;
			if (failing <= 3)
			{
				if (!text.IsEmpty())
					text += "; ";

				text += string.Format("%1 %2 at %3: %4", row.m_sKind, row.m_sName, row.m_vPoint, row.m_sVerdict);
				if (!row.m_sBlocker.IsEmpty())
					text += " (blocked by " + row.m_sBlocker + ")";
			}
		}

		return text;
	}
}
