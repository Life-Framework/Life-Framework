// red-proof: observed 2026-11-14 on DebugWorld only: the no-road-network branch, with NearestRoad forced to return true, failed `no AI road network finds no road`; restored, green. The Arland branch waits on #1129: bare Arland has no game mode, so the suite never starts there.
// tier: WORLD
//! #1026 review: the ACROSS_ROAD snap and the server's across-the-road check on a world with an AI
//! road network. DebugWorld has none (its roads are spline meshes), so there the test only says so;
//! the real run is on vanilla Arland:
//!   tools/test/test-e2e.ps1 -Mission worlds/Arland/Arland.ent -Only LF_Test_PlacementRoad
//! It finds a real road, stands 2 m off a straight stretch of it, and checks that the road is found,
//! that the plan's axis runs straight across it for a player facing 40 degrees up the road, and that
//! the server accepts a proposal across the road and refuses one turned 30 degrees.
class LF_Test_PlacementRoad : LF_Test
{
	override string GetName()
	{
		return "placement/across-road";
	}

	override void Run(LF_TestContext ctx)
	{
		ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld || !aiWorld.GetRoadNetworkManager())
		{
			LF_Debug.Info(LF_PlacementService.FEATURE, "across-road test: this world has no AI road network; run it on Arland (test-e2e.ps1 -Mission worlds/Arland/Arland.ent -Only LF_Test_PlacementRoad)");
			vector none;
			ctx.False(LF_PlacementService.NearestRoad("300 0 248", none), "no AI road network finds no road");
			return;
		}

		vector start;
		vector end;
		if (!FindStraight(aiWorld, start, end))
		{
			ctx.Fail("the AI road network has no straight stretch of road 8 m long near the map's middle");
			return;
		}

		vector along = LF_PlacementGeometry.Flat(end - start);
		vector across = Vector(-along[2], 0, along[0]);
		vector spot = (start + end) * 0.5 + across * 2;
		spot[1] = GetGame().GetWorld().GetSurfaceY(spot[0], spot[2]);
		LF_Debug.Info(LF_PlacementService.FEATURE, string.Format("across-road test: road from %1 to %2, spot %3", start, end, spot));

		vector roadDir;
		ctx.True(LF_PlacementService.NearestRoad(spot, roadDir), "a road 2 m away is found");
		ctx.InRange(LF_PlacementGeometry.AcrossAngle(across, roadDir), 0, 1, "the road found runs the way the network's points do");

		// A player facing 40 degrees up the road: the plan's axis still runs straight across it.
		vector facing = (across * Math.Cos(40 * Math.DEG2RAD) + along * Math.Sin(40 * Math.DEG2RAD)).Normalized();
		vector axis = LF_PlacementGeometry.Axis(facing, true, roadDir);
		ctx.InRange(LF_PlacementGeometry.AcrossAngle(axis, roadDir), 0, 0.5, "the plan lies straight across the road, not where the player faces");
		ctx.True(vector.Dot(axis, facing) > 0, "the plan lies on the side the player faces");

		// The server's check of a proposal's heading.
		vector proposal[4];
		LF_PlacementGeometry.Transform(spot - across * 0.5, spot + across * 0.5, proposal);
		string note;
		ctx.EqualStr("", LF_PlacementService.RoadHeadingProblem(proposal, note), "a proposal straight across the road passes");
		ctx.True(note.IndexOf("across the road") != -1, "the placed line notes the angle: " + note);

		vector turnedDir = (across * Math.Cos(30 * Math.DEG2RAD) + along * Math.Sin(30 * Math.DEG2RAD)).Normalized();
		LF_PlacementGeometry.Transform(spot - turnedDir * 0.5, spot + turnedDir * 0.5, proposal);
		string problem = LF_PlacementService.RoadHeadingProblem(proposal, note);
		ctx.True(problem.IndexOf("across the road") != -1, "a proposal turned 30 degrees off across is refused: " + problem);
	}

	//------------------------------------------------------------------------------------------------
	//! A straight road segment at least 8 m long, near a grid of points around the map's middle.
	protected bool FindStraight(ChimeraAIWorld aiWorld, out vector start, out vector end)
	{
		vector mins;
		vector maxs;
		GetGame().GetWorld().GetBoundBox(mins, maxs);
		vector middle = (mins + maxs) * 0.5;
		for (int x = -3; x <= 3; x++)
		{
			for (int z = -3; z <= 3; z++)
			{
				vector at = middle + Vector(x * 300, 0, z * 300);
				BaseRoad road;
				float distance;
				aiWorld.GetRoadNetworkManager().GetClosestRoad(at, road, distance);
				if (!road)
					continue;

				array<vector> points = {};
				road.GetPoints(points);
				for (int i = 0; i + 1 < points.Count(); i++)
				{
					vector a = points[i];
					vector b = points[i + 1];
					if (vector.Distance(Vector(a[0], 0, a[2]), Vector(b[0], 0, b[2])) >= 8)
					{
						start = a;
						end = b;
						return true;
					}
				}
			}
		}

		return false;
	}
}
