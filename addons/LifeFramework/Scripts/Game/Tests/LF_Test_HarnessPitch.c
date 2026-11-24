// red-proof: `tools\cli test --no-build --red LF_Test_HarnessPitch` (2026-11-24): RED-OK harness/absolute-pitch
// (fails with the hook on, 6 assertions); green with `--only LF_Test_HarnessPitch`.

//! The harness's absolute pitch (`input CharacterPitch <deg>`, #4803). The booking-desk proof turned the suspect's
//! view with relative steps; one step sent while the client stalled after paying a fine was lost, and every later
//! sweep landed 30 degrees low, on the floor: "no interaction for ~90 s". An absolute pitch lands on the same view
//! from any start, so a lost or repeated command cannot leave the view tilted.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_HarnessPitch : LF_Test
{
	override string GetName()
	{
		return "harness/absolute-pitch";
	}

	override void Run(LF_TestContext ctx)
	{
		// A view already 30 degrees low (the lost turn), yaw 1.2 rad, asked for -30.
		vector drifted = Vector(1.2, -30 * Math.DEG2RAD, 0);
		vector level = Vector(1.2, 0, 0);
		vector fromDrift = drifted;
		vector fromLevel = level;
		if (!RedProof())
		{
			fromDrift = LF_AimHarness.PitchTo(drifted, -30);
			fromLevel = LF_AimHarness.PitchTo(level, -30);
		}

		ctx.EqualFloat(-30, fromDrift[1] * Math.RAD2DEG, 0.01, "from a drifted view the pitch lands on -30");
		ctx.EqualFloat(-30, fromLevel[1] * Math.RAD2DEG, 0.01, "from a level view the pitch lands on -30");
		ctx.EqualFloat(1.2, fromDrift[0], 0.0001, "the yaw is kept");

		vector twice = LF_AimHarness.PitchTo(LF_AimHarness.PitchTo(level, -20), -20);
		ctx.EqualFloat(-20, twice[1] * Math.RAD2DEG, 0.01, "sent twice, the view does not move further");

		// The game-side input allowlist takes it (a refused action answers action_not_allowed).
		LF_AgentCommand cmd = new LF_AgentCommand();
		ctx.True(LF_AgentCommand.Parse("6 input CharacterPitch -30", cmd), "input CharacterPitch is allowed: " + cmd.Error);

		vector steep = LF_AimHarness.PitchTo(level, -120);
		ctx.EqualFloat(-LF_AimHarness.PITCH_LIMIT_DEG, steep[1] * Math.RAD2DEG, 0.01, "a pitch past the limit is clamped");
	}
}
