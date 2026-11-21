// red-proof: 2026-11-21 (#3891), with Clips on the world-axis box the run failed "a thin plate turned 9 degrees on a wall clips nothing (clip=BrickWall_01_4m;TableRecreation_01)"; with the oriented box the same run passed.
// red-proof: 2026-11-21 (#3884), before the fix `--only LF_Test_PlacementAudit` failed "a table pushed 0.3 m into a wall is flagged through-wall (flags=none, clip=none)" while the table's bounds were stale (flags had no through-wall), and with Flags lacking the ClipsWall line the run with clip=BrickWall_01_4m failed the same assertion; the missing row printed "0 missing=1 near=2" and passed.
// red-proof: 2026-11-11, LF_PlacementAudit.Flags with the floating test at GAP + 5: `--only LF_Test_PlacementAudit` failed "a crate 1 m above the ground is flagged floating (flags=)" while the log measured gap=0.999. Restored, green.
//! The placement audit (LF_PlacementAudit, `tools\cli audit placement`) on fixtures spawned on open
//! ground east of the GridWorld annex fence (400, 100): a crate on the ground and one stacked on it are clean, a crate 1 m up is floating, a crate
//! at three times its size is scaled, a crate pushed half off a table overhangs without floating, a table pushed 0.3 m into a wall is
//! through-wall, and a layer row 2 m from the only entity of its prefab is a `missing` row that carries the distance (#3884).
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_PlacementAudit : LF_Test
{
	protected static const ResourceName CRATE = "{2B1D0128ED660BE9}Prefabs/Props/Crates/BoxWooden_01.et";
	protected static const ResourceName WALL = "{419130031F0009C9}Prefabs/Structures/Walls/Brick/BrickWall_01/BrickWall_01_4m.et";
	protected static const ResourceName FASCIA = "{4697D9464B596C24}Prefabs/Props/Signs/LF_Sign_Fascia.et";
	protected static const ResourceName TABLE = "{622F9989DDC4865B}PrefabLibrary/Props/Civilian/Recreation/TableRecreation_01/TableRecreation_01.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "agent-harness/placement-audit";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		BaseWorld world = GetGame().GetWorld();
		vector spot = Vector(400, world.GetSurfaceY(400, 100), 100);
		array<IEntity> spawned = {};

		IEntity grounded = LF_Utils.SpawnEntityPrefab(CRATE, spot);
		IEntity floating = LF_Utils.SpawnEntityPrefab(CRATE, Ground(world, spot + "4 0 0") + "0 1 0");
		IEntity scaled = LF_Utils.SpawnEntityPrefab(CRATE, Ground(world, spot + "8 0 0"));
		IEntity table = LF_Utils.SpawnEntityPrefab(TABLE, Ground(world, spot + "0 0 6"));
		spawned.Insert(grounded);
		spawned.Insert(floating);
		spawned.Insert(scaled);
		spawned.Insert(table);
		ctx.NotNull(grounded, "the grounded crate spawned");
		ctx.NotNull(floating, "the floating crate spawned");
		ctx.NotNull(scaled, "the scaled crate spawned");
		ctx.NotNull(table, "the table spawned");
		if (!grounded || !floating || !scaled || !table)
		{
			Cleanup(spawned);
			return;
		}

		scaled.SetScale(3);
		Physics scaledPhysics = scaled.GetPhysics();
		if (scaledPhysics)
			scaledPhysics.UpdateGeometries();

		LF_PlacementMeasure ground = LF_PlacementAudit.Measure(grounded, false);
		string groundFlags = LF_PlacementAudit.Flags(ground);
		LF_Debug.Info("PlaceAudit", "grounded crate " + LF_PlacementAudit.Format(ground));
		ctx.True(groundFlags.IsEmpty(), "a crate on the ground is not flagged (flags=" + groundFlags + ")");

		LF_PlacementMeasure up = LF_PlacementAudit.Measure(floating, false);
		string upFlags = LF_PlacementAudit.Flags(up);
		LF_Debug.Info("PlaceAudit", "floating crate " + LF_PlacementAudit.Format(up));
		ctx.True(upFlags.Contains("floating"), "a crate 1 m above the ground is flagged floating (flags=" + upFlags + ")");
		ctx.EqualFloat(1.0, up.m_fGap, 0.1, "its gap is the 1 m it was lifted");

		LF_PlacementMeasure big = LF_PlacementAudit.Measure(scaled, false);
		string bigFlags = LF_PlacementAudit.Flags(big);
		LF_Debug.Info("PlaceAudit", "scaled crate " + LF_PlacementAudit.Format(big));
		ctx.True(bigFlags.Contains("scaled"), "a crate at three times its size is flagged scaled (flags=" + bigFlags + ")");
		ctx.EqualFloat(ground.m_vHalf[0] * 3, big.m_vHalf[0], 0.05, "the oriented box carries the scale");

		// A crate stacked on the grounded one rests on it: the crate's top is a surface (2026-11-11: the
		// projectile-layer trace went through BoxWooden_01 and called every stacked crate floating).
		IEntity stacked = LF_Utils.SpawnEntityPrefab(CRATE, grounded.GetOrigin() + Vector(0, ground.m_vMaxs[1] - ground.m_vMins[1], 0));
		spawned.Insert(stacked);
		ctx.NotNull(stacked, "the stacked crate spawned");
		if (stacked)
		{
			LF_PlacementMeasure top = LF_PlacementAudit.Measure(stacked, false);
			string topFlags = LF_PlacementAudit.Flags(top);
			LF_Debug.Info("PlaceAudit", "stacked crate " + LF_PlacementAudit.Format(top));
			ctx.True(topFlags.IsEmpty(), "a crate stacked on another is not flagged (flags=" + topFlags + ", on=" + top.m_sSupport + ")");
		}

		// Push a crate a third of its width past the table's long edge.
		string topLabel;
		vector tmin;
		vector tmax;
		table.GetWorldBounds(tmin, tmax);
		float cz = (tmin[2] + tmax[2]) * 0.5;
		float topY = LF_PlacementAudit.SupportAt(null, (tmin[0] + tmax[0]) * 0.5, cz, tmax[1] + 0.5, topLabel);
		float cx = tmax[0] - ground.m_vHalf[0] * 0.35;
		IEntity edge = LF_Utils.SpawnEntityPrefab(CRATE, Vector(cx, topY, cz) + Vector(0, grounded.GetOrigin()[1] - ground.m_vMins[1], 0));
		spawned.Insert(edge);
		ctx.NotNull(edge, "the edge crate spawned");
		if (edge)
		{
			LF_PlacementMeasure over = LF_PlacementAudit.Measure(edge, false);
			string overFlags = LF_PlacementAudit.Flags(over);
			LF_Debug.Info("PlaceAudit", string.Format("edge crate on %1 (top %2) %3", topLabel, topY, LF_PlacementAudit.Format(over)));
			ctx.True(overFlags.Contains("overhang"), "a crate half off the table is flagged overhang (flags=" + overFlags + ")");
			ctx.True(!overFlags.Contains("floating"), "it is not floating: its centre rests on the table");
		}

		// A table pushed 0.3 m into a wall stands through it; one a metre clear of the same wall does not (a crate is too small for
		// the clip rays: they need a box over 0.3 m in every direction).
		IEntity wall = LF_Utils.SpawnEntityPrefab(WALL, Ground(world, spot + "12 0 -8"));
		spawned.Insert(wall);
		ctx.NotNull(wall, "the wall spawned");
		if (wall)
		{
			vector wmin;
			vector wmax;
			wall.GetWorldBounds(wmin, wmax);
			float wx = (wmin[0] + wmax[0]) * 0.5;
			float wy = world.GetSurfaceY(wx, wmax[2] + 1.0);
			// The bounds of an entity moved after its spawn are stale, so measure one table's size and spawn the two at their spots.
			IEntity sizing = LF_Utils.SpawnEntityPrefab(TABLE, Vector(wx, wy, wmax[2] + 3.0));
			ctx.NotNull(sizing, "the sizing table spawned");
			if (sizing)
			{
				vector pmin;
				vector pmax;
				sizing.GetWorldBounds(pmin, pmax);
				float half = (pmax[2] - pmin[2]) * 0.5;
				float pivot = sizing.GetOrigin()[2] - (pmin[2] + pmax[2]) * 0.5;
				SCR_EntityHelper.DeleteEntityAndChildren(sizing);
				IEntity pushed = LF_Utils.SpawnEntityPrefab(TABLE, Vector(wx, wy, wmax[2] - 0.3 + half + pivot));
				IEntity clear = LF_Utils.SpawnEntityPrefab(TABLE, Vector(wx, wy, wmax[2] + 1.0 + half + pivot));
				spawned.Insert(pushed);
				spawned.Insert(clear);
				ctx.NotNull(pushed, "the pushed table spawned");
				ctx.NotNull(clear, "the clear table spawned");
				if (!pushed || !clear)
				{
					Cleanup(spawned);
					return;
				}

				LF_Debug.Info("PlaceAudit", string.Format("wall %1..%2 table half=%3", wmin, wmax, half));
				LF_PlacementMeasure through = LF_PlacementAudit.Measure(pushed, false);
				string throughFlags = LF_PlacementAudit.Flags(through);
				LF_Debug.Info("PlaceAudit", "table in a wall " + LF_PlacementAudit.Format(through));
				ctx.True(throughFlags.Contains("through-wall"), "a table pushed 0.3 m into a wall is flagged through-wall (flags=" + throughFlags + ", clip=" + through.m_sClips + ")");
				LF_PlacementMeasure free = LF_PlacementAudit.Measure(clear, false);
				string freeFlags = LF_PlacementAudit.Flags(free);
				ctx.True(!freeFlags.Contains("through-wall"), "a table a metre clear of the wall is not through-wall (flags=" + freeFlags + ")");

				// A thin fascia plate turned 9 degrees off the world axes (the b59 hardware shop's gable, #3891): its world-axis box is 0.7 m
				// deep and always holds the wall behind it, so it used to read as clipping; its own box is a 5 cm plate and clips nothing.
				IEntity plate = LF_Utils.SpawnEntityPrefab(FASCIA, Vector(wx, wy + 0.6, (wmin[2] + wmax[2]) * 0.5), "9 0 0");
				spawned.Insert(plate);
				ctx.NotNull(plate, "the turned fascia plate spawned");
				if (plate)
				{
					LF_PlacementMeasure turned = LF_PlacementAudit.Measure(plate, false);
					LF_Debug.Info("PlaceAudit", "fascia plate 9 deg on a wall " + LF_PlacementAudit.Format(turned));
					ctx.True(turned.m_sClips == "none", "a thin plate turned 9 degrees on a wall clips nothing (clip=" + turned.m_sClips + ")");
				}
			}
		}

		// A layer row 2 m from the only crate there is: nothing within 1.5 m, so the row is missing, and it says how far the crate is.
		string inPath = "$profile:lf_place_audit_missing_in.txt";
		string outPath = "$profile:lf_place_audit_missing_out.txt";
		FileHandle rowFile = FileIO.OpenFile(inPath, FileMode.WRITE);
		ctx.True(rowFile != null, "the batch input file opened");
		if (rowFile)
		{
			vector layerSpot = Ground(world, spot + "0 0 -2");
			rowFile.WriteLine(string.Format("0 - BoxWooden_01 %1 %2 %3 0", layerSpot[0], layerSpot[1], layerSpot[2]));
			rowFile.Close();
			int missing;
			LF_PlacementAudit.RunBatch(inPath, outPath, missing);
			ctx.Equal(1, missing, "the moved row is counted missing");
			FileHandle outFile = FileIO.OpenFile(outPath, FileMode.READ);
			ctx.True(outFile != null, "the batch output file opened");
			if (outFile)
			{
				string first;
				outFile.ReadLine(first);
				outFile.Close();
				LF_Debug.Info("PlaceAudit", "moved row: " + first);
				ctx.True(first.Contains("missing=1"), "a row 2 m from its prefab is missing with the distance (row=" + first + ")");
				float near = ParseNear(first);
				ctx.EqualFloat(2.0, near, 0.4, "the row carries the distance to the nearest crate (row=" + first + ")");
			}
		}

		Cleanup(spawned);
	}

	//------------------------------------------------------------------------------------------------
	//! The number after "near=" in a batch row, -1 when absent.
	protected static float ParseNear(string row)
	{
		array<string> parts = {};
		row.Split(" ", parts, true);
		foreach (string part : parts)
		{
			if (part.StartsWith("near="))
				return part.Substring(5, part.Length() - 5).ToFloat();
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	protected static vector Ground(BaseWorld world, vector p)
	{
		return Vector(p[0], world.GetSurfaceY(p[0], p[2]), p[2]);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Cleanup(array<IEntity> spawned)
	{
		foreach (IEntity entity : spawned)
		{
			if (entity)
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
		}
	}
}
