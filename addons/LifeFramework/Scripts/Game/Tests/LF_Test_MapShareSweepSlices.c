// red-proof: 2026-11-17, cli test --no-build --red LF_Test_MapShareSweepSlices: RED-OK map/share-sweep-slices (RedProof() runs the old one-frame RefreshAll per tick, so the first tick refreshes all 45 players, not a batch of 3, and no cycle is counted). Green with SweepStep (22 assertions).
//------------------------------------------------------------------------------------------------
//! map/share-sweep-slices (#2531): the 5 s share sweep refreshed all 128 players in one frame (20 to 50 ms). It is
//! spread over LF_MapShares.SWEEP_SLICES ticks: each tick refreshes a batch of the cycle's players (the list divided
//! by the slice count, rounded up), every player once per cycle; the first tick of a cycle expires rows and forgets
//! players who left; a player who left mid-cycle is skipped, not sent anything.
//! Pure script: 45 fixture players (a batch of 3), an ORG row, the ticks driven by hand.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_MapShareSweepSlices : LF_Test
{
	protected static const int PLAYERS = 45;
	protected static const int BATCH = 3;	// PLAYERS / SWEEP_SLICES, rounded up
	protected static const string ORG = "slice-crew";
	protected static const string ROW = "org:slice-crew:1";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "map/share-sweep-slices";
	}

	//------------------------------------------------------------------------------------------------
	protected void SetAll(LF_Test_MapShareFacts facts, string org)
	{
		for (int i = 1; i <= PLAYERS; i++)
		{
			facts.Set(i, "slice-" + i.ToString(), "", org);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected int Holding()
	{
		int count;
		for (int i = 1; i <= PLAYERS; i++)
		{
			if (LF_MapShares.IsDelivered(i, ROW))
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! One sweep tick; the red proof runs the old one-frame pass instead.
	protected void Tick(float now)
	{
		if (RedProof())
			LF_MapShares.RefreshAll();
		else
			LF_MapShares.SweepStep(now);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MapShares.ResetStatics();
		LF_Test_MapShareSink sink = new LF_Test_MapShareSink();
		LF_Test_MapShareFacts facts = new LF_Test_MapShareFacts();
		SetAll(facts, "");
		LF_MapShares.SetSink(sink);
		LF_MapShares.SetFacts(facts);

		ctx.Equal(20, LF_MapShares.SWEEP_SLICES, "twenty ticks make one cycle");
		ctx.Equal(LF_MapShares.SWEEP_MS, LF_MapShares.SLICE_MS * LF_MapShares.SWEEP_SLICES, "a cycle is the old sweep period");

		LF_MapShares.Put(LF_MapShareRow.Make(ROW, LF_EMapShareAudience.ORG, ORG, 1, 1, 0, 0, "crew", 0));
		ctx.Equal(0, Holding(), "nobody is in the org yet");

		// Everyone joins the org: one tick refreshes one batch, the cycle reaches everyone.
		SetAll(facts, ORG);
		Tick(0);
		ctx.Equal(BATCH, Holding(), "the first tick refreshes one batch, not everyone");
		ctx.True(LF_MapShares.IsDelivered(1, ROW) && LF_MapShares.IsDelivered(BATCH, ROW), "the batch is the first players of the list");

		int tick;
		for (tick = 1; tick < LF_MapShares.SWEEP_SLICES; tick++)
		{
			Tick(0);
		}

		ctx.Equal(PLAYERS, Holding(), "one cycle reaches every player");
		ctx.Equal(1, LF_MapShares.SweepCycles(), "the cycle started once");
		Tick(0);
		ctx.Equal(2, LF_MapShares.SweepCycles(), "the twenty-first tick starts the next cycle");
		ctx.Equal(PLAYERS, CountVerb(sink, "add:"), "each player was sent the row once");

		// Everyone leaves the org; the last player leaves the server after the cycle's first tick.
		LF_MapShares.RestartSweepCycle();
		sink.m_aSent.Clear();
		SetAll(facts, "");
		Tick(0);
		facts.m_aOrder.RemoveItem(PLAYERS);
		for (tick = 1; tick < LF_MapShares.SWEEP_SLICES; tick++)
		{
			Tick(0);
		}

		ctx.Equal(PLAYERS - 1, CountVerb(sink, "remove:"), "players who left the org lose the row");
		ctx.Equal(0, sink.CountFor(PLAYERS), "a player who left the server mid-cycle is sent nothing");
		ctx.True(LF_MapShares.IsDelivered(PLAYERS, ROW), "until the next cycle starts, what they held is kept");
		Tick(0);
		ctx.Equal(0, LF_MapShares.DeliveredCount(PLAYERS), "the next cycle forgets the player who left");

		// Expiry runs on a cycle's first tick only.
		LF_MapShares.Put(LF_MapShareRow.Make("event:slice", LF_EMapShareAudience.EVERYONE, "", 1, 1, 0, 0, "", 10));
		Tick(20);
		ctx.True(LF_MapShares.Has("event:slice"), "an expired row waits for the cycle's first tick");
		LF_MapShares.RestartSweepCycle();
		Tick(20);
		ctx.False(LF_MapShares.Has("event:slice"), "the cycle's first tick expires it");

		LF_MapShares.ResetStatics();
		LF_MapShares.EnsureStarted();
	}

	//------------------------------------------------------------------------------------------------
	protected int CountVerb(LF_Test_MapShareSink sink, string verb)
	{
		int count;
		foreach (string line : sink.m_aSent)
		{
			if (line.StartsWith(verb))
				count++;
		}

		return count;
	}
}
