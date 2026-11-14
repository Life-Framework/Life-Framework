// red-proof: observed 2026-11-14 on CoreOnEveron (real Everon roads): the `a proposal along the road is refused` assertion flipped to `== -1` failed the test (`[LFTEST] FAIL placement/across-road`); restored, 36 assertions green over 4 road stretches. On DebugWorld the no-road-network branch, with NearestRoad forced to return true, failed `no AI road network finds no road`.
// tier: WORLD
//! #1026 review, #1129: the ACROSS_ROAD snap and the server's across-the-road check on a world with an
//! AI road network. DebugWorld has none (its roads are spline meshes), so there the test only says so;
//! the real run is on vanilla Everon with the core's game mode and an AI world (Worlds/CoreOnEveron):
//!   tools\cli test --no-build --only LF_Test_PlacementRoad --world Worlds/CoreOnEveron/CoreOnEveron.ent
//! It finds up to five real roads near the map's middle and, on a straight stretch of each, stands 2 m
//! off it and checks that the road is found, that the plan's axis runs straight across it for a player
//! facing 40 degrees up the road, and that the server accepts a proposal across the road (and 10 degrees
//! off) and refuses one turned 30 degrees and one along the road.
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
			LF_Debug.Info(LF_PlacementService.FEATURE, "across-road test: this world has no AI road network; run it on CoreOnEveron");
			vector none;
			ctx.False(LF_PlacementService.NearestRoad("300 0 248", none), "no AI road network finds no road");
			return;
		}

		array<vector> starts = {};
		array<vector> ends = {};
		FindStraights(aiWorld, 5, starts, ends);
		if (starts.Count() < 3)
		{
			ctx.Fail(string.Format("the AI road network has only %1 straight stretches of road 8 m long near the map's middle, wanted 3", starts.Count()));
			return;
		}

		for (int s = 0; s < starts.Count(); s++)
			RunStretch(ctx, starts[s], ends[s]);
	}

	//------------------------------------------------------------------------------------------------
	//! One real stretch of road: straight across passes, 10 degrees off passes, 30 degrees off and along it are refused.
	protected void RunStretch(LF_TestContext ctx, vector start, vector end)
	{
		vector along = LF_PlacementGeometry.Flat(end - start);
		vector across = Vector(-along[2], 0, along[0]);
		vector spot = (start + end) * 0.5 + across * 2;
		spot[1] = GetGame().GetWorld().GetSurfaceY(spot[0], spot[2]);
		string at = string.Format("[road %1 to %2]", start, end);
		LF_Debug.Info(LF_PlacementService.FEATURE, string.Format("across-road test: road from %1 to %2, spot %3", start, end, spot));

		vector roadDir;
		ctx.True(LF_PlacementService.NearestRoad(spot, roadDir), at + " a road 2 m away is found");
		ctx.InRange(LF_PlacementGeometry.AcrossAngle(across, roadDir), 0, 1, at + " the road found runs the way the network's points do");

		// A player facing 40 degrees up the road: the plan's axis still runs straight across it.
		vector facing = (across * Math.Cos(40 * Math.DEG2RAD) + along * Math.Sin(40 * Math.DEG2RAD)).Normalized();
		vector axis = LF_PlacementGeometry.Axis(facing, true, roadDir);
		ctx.InRange(LF_PlacementGeometry.AcrossAngle(axis, roadDir), 0, 0.5, at + " the plan lies straight across the road, not where the player faces");
		ctx.True(vector.Dot(axis, facing) > 0, at + " the plan lies on the side the player faces");

		// The server's check of a proposal's heading: across and 10 degrees off pass, 30 degrees off and along the road are refused.
		vector proposal[4];
		string note;
		LF_PlacementGeometry.Transform(spot - across * 0.5, spot + across * 0.5, proposal);
		ctx.EqualStr("", LF_PlacementService.RoadHeadingProblem(proposal, note), at + " a proposal straight across the road passes");
		ctx.True(note.IndexOf("across the road") != -1, at + " the placed line notes the angle: " + note);

		vector slightDir = (across * Math.Cos(10 * Math.DEG2RAD) + along * Math.Sin(10 * Math.DEG2RAD)).Normalized();
		LF_PlacementGeometry.Transform(spot - slightDir * 0.5, spot + slightDir * 0.5, proposal);
		ctx.EqualStr("", LF_PlacementService.RoadHeadingProblem(proposal, note), at + " a proposal 10 degrees off across passes");

		vector turnedDir = (across * Math.Cos(30 * Math.DEG2RAD) + along * Math.Sin(30 * Math.DEG2RAD)).Normalized();
		LF_PlacementGeometry.Transform(spot - turnedDir * 0.5, spot + turnedDir * 0.5, proposal);
		string problem = LF_PlacementService.RoadHeadingProblem(proposal, note);
		ctx.True(problem.IndexOf("across the road") != -1, at + " a proposal turned 30 degrees off across is refused: " + problem);

		vector alongDir = along.Normalized();
		LF_PlacementGeometry.Transform(spot - alongDir * 0.5, spot + alongDir * 0.5, proposal);
		problem = LF_PlacementService.RoadHeadingProblem(proposal, note);
		ctx.True(problem.IndexOf("across the road") != -1, at + " a proposal along the road is refused: " + problem);
	}

	//------------------------------------------------------------------------------------------------
	//! Up to `wanted` straight road segments at least 8 m long, from different roads (at least 100 m apart),
	//! near a grid of points around the map's middle.
	protected void FindStraights(ChimeraAIWorld aiWorld, int wanted, out array<vector> starts, out array<vector> ends)
	{
		vector mins;
		vector maxs;
		GetGame().GetWorld().GetBoundBox(mins, maxs);
		vector middle = (mins + maxs) * 0.5;
		for (int x = -3; x <= 3; x++)
		{
			for (int z = -3; z <= 3; z++)
			{
				if (starts.Count() >= wanted)
					return;

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
					if (vector.Distance(Vector(a[0], 0, a[2]), Vector(b[0], 0, b[2])) < 8)
						continue;

					bool fresh = true;
					for (int k = 0; k < starts.Count(); k++)
					{
						if (vector.Distance(Vector(a[0], 0, a[2]), Vector(starts[k][0], 0, starts[k][2])) < 100)
							fresh = false;
					}

					if (fresh)
					{
						starts.Insert(a);
						ends.Insert(b);
					}

					break;
				}
			}
		}
	}
}
