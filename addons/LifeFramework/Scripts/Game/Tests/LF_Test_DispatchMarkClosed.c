// red-proof: `tools\cli test --no-build --only LF_Test_DispatchMarkClosed` on main (before LF_OnServiceCallClosed) FAILED
//   "clearing the suspect's wanted level removes their mark"; the fix turns it green.
// tier: WORLD
//! comms/dispatch-mark-closed (#2579, finding 10 of #2495): a crime dispatch puts a police mark on the map; the
//! mark leaves when its suspect's wanted level returns to 0, and another suspect's mark stays. The crime goes
//! through LF_Dispatch.Report (it sounds a real alarm at the site, so this is a WORLD test); the wanted change
//! is the bus event LF_PoliceCalls closes its calls on. One fixture officer and a recording sink, no network.
class LF_Test_DispatchMarkClosed : LF_Test
{
	protected static const int OFFICER = 1;
	protected static const string SUSPECT_A = "mark-closed-suspect-a";
	protected static const string SUSPECT_B = "mark-closed-suspect-b";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "comms/dispatch-mark-closed";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MapShares.ResetStatics();
		LF_ServiceCallMapMarks.ResetStatics();
		LF_Dispatch.ResetStatics();
		LF_WantedReports.ResetStatics();
		LF_ServiceCallMapMarks.Install();
		RunClosed(ctx);
		LF_Dispatch.StopAllAlarms();
		LF_MapShares.ResetStatics();
		LF_ServiceCallMapMarks.ResetStatics();
		LF_Dispatch.ResetStatics();
		LF_WantedReports.ResetStatics();
		// The resets dropped the game mode's subscriptions; put back the ones the dispatch mark test
		// owns. Not the wanted-change one: LF_WantedPosters.Install trusts any listener on that event
		// and would then skip its own subscriptions in a later test.
		LF_MapShares.EnsureStarted();
		LF_ServiceCallMapMarks.Install();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunClosed(LF_TestContext ctx)
	{
		LF_DispatchMarkTestSink sink = new LF_DispatchMarkTestSink();
		LF_Test_MapShareFacts facts = new LF_Test_MapShareFacts();
		facts.Set(OFFICER, "char-officer", LF_MapShares.ROLE_POLICE, "");
		LF_MapShares.SetSink(sink);
		LF_MapShares.SetFacts(facts);

		ctx.NotNull(LF_Dispatch.Report(SUSPECT_A, null, LF_WantedReports.OFFENCE_ROBBERY, Vector(100, 0, 200), 1), "suspect A's crime is dispatched");
		ctx.NotNull(LF_Dispatch.Report(SUSPECT_B, null, LF_WantedReports.OFFENCE_ROBBERY, Vector(300, 0, 400), 1), "suspect B's crime is dispatched");
		ctx.Equal(2, LF_ServiceCallMapMarks.Count(), "each crime dispatch adds a mark");
		ctx.True(LF_MapShares.Has("dispatch:1") && LF_MapShares.Has("dispatch:2"), "both marks are in the share table");

		// The suspects are wanted, then A stops being wanted (served time, expiry, an arrest).
		LF_EventBus.Publish(LF_OnWantedChanged.Create(SUSPECT_A, 0, 2));
		LF_EventBus.Publish(LF_OnWantedChanged.Create(SUSPECT_B, 0, 1));
		ctx.Equal(2, LF_ServiceCallMapMarks.Count(), "a rising wanted level removes no mark");

		sink.m_aSent.Clear();
		if (!RedProof())
			LF_EventBus.Publish(LF_OnWantedChanged.Create(SUSPECT_A, 2, 0));

		ctx.False(LF_MapShares.Has("dispatch:1"), "clearing the suspect's wanted level removes their mark");
		ctx.True(sink.Got("remove:1:dispatch:1"), "the mark leaves the officer's map");
		ctx.True(LF_MapShares.Has("dispatch:2"), "another suspect's mark stays");
		ctx.Equal(1, LF_ServiceCallMapMarks.Count(), "one mark is left");

		// A level change that is still wanted leaves B's mark; the clear removes it.
		LF_EventBus.Publish(LF_OnWantedChanged.Create(SUSPECT_B, 1, 3));
		ctx.True(LF_MapShares.Has("dispatch:2"), "a wanted level that stays above 0 keeps the mark");
		LF_EventBus.Publish(LF_OnWantedChanged.Create(SUSPECT_B, 3, 0));
		ctx.False(LF_MapShares.Has("dispatch:2"), "clearing the second suspect removes their mark too");
		ctx.Equal(0, LF_ServiceCallMapMarks.Count(), "no dispatch mark is left");
	}
}
