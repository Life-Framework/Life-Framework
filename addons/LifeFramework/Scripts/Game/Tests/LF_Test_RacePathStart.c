// red-proof: 2026-11-17 `tools/cli test --no-build --red LF_Test_QuestRestoreCooldown,LF_Test_RacePathStart`: RedProof() skips Lose()
// and reuses the body id; "after a tick with no living body the path starts again" and "a new body (respawn) starts a new
// path" failed, RED-OK racing/path-start. Green with the fix (--only, 20 passed).

// tier: LOGIC
//! A racer's checkpoint path never joins two bodies (docs/reviews/2026-11-17-life.md #2). The tick
//! kept the last spot while the racer had no living body, so the first tick after a respawn tested
//! the line from where they died to the hospital and passed any checkpoint on it.
class LF_Test_RacePathStart : LF_Test
{
	override string GetName()
	{
		return "racing/path-start";
	}

	override void Run(LF_TestContext ctx)
	{
		EntityID first = EntityID.FromInt(0, 101);
		EntityID second = EntityID.FromInt(0, 202);
		if (RedProof())
			second = first;

		LF_RaceEntrant racer = new LF_RaceEntrant();
		vector a = "-5010 0 -5010";
		vector b = "-5020 0 -5010";
		vector c = "-5900 0 -5900";
		vector d = "-5300 0 -5300";

		ctx.True(racer.Step(first, a) == a, "the first tick starts the path where the body stands");
		ctx.True(racer.Step(first, b) == a, "the same body joins its path to the last spot");

		if (!RedProof())
			racer.Lose();
		ctx.True(racer.Step(first, c) == c, "after a tick with no living body the path starts again");

		ctx.True(racer.Step(second, d) == d, "a new body (respawn) starts a new path");
	}
}
