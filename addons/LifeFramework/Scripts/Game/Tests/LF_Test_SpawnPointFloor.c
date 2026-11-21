// red-proof: 2026-11-21, observed with `cli test --no-build --red LF_Test_SpawnPointFloor`: the RedProof() hook adds one floating point (0.4 m up) to the collected rows and the sweep line "every spawn and teleport point ... stands on the floor" fails naming it. The first run of the sweep on the unfixed world failed on 6 first-spawn and hospital stand points (no room: a stand offset 0.27 m off the mattress) and on the third bed of the first-night room.
//! #3912: every point a script puts a player body at stands on the floor with room for a body. The verdict
//! (LF_SpawnPointAudit.Judge) is checked with no world; a point on open ground passes, the same point
//! 0.4 m up, one standing on a crate top and one beside a crate that crowds the body each fail with the
//! reason; and every point the loaded world has (beds, houses, the jail, the role spawns) passes, so a
//! new spawn point that floats fails this test. The island points are the harness verb spawncheck (a
//! session), because tests do not load that world.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_SpawnPointFloor : LF_Test
{
	protected static const ResourceName CRATE = "{2B1D0128ED660BE9}Prefabs/Props/Crates/BoxWooden_01.et";
	protected static const float HOVER_M = 0.4;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "spawn/point-floor";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The verdict, with no world.
		ctx.EqualStr("", LF_SpawnPointAudit.Judge(true, 0.02, false, true), "a point 2 cm over the floor passes");
		ctx.True(LF_SpawnPointAudit.Judge(true, HOVER_M, false, true).Contains("above"), "a point 0.4 m up fails as above the floor");
		ctx.True(LF_SpawnPointAudit.Judge(true, -HOVER_M, false, true).Contains("below"), "a point 0.4 m under fails as below the floor");
		ctx.True(LF_SpawnPointAudit.Judge(true, 0, true, true).Contains("prop"), "a point over a prop mesh fails");
		ctx.True(LF_SpawnPointAudit.Judge(true, 0, false, false).Contains("room"), "a point with no room for a body fails");
		ctx.True(LF_SpawnPointAudit.Judge(false, 0, false, true).Contains("no floor"), "a point with no floor under it fails");

		vector ground;
		ctx.True(FindGround(ground), "fixture: open ground");
		// A point on the ground, then the same point hovering.
		LF_SpawnPointRow onGround = LF_SpawnPointAudit.Measure("test", "ground", ground);
		ctx.EqualStr("", onGround.m_sVerdict, string.Format("a point on open ground passes (gap %1, room %2)", onGround.m_fGap, onGround.m_bRoom));
		LF_SpawnPointRow hovering = LF_SpawnPointAudit.Measure("test", "hover", ground + Vector(0, HOVER_M, 0));
		ctx.True(hovering.m_sVerdict.Contains("above"), string.Format("a point 0.4 m over the ground fails (%1)", hovering.m_sVerdict));
		ctx.True(Math.AbsFloat(hovering.m_fGap - HOVER_M) < 0.02, string.Format("its gap is measured against the ground (%1 m)", hovering.m_fGap));

		// A crate: a point on its top is over a prop mesh; a point beside it, close enough that the body
		// would stand inside it, has no room.
		IEntity crate = LF_Utils.SpawnEntityPrefab(CRATE, ground + "6 0 0");
		ctx.True(crate != null, "fixture: the crate spawns");
		if (!crate)
			return;

		vector mins, maxs;
		crate.GetWorldBounds(mins, maxs);
		vector top = Vector((mins[0] + maxs[0]) * 0.5, maxs[1], (mins[2] + maxs[2]) * 0.5);
		LF_SpawnPointRow onCrate = LF_SpawnPointAudit.Measure("test", "crate-top", top);
		ctx.True(onCrate.m_bOverProp, string.Format("a point on a crate top is over a prop mesh (crate height %1)", maxs[1] - mins[1]));
		ctx.True(onCrate.m_sVerdict.Contains("prop"), string.Format("its verdict names the prop (%1)", onCrate.m_sVerdict));

		vector beside = Vector(maxs[0] + 0.1, mins[1], (mins[2] + maxs[2]) * 0.5);
		beside[1] = GetGame().GetWorld().GetSurfaceY(beside[0], beside[2]);
		LF_SpawnPointRow crowded = LF_SpawnPointAudit.Measure("test", "crowded", beside);
		ctx.False(crowded.m_bRoom, "a point 10 cm from a crate has no room for a standing body");
		SCR_EntityHelper.DeleteEntityAndChildren(crate);

		// Every point of the loaded world.
		array<ref LF_SpawnPointRow> rows = {};
		LF_SpawnPointAudit.Collect(rows);
		foreach (LF_SpawnPointRow each : rows)
			LF_SpawnPointAudit.Log(each);

		// Red proof: a spawn point that floats, added to the world, must fail this sweep.
		if (RedProof())
			rows.Insert(LF_SpawnPointAudit.Measure("test", "new-floating-point", ground + Vector(0, HOVER_M, 0)));

		int failing;
		string text = LF_SpawnPointAudit.Failures(rows, failing);
		ctx.Equal(0, failing, string.Format("every spawn and teleport point in the world stands on the floor with room (%1 points, %2 fail: %3)", rows.Count(), failing, text));
	}

	//------------------------------------------------------------------------------------------------
	//! Open DebugWorld terrain: the first candidate with ground under it and nothing on it (no town coordinates).
	protected bool FindGround(out vector at)
	{
		array<vector> candidates = {"420 0 420", "380 0 460", "460 0 380", "200 0 330", "330 0 60"};
		foreach (vector candidate : candidates)
		{
			BaseWorld world = GetGame().GetWorld();
			float y = world.GetSurfaceY(candidate[0], candidate[2]);
			if (y < -100)
				continue;

			IEntity hit;
			float d = LF_PlacementAudit.SurfaceTrace(Vector(candidate[0], y + 3, candidate[2]), Vector(candidate[0], y - 1, candidate[2]), null, hit);
			if (Math.AbsFloat(d - 3) < 0.02)
			{
				at = Vector(candidate[0], y, candidate[2]);
				return true;
			}
		}

		return false;
	}
}
