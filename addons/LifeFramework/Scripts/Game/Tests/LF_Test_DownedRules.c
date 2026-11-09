// red-proof: observed 2026-11-09 with `tools\cli test --no-build --only LF_Test_DownedRules`: LF_Downed.Decide returning PASS instead of ABSORB for an already-downed player failed `a downed player absorbs a lethal hit (expected 2, got 0)`; restored, green.

// tier: LOGIC
//! The pure rule for one hit on a player's Health hit zone (LF_Downed.Decide): lethal damage
//! downs, non-lethal passes, a downed player absorbs every hit, and config off, an NPC, a
//! disallowed unconsciousness, or a real-kill request all leave the hit to vanilla.
class LF_Test_DownedRules : LF_Test
{
	override string GetName()
	{
		return "medic/downed-rules";
	}

	override void Run(LF_TestContext ctx)
	{
		// enabled, eligible, alreadyDowned, permitted, lethalBypass, damage, health
		ctx.Equal(LF_Downed.DOWN, LF_Downed.Decide(true, true, false, true, false, 100, 100), "damage equal to health is lethal: down");
		ctx.Equal(LF_Downed.DOWN, LF_Downed.Decide(true, true, false, true, false, 500, 40), "damage above health is lethal: down");
		ctx.Equal(LF_Downed.PASS, LF_Downed.Decide(true, true, false, true, false, 30, 100), "a survivable hit passes");
		ctx.Equal(LF_Downed.PASS, LF_Downed.Decide(true, true, false, true, false, 0, 100), "no damage passes");
		ctx.Equal(LF_Downed.PASS, LF_Downed.Decide(true, true, false, true, false, -20, 100), "healing passes");

		ctx.Equal(LF_Downed.ABSORB, LF_Downed.Decide(true, true, true, true, false, 5, 100), "a downed player absorbs even a small hit");
		ctx.Equal(LF_Downed.ABSORB, LF_Downed.Decide(true, true, true, true, false, 1000, 10), "a downed player absorbs a lethal hit");
		ctx.Equal(LF_Downed.PASS, LF_Downed.Decide(true, true, true, true, false, -5, 10), "a downed player can still be healed");

		ctx.Equal(LF_Downed.PASS, LF_Downed.Decide(false, true, false, true, false, 500, 100), "config off: lethal damage kills as in vanilla");
		ctx.Equal(LF_Downed.PASS, LF_Downed.Decide(true, false, false, true, false, 500, 100), "an NPC dies as in vanilla");
		ctx.Equal(LF_Downed.PASS, LF_Downed.Decide(true, true, false, false, false, 500, 100), "no unconsciousness in this game mode: lethal damage kills");
		ctx.Equal(LF_Downed.PASS, LF_Downed.Decide(true, true, false, true, true, 500, 100), "a real-kill request passes");
		ctx.Equal(LF_Downed.PASS, LF_Downed.Decide(true, true, true, true, true, 500, 10), "a real kill of a downed player passes");
	}
}
