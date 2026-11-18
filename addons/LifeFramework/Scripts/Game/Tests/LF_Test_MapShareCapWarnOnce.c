// red-proof: 2026-11-18, cli test --no-build --only LF_Test_MapShareCapWarnOnce on the unfixed warning (counter seam only): FAIL "three sweeps with the player over the cap warn once (expected 1, got 3)", "going over again after dropping under warns a second time, once (expected 2, got 5)". Green with the once-per-excursion warning.
//------------------------------------------------------------------------------------------------
//! map/share-cap-warn-once (review 2026-11-17, #2514): a player in more than SEND_CAP shares got the "the rest
//! wait" warning on every sweep, every 5 s for the rest of the session. It is logged once, when the player first
//! goes over the cap, and again only after they dropped under it and went over once more.
//! Pure script: one fixture player, EVERYONE rows, and the sweep driven by hand (RefreshAll).
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_MapShareCapWarnOnce : LF_Test
{
	protected static const int PLAYER = 1;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "map/share-cap-warn-once";
	}

	//------------------------------------------------------------------------------------------------
	protected void PutRow(string key)
	{
		LF_MapShares.Put(LF_MapShareRow.Make(key, LF_EMapShareAudience.EVERYONE, "", 1, 1, 0, 0, "cap", 0));
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MapShares.ResetStatics();
		LF_Test_MapShareSink sink = new LF_Test_MapShareSink();
		LF_Test_MapShareFacts facts = new LF_Test_MapShareFacts();
		facts.Set(PLAYER, "cap-warn-1", "", "");
		LF_MapShares.SetSink(sink);
		LF_MapShares.SetFacts(facts);

		// One row over the cap: the player holds SEND_CAP and the last row waits.
		for (int i = 0; i <= LF_MapShares.SEND_CAP; i++)
		{
			PutRow("cap-" + i.ToString());
		}

		ctx.Equal(LF_MapShares.SEND_CAP, LF_MapShares.DeliveredCount(PLAYER), "the player holds the cap, no more");
		ctx.Equal(0, LF_MapShares.CapWarnings(), "a put never logs the sweep's over-cap warning");

		LF_MapShares.RefreshAll();
		LF_MapShares.RefreshAll();
		LF_MapShares.RefreshAll();
		ctx.Equal(1, LF_MapShares.CapWarnings(), "three sweeps with the player over the cap warn once");

		// Under the cap again: nothing to warn about, and the warning is armed for the next time.
		LF_MapShares.Remove("cap-0");
		LF_MapShares.Remove("cap-1");
		LF_MapShares.RefreshAll();
		ctx.Equal(1, LF_MapShares.CapWarnings(), "under the cap logs nothing");

		PutRow("cap-extra");
		LF_MapShares.RefreshAll();
		LF_MapShares.RefreshAll();
		ctx.Equal(1, LF_MapShares.CapWarnings(), "exactly at the cap is not over it");

		PutRow("cap-over");
		LF_MapShares.RefreshAll();
		LF_MapShares.RefreshAll();
		ctx.Equal(2, LF_MapShares.CapWarnings(), "going over again after dropping under warns a second time, once");

		LF_MapShares.ResetStatics();
		LF_MapShares.EnsureStarted();
	}
}
