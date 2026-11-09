// red-proof: observed 2026-11-08 with ClampTickDelta passing the raw delta and NeedsSync always
// true: FAIL survival/sync "a 60 s hitch applies at most the configured 5 s", "a sub-point drift
// does not bump replication".

//------------------------------------------------------------------------------------------------
//! Server-side survival tick helpers: the frame delta is clamped so a hitch does not drop a big
//! chunk, and replication bumps only when a whole point or the low-needs state changes.
// tier: LOGIC
class LF_Test_SurvivalSync : LF_Test
{
	override string GetName()
	{
		return "survival/sync";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.True(Math.AbsFloat(LF_CharacterSurvivalComponent.ClampTickDelta(60, 5) - 5) < 0.001, "a 60 s hitch applies at most the configured 5 s");
		ctx.True(Math.AbsFloat(LF_CharacterSurvivalComponent.ClampTickDelta(-3, 5)) < 0.001, "a negative delta clamps to zero");
		ctx.True(Math.AbsFloat(LF_CharacterSurvivalComponent.ClampTickDelta(1, 5) - 1) < 0.001, "a normal delta passes through");

		ctx.False(LF_CharacterSurvivalComponent.NeedsSync(50, 50, false, 49.7, 50.2, false), "a sub-point drift does not bump replication");
		ctx.True(LF_CharacterSurvivalComponent.NeedsSync(50, 50, false, 48.9, 50, false), "a whole-point hunger change bumps");
		ctx.True(LF_CharacterSurvivalComponent.NeedsSync(50, 50, false, 50, 51.2, false), "a whole-point thirst change bumps");
		ctx.True(LF_CharacterSurvivalComponent.NeedsSync(20, 50, false, 20, 50, true), "a low-needs flip bumps");
	}
};
