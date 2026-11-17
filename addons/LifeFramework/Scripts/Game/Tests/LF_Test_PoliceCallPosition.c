// red-proof: on main (before LF_WantedReports.AddAt, LF_PoliceCallBoardAction.CallOrigin and LF_PoliceCall.m_vPosition existed) this test did not compile (2026-11-18); `--red` skips the position filing, so the call has no point for a map-label site.

// tier: WORLD
//! police/call-position (#2578, parent #2516): a call keeps the position its crime was filed at, so a site
//! named only by a map location label still gets a waypoint. The position comes from the report (the event
//! when the call waits for the report, the stored report when the report came first); a report with no
//! position falls back to the site text, which a bare label cannot resolve.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_PoliceCallPosition : LF_Test
{
	protected static const string LATE = "police-pos-late";
	protected static const string EARLY = "police-pos-early";
	protected static const string NONE = "police-pos-none";
	//! A place text no sign carries and no grid prefix: what a map location label looks like to SiteOrigin.
	protected static const string LABEL = "Test Map Label";

	override string GetName()
	{
		return "police/call-position";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_PoliceCalls.ResetStatics();
		LF_WantedReports.ResetStatics();
		ctx.True(LF_PoliceCalls.Install(), "the calls subscribe to the bus");

		vector filedAt = Vector(1234, 7, 4321);
		vector point;

		// The label alone has no point: this is the bug the position fixes.
		ctx.False(LF_PoliceCallBoardAction.SiteOrigin(LABEL, point), "a map-label site alone has no point to mark");

		// A crime's real order: the wanted level rises, the report is filed after, the event carries the position.
		LF_EventBus.Publish(LF_OnWantedChanged.Create(LATE, 0, 1));
		if (!RedProof())
			LF_WantedReports.AddAt(LATE, LF_WantedReports.OFFENCE_ROBBERY, "", "", LABEL, false, filedAt);
		else
			LF_WantedReports.Add(LATE, LF_WantedReports.OFFENCE_ROBBERY, "", "", LABEL, false);

		LF_PoliceCall late = LF_PoliceCalls.FindOpenFor(LATE);
		ctx.True(late != null, "the report posts the call");
		if (late)
		{
			ctx.EqualStr(LABEL, late.m_sSite, "the call keeps the label as its site text");
			ctx.True(late.m_bHasPosition, "the call keeps the position the crime was filed at");
			ctx.True(LF_PoliceCallBoardAction.CallOrigin(late, point), "taking the call has a point to mark");
			ctx.Equal(1234, Math.Round(point[0]), "the waypoint is at the filed position (x)");
			ctx.Equal(4321, Math.Round(point[2]), "the waypoint is at the filed position (z)");
		}

		// The report came first: the rise posts the call from the stored report.
		LF_WantedReports.AddAt(EARLY, LF_WantedReports.OFFENCE_ROBBERY, "", "", LABEL, false, filedAt);
		LF_EventBus.Publish(LF_OnWantedChanged.Create(EARLY, 0, 1));
		LF_PoliceCall early = LF_PoliceCalls.FindOpenFor(EARLY);
		ctx.True(early != null, "a report filed first posts the call at the rise");
		if (early)
		{
			ctx.True(early.m_bHasPosition, "the call keeps the position of the report filed first");
			ctx.True(LF_PoliceCallBoardAction.CallOrigin(early, point), "that call has a point to mark");
			ctx.Equal(1234, Math.Round(point[0]), "the waypoint is at the report's position (x)");
		}

		// A report with no position keeps today's behaviour: the label has no point, a grid text still does.
		LF_WantedReports.Add(NONE, LF_WantedReports.OFFENCE_ROBBERY, "", "", LABEL, false);
		LF_PoliceCall none = LF_PoliceCalls.Post(LF_PoliceCalls.KIND_FINE, NONE, LABEL, LF_PoliceRanks.PATROL);
		ctx.True(none != null, "a call with no position is posted");
		if (none)
		{
			ctx.False(none.m_bHasPosition, "a call posted without a position has none");
			ctx.False(LF_PoliceCallBoardAction.CallOrigin(none, point), "a label with no position still has no point");
		}

		LF_PoliceCall grid = LF_PoliceCalls.Post(LF_PoliceCalls.KIND_FINE, NONE + "-grid", LF_WantedReports.GridPlace(Vector(250, 0, 450)), LF_PoliceRanks.PATROL);
		if (grid)
		{
			ctx.True(LF_PoliceCallBoardAction.CallOrigin(grid, point), "a grid site still resolves without a position");
			ctx.Equal(250, Math.Round(point[0]), "the grid site's point is the square's centre (x)");
		}

		LF_PoliceCalls.ResetStatics();
		LF_WantedReports.ResetStatics();
		// The reset dropped the game mode's subscription; put it back for later tests.
		LF_PoliceCalls.Install();
	}
}
