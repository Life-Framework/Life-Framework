// tier: LOGIC
// red-proof: 2026-11-17, on the unfixed gate (Admit = LF_RateLimit.Allow COMMS) `--only LF_Test_MapKnownSyncGate` failed "a full comms bucket does not refuse the sync (expected 0, got 2)", plus the DEFER and after-gap asserts. With LF_MapKnownSync.Admit, green.
//! map/known-sync-gate: the join sync of a player's known places (#2241) is not refused by the
//! shared COMMS rate bucket (review 2026-11-17). A player who just made three chess moves or board
//! reads and then respawns still gets the new character's set. A second ask inside a second is
//! deferred, not dropped (it may be the one for the new character); a third while one waits is dropped.
class LF_Test_MapKnownSyncGate : LF_Test
{
	protected static const int PLAYER = 990243;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "map/known-sync-gate";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MapKnownSync.ResetStatics();
		int now = System.GetTickCount();

		// Three comms requests (chess moves, board reads) fill the shared bucket.
		for (int i = 0; i < LF_RateLimit.COMMS_BURST; i++)
			LF_RateLimit.Allow(PLAYER, LF_ERequestKind.COMMS, "test comms", false);

		ctx.Equal(LF_EMapKnownSyncGate.SEND, LF_MapKnownSync.Admit(PLAYER, now), "a full comms bucket does not refuse the sync");
		ctx.Equal(LF_EMapKnownSyncGate.DEFER, LF_MapKnownSync.Admit(PLAYER, now + 100), "a second ask inside the gap waits, not dropped");
		ctx.Equal(LF_EMapKnownSyncGate.DROP, LF_MapKnownSync.Admit(PLAYER, now + 200), "a third ask while one waits is dropped");

		// The deferred send runs (no controller here: nothing goes out) and opens the gate again later.
		LF_MapKnownSync.Deferred(PLAYER);
		int later = System.GetTickCount() + LF_MapKnownSync.MIN_GAP_MS + 1;
		ctx.Equal(LF_EMapKnownSyncGate.SEND, LF_MapKnownSync.Admit(PLAYER, later), "after the gap the next ask is sent");
		ctx.Equal(LF_EMapKnownSyncGate.SEND, LF_MapKnownSync.Admit(PLAYER + 1, now), "another player has their own gate");

		LF_MapKnownSync.ResetStatics();
	}
}
