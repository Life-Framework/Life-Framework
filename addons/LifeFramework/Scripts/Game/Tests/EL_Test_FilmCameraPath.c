// red-proof: NOT YET OBSERVED. On 2026-11-07 the quarter-time assertion was
// broken (15.625 -> 15.0) and the fast suite was run
// (`cli test --no-build --tier fast`). One boot caught a compile error;
// every later boot compiled, then stopped at "Unable to start replication"
// (servers from other worktrees kept taking UDP 2001), so no FAIL line was
// printed. Replace this with the observed FAIL line on the next free boot.

// tier: LOGIC
class EL_Test_FilmCameraPath : EL_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "film/camera-path";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(EL_TestContext ctx)
	{
		LF_FilmCameraPath empty = new LF_FilmCameraPath();
		vector position;
		vector lookAt;
		float fov;
		ctx.False(empty.Sample(1, position, lookAt, fov), "an empty path has nothing to sample");

		LF_FilmCameraPath path = new LF_FilmCameraPath();
		ctx.True(path.Add(0, "0 10 0", "0 0 100", 60), "first key accepted");
		ctx.True(path.Add(10, "100 10 0", "100 0 100", 40), "later key accepted");
		ctx.False(path.Add(10, "5 5 5", "0 0 0", 60), "a key at the same time is rejected");
		ctx.False(path.Add(4, "5 5 5", "0 0 0", 60), "an earlier key is rejected");
		ctx.Equal(2, path.Count(), "rejected keys are not stored");
		ctx.EqualFloat(10, path.GetDuration(), 0.001, "duration is the last key time");

		ctx.True(path.Sample(5, position, lookAt, fov), "mid-path sample");
		ctx.EqualFloat(50, position[0], 0.001, "smoothstep midpoint is halfway");
		ctx.EqualFloat(10, position[1], 0.001, "height holds between equal keys");
		ctx.EqualFloat(50, fov, 0.001, "fov eases with the position");

		path.Sample(2.5, position, lookAt, fov);
		ctx.EqualFloat(15.625, position[0], 0.001, "quarter time eases in (smoothstep 0.25 = 0.15625)");
		ctx.EqualFloat(15.625, lookAt[0], 0.001, "look-at follows the same ease");

		path.Sample(-3, position, lookAt, fov);
		ctx.EqualFloat(0, position[0], 0.001, "before the first key clamps to it");
		path.Sample(99, position, lookAt, fov);
		ctx.EqualFloat(100, position[0], 0.001, "after the last key clamps to it");
		ctx.EqualFloat(40, fov, 0.001, "after the last key keeps its fov");
	}
}
