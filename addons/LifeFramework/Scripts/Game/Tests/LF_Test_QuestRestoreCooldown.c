// red-proof: 2026-11-17 `tools/cli test --no-build --red LF_Test_QuestRestoreCooldown,LF_Test_RacePathStart`: RedProof() restores
// with time 0 (the old rule) and the cooldown assertions fail; RED PROOF OK (all 2 listed). Green with the fix (--only, 20 passed).

// tier: LOGIC
//! A repeatable quest's cooldown survives a respawn (docs/reviews/2026-11-17-life.md #3). The new
//! body restores saved turn-ins; with no turn-in time the cooldown was skipped, so a player could
//! turn in, die, respawn and take the quest again at once, paid every time.
class LF_Test_QuestRestoreCooldown : LF_Test
{
	override string GetName()
	{
		return "quests/restore-cooldown";
	}

	override void Run(LF_TestContext ctx)
	{
		float restoredAt = 100;
		if (RedProof())
			restoredAt = 0;

		LF_QuestState state = new LF_QuestState();
		state.RestoreTurnedIn("q_repeat", restoredAt);
		ctx.True(state.GetStatus() == LF_EQuestStatus.TURNED_IN, "the restored quest is turned in");
		ctx.False(state.CanReaccept(true, 60, 120), "a respawn 20 s in does not lift a 60 s cooldown");
		ctx.True(state.CanReaccept(true, 60, 161), "the cooldown ends 60 s after the restore");
		ctx.False(state.CanReaccept(false, 60, 500), "a one-off quest is never taken again");

		LF_QuestTracker tracker = new LF_QuestTracker();
		ctx.True(tracker.RestoreTurnedIn("q_repeat", restoredAt), "the tracker restores the turn-in");
		ctx.False(tracker.GetState("q_repeat").CanReaccept(true, 60, 120), "the tracker passes the restore time on");
	}
}
