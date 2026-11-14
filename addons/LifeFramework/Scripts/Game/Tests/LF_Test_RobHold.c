// red-proof: Observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_RobHold`: made
// LF_RobHoldRules.Judge end the hold at `distance >= radius`; got `[LFTEST]   - standing on the radius
// still counts (expected 0, got 2)`. Restored `distance > radius`; green.
// tier: LOGIC
//! The rules of a held robbery (#936, LF_RobHoldRules): what ends a hold and in which order, the
//! timer text, the bar, the edge warning, when the attendant begs, and the message and the
//! attendant's cry for every way it ends.
class LF_Test_RobHold : LF_Test
{
	override string GetName()
	{
		return "crime/rob-hold";
	}

	override void Run(LF_TestContext ctx)
	{
		// present, downed, armed, distance, radius, police near, elapsed, hold
		ctx.Equal(LF_ERobHoldEnd.NONE, LF_RobHoldRules.Judge(true, false, true, 2, 6, false, 10000, 30000), "armed, in range, no police, time left: still holding");
		ctx.Equal(LF_ERobHoldEnd.PAID, LF_RobHoldRules.Judge(true, false, true, 2, 6, false, 30000, 30000), "the hold kept to the end pays");
		ctx.Equal(LF_ERobHoldEnd.LEFT, LF_RobHoldRules.Judge(true, false, true, 6.1, 6, false, 10000, 30000), "a step past the radius ends it as left");
		ctx.Equal(LF_ERobHoldEnd.NONE, LF_RobHoldRules.Judge(true, false, true, 6, 6, false, 10000, 30000), "standing on the radius still counts");
		ctx.Equal(LF_ERobHoldEnd.WEAPON, LF_RobHoldRules.Judge(true, false, false, 2, 6, false, 10000, 30000), "putting the weapon away ends it");
		ctx.Equal(LF_ERobHoldEnd.POLICE, LF_RobHoldRules.Judge(true, false, true, 2, 6, true, 10000, 30000), "an officer arriving ends it");
		ctx.Equal(LF_ERobHoldEnd.DOWNED, LF_RobHoldRules.Judge(true, true, false, 2, 6, false, 10000, 30000), "a downed robber has no weapon up, and is told he went down");
		ctx.Equal(LF_ERobHoldEnd.GONE, LF_RobHoldRules.Judge(false, false, false, 0, 6, false, 10000, 30000), "a robber who left the server ends it as gone");
		ctx.Equal(LF_ERobHoldEnd.POLICE, LF_RobHoldRules.Judge(true, false, true, 2, 6, true, 30000, 30000), "police at the last moment beat the payout");
		ctx.Equal(LF_ERobHoldEnd.LEFT, LF_RobHoldRules.Judge(true, false, true, 9, 6, true, 10000, 30000), "running off with police near is told as leaving");

		ctx.EqualStr("0:30", LF_RobHoldRules.Clock(30000), "the timer starts at the full hold");
		ctx.EqualStr("0:20", LF_RobHoldRules.Clock(19500), "a part second rounds up");
		ctx.EqualStr("0:05", LF_RobHoldRules.Clock(5000), "seconds under ten are padded");
		ctx.EqualStr("1:15", LF_RobHoldRules.Clock(75000), "minutes show");
		ctx.EqualStr("0:00", LF_RobHoldRules.Clock(-300), "it never counts below zero");

		ctx.True(Math.AbsFloat(LF_RobHoldRules.Progress(15000, 30000) - 0.5) < 0.001, "halfway fills half the bar");
		ctx.True(LF_RobHoldRules.Progress(40000, 30000) == 1, "the bar never overfills");
		ctx.True(LF_RobHoldRules.Progress(-10, 30000) == 0, "the bar starts empty");

		ctx.False(LF_RobHoldRules.NearEdge(4.4, 6), "well inside the radius: no warning");
		ctx.True(LF_RobHoldRules.NearEdge(4.5, 6), "three quarters of the radius: the warning shows");

		ctx.Equal(0, LF_RobHoldRules.BegStage(11000, 30000), "the attendant opens the till before he begs");
		ctx.Equal(1, LF_RobHoldRules.BegStage(12000, 30000), "he begs at two fifths of the hold");
		ctx.Equal(2, LF_RobHoldRules.BegStage(22000, 30000), "he stammers near the end");

		array<int> ends = {LF_ERobHoldEnd.PAID, LF_ERobHoldEnd.LEFT, LF_ERobHoldEnd.WEAPON, LF_ERobHoldEnd.POLICE, LF_ERobHoldEnd.DOWNED, LF_ERobHoldEnd.TILL};
		foreach (int end : ends)
		{
			string key = LF_RobHoldRules.EndKey(end);
			ctx.True(!key.IsEmpty(), string.Format("ending %1 has a message", end));
		}

		ctx.EqualStr("", LF_RobHoldRules.EndKey(LF_ERobHoldEnd.GONE), "a robber who left the server is told nothing");
		ctx.EqualStr("#LF-RobHold_ClerkPaid", LF_RobHoldRules.ClerkKey(LF_ERobHoldEnd.PAID), "the attendant hands it over");
		ctx.EqualStr("#LF-RobHold_ClerkPolice", LF_RobHoldRules.ClerkKey(LF_ERobHoldEnd.POLICE), "the attendant calls to the officer");
		ctx.EqualStr("#LF-RobHold_ClerkGone", LF_RobHoldRules.ClerkKey(LF_ERobHoldEnd.LEFT), "the attendant cries for help when the robber runs");
		ctx.EqualStr("", LF_RobHoldRules.ClerkKey(LF_ERobHoldEnd.TILL), "a till that could not pay leaves the attendant silent");

		ctx.EqualStr("#LF-RobHold_Refused_Cooldown", LF_RobHoldRules.RefusalKey(LF_RobAction.REFUSED_COOLDOWN), "an emptied till says so");
		ctx.EqualStr("#LF-RobHold_Refused_Police", LF_RobHoldRules.RefusalKey(LF_RobAction.REFUSED_POLICE), "too few officers says so");
		ctx.EqualStr("#LF-RobHold_Refused_Civilian", LF_RobHoldRules.RefusalKey(LF_RobAction.REFUSED_NOT_CIVILIAN), "an officer cannot rob");
		ctx.EqualStr("#LF-RobHold_Refused_Busy", LF_RobHoldRules.RefusalKey(LF_RobHold.REFUSED_BUSY), "a till already being robbed says so");
		ctx.EqualStr("#LF-RobHold_Refused_Weapon", LF_RobHoldRules.RefusalKey(LF_RobHold.REFUSED_WEAPON), "no weapon says so");
		ctx.EqualStr("#LF-RobHold_Refused_NoTill", LF_RobHoldRules.RefusalKey(LF_RobHold.REFUSED_REACH), "out of the till's reach falls back to no till here");
	}
}
