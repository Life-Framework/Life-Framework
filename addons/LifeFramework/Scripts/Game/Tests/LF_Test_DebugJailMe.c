// red-proof: flipped the gate-off assertion to True; "the gate off refuses to jail" failed, restored
// tier: LOGIC
class LF_Test_DebugJailMe : LF_Test
{
	override string GetName()
	{
		return "jail/debug-jail-me";
	}

	override void Run(LF_TestContext ctx)
	{
		string uid = "debug-jail-me-player";
		LF_Jail.Configure(60, 300, 1, 5);
		LF_Jail.SetCellOrigin("48 0 64");

		ctx.False(LF_DebugJailMe.Perform(false, uid, null), "the gate off refuses to jail");
		ctx.False(LF_Jail.IsServing(uid), "a refused request starts no sentence");
		ctx.False(LF_DebugJailMe.Perform(true, "", null), "an empty account id is refused");

		ctx.True(LF_DebugJailMe.Perform(true, uid, null), "the gate on jails through the real jail path");
		ctx.True(LF_Jail.IsServing(uid), "the player is serving");
		ctx.EqualFloat(LF_DebugJailMe.SENTENCE_SECONDS, LF_Jail.GetRemaining(uid), 0.01, "the sentence is the short debug one");
		ctx.False(LF_DebugJailMe.Perform(true, uid, null), "a second press while serving is refused");

		// IsEnabled reads the live game mode (on in the debug mode); Perform takes the flag so both sides are covered above.
	}
}
