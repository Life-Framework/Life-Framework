// red-proof: Observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_SessionSlots`: not releasing the slot in LF_PlayerIdentity.Forget failed `- the rejoined client is the same identity as before: 00bbbddd-5e55-0000-0000-000000000003`. Restored; green.

// tier: LOGIC
//! An agent-box session keys a client by a slot, not by its player id, so a client that quits and
//! rejoins (a new player id) is the same account again.
class LF_Test_SessionSlots : LF_Test
{
	override string GetName()
	{
		return "session/slots";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_PlayerIdentity.ResetStatics();

		string first = LF_PlayerIdentity.Resolve(11, "", true, true, true, 0);
		string second = LF_PlayerIdentity.Resolve(12, "", true, true, true, 0);
		ctx.True(first == LF_Utils.BuildSessionIdentity(1), "the first client is slot 1: " + first);
		ctx.True(second == LF_Utils.BuildSessionIdentity(2), "the second client is slot 2: " + second);
		ctx.True(LF_PlayerIdentity.Resolve(12, "", true, true, true, 5000) == second, "a player keeps its slot on every lookup");

		// Client 2 quits; its game rejoins as player 13. Client 1 is still connected.
		LF_PlayerIdentity.Forget(12);
		string rejoined = LF_PlayerIdentity.Resolve(13, "", true, true, true, 9000);
		ctx.True(rejoined == second, "the rejoined client is the same identity as before: " + rejoined);

		// A third client takes the next free slot, not a held one.
		string third = LF_PlayerIdentity.Resolve(14, "", true, true, true, 9000);
		ctx.True(third == LF_Utils.BuildSessionIdentity(3), "the third client is slot 3: " + third);

		// Client 1 quits: the lowest free slot is 1, whichever client comes next.
		LF_PlayerIdentity.Forget(11);
		ctx.True(LF_PlayerIdentity.SlotOf(15) == 1, "a freed slot is handed out again, lowest first");

		LF_PlayerIdentity.ResetStatics();
	}
}
