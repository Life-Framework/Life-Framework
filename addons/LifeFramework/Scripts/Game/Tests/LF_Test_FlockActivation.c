// red-proof: 2026-11-23 `tools/cli.mjs test --no-build --red LF_Test_FlockActivation`: RED-OK atmosphere/flock-activation (fails with the hook on, 16 assertions).
//! atmosphere/flock-activation (#4403): the server wakes a bird flock a layer placed (a named entity) when
//! a player walks up to it, once per approach, and leaves the flocks the world's bird manager spawns
//! (unnamed) to the manager. That a woken flock then shows its birds is proved on the island
//! (`square-harbor-life-proof`): the test grid has no AIWorld, so a flock cannot wake there.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_FlockActivation : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "atmosphere/flock-activation";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		bool placed;
		bool spawned = true;
		bool near;
		bool far = true;
		bool disarmed = true;
		bool tooSoon = true;
		bool rearmedNear = true;
		bool rearmedFar;
		if (!RedProof())
		{
			placed = LF_FlockActivation.IsPlaced("Fit_saint_philippe_square_life_pigeons_a");
			spawned = LF_FlockActivation.IsPlaced("");
			near = LF_FlockActivation.ShouldWake(true, 30, 600000);
			far = LF_FlockActivation.ShouldWake(true, 200, 600000);
			disarmed = LF_FlockActivation.ShouldWake(false, 30, 600000);
			tooSoon = LF_FlockActivation.ShouldWake(true, 30, 5000);
			rearmedNear = LF_FlockActivation.Rearms(30);
			rearmedFar = LF_FlockActivation.Rearms(200);
		}

		ctx.True(placed, "a flock a layer named is placed: the server wakes it");
		ctx.False(spawned, "an unnamed flock is the bird manager's: left alone");
		ctx.True(near, "a player 30 m from an armed flock wakes it");
		ctx.False(far, "a player 200 m away does not");
		ctx.False(disarmed, "a flock already woken for this approach stays asleep");
		ctx.False(tooSoon, "a flock woken 5 s ago does not wake again");
		ctx.False(rearmedNear, "a player still on the square does not re-arm the flock");
		ctx.True(rearmedFar, "a player who left (200 m) re-arms it");
	}
}
