// red-proof: RedProof() skips the route search, the corner speeds, the heap and the resample, so every
// assertion below fails. RED-OK 2026-11-27 (tools\cli test --no-build --red LF_Test_DriveRoutePlanner).
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_DriveRoutePlanner : LF_Test
{
	override string GetName()
	{
		return "agent-harness/drive-route-planner";
	}

	override void Run(LF_TestContext ctx)
	{
		// Three roads: A east 100 m, B north 100 m from A's end (the short way), C a 400 m detour from
		// A's start round to B's end.
		array<vector> nodes = {};
		array<int> roadOf = {};
		for (int a = 0; a <= 10; a++)
			AddNode(nodes, roadOf, Vector(a * 10, 0, 0), 0);

		for (int b = 0; b <= 10; b++)
			AddNode(nodes, roadOf, Vector(100, 0, b * 10), 1);

		AddNode(nodes, roadOf, Vector(0, 0, 0), 2);
		AddNode(nodes, roadOf, Vector(0, 0, 200), 2);
		AddNode(nodes, roadOf, Vector(100, 0, 200), 2);
		AddNode(nodes, roadOf, Vector(100, 0, 100), 2);
		int goal = 21;

		float length = -1;
		array<int> chain = {};
		array<float> straightSpeeds = {};
		array<float> bendSpeeds = {};
		array<int> popped = {};
		array<vector> resampled = {};
		int joinCount;
		if (!RedProof())
		{
			map<int, ref array<int>> joins = new map<int, ref array<int>>();
			LF_DriveRoute.Joins(nodes, roadOf, 6, joins);
			joinCount = joins.Count();
			length = LF_DriveRoute.Search(nodes, roadOf, joins, 0, goal, chain);

			array<vector> straight = {};
			for (int s = 0; s < 20; s++)
				straight.Insert(Vector(s * 2, 0, 0));

			LF_DriveHarness.CornerSpeeds(straight, 2.6, straightSpeeds);

			// A 10 m radius circle, a point every 2 m of arc.
			array<vector> circle = {};
			for (int c = 0; c < 31; c++)
			{
				float angle = c * 0.2;
				circle.Insert(Vector(10 * Math.Cos(angle), 0, 10 * Math.Sin(angle)));
			}

			LF_DriveHarness.CornerSpeeds(circle, 2.6, bendSpeeds);

			array<float> keys = {};
			array<int> values = {};
			array<int> order = {5, 1, 4, 2, 3};
			foreach (int o : order)
				LF_DriveRoute.HeapPush(keys, values, o, o * 10);

			while (!values.IsEmpty())
				popped.Insert(LF_DriveRoute.HeapPop(keys, values));

			array<vector> segment = {};
			segment.Insert(Vector(0, 0, 0));
			segment.Insert(Vector(10, 0, 0));
			LF_DriveHarness.Resample(segment, 2, resampled);
		}

		ctx.True(joinCount >= 4, "the road ends that meet are joined");
		ctx.EqualFloat(200, length, 0.01, "the search takes the 200 m way, not the 400 m detour");
		ctx.True(chain.Count() > 0 && chain[0] == 0 && chain[chain.Count() - 1] == goal, "the chain runs from the start node to the goal node");
		ctx.True(straightSpeeds.Count() == 20 && straightSpeeds[10] >= LF_DriveHarness.CORNER_MAX_MPS - 0.01, "a straight road allows the top speed");
		ctx.True(bendSpeeds.Count() == 31, "one corner speed per point");
		if (bendSpeeds.Count() == 31)
			ctx.EqualFloat(Math.Sqrt(2.6 * 10), bendSpeeds[15], 0.6, "a 10 m bend allows sqrt(grip x radius)");

		ctx.Equal(5, popped.Count(), "the heap gives back every node");
		if (popped.Count() == 5)
		{
			ctx.Equal(10, popped[0], "the heap pops the smallest key first");
			ctx.Equal(50, popped[4], "and the largest last");
		}

		ctx.Equal(6, resampled.Count(), "10 m at a 2 m step is six points");
		if (resampled.Count() == 6)
			ctx.EqualFloat(10, resampled[5][0], 0.01, "the last point is kept");
	}

	protected void AddNode(array<vector> nodes, array<int> roadOf, vector p, int road)
	{
		nodes.Insert(p);
		roadOf.Insert(road);
	}
}
