// red-proof: `tools\cli test --no-build --only LF_Test_PoliceCallLifecycle` on the unfixed LF_PoliceCalls (2026-11-18) FAILED 7 checks:
// "no call is posted before the report (expected 0, got 1)", "the call carries the report's place (expected 'Square', got '')", "a cleared wanted level closes the call"; the fix turns it green.
// tier: LOGIC
//! Police calls (#2516, parent #234): a crime raises the wanted level before its report is filed, so the
//! call waits for the report and carries its place; a call closes when the suspect stops being wanted.
class LF_Test_PoliceCallLifecycle : LF_Test
{
	protected static const string CRIMINAL = "police-life-criminal";
	protected static const string REPORTED_FIRST = "police-life-reported-first";
	protected static const string ADMIN = "police-life-admin";
	protected static const string OFFICER = "police-life-officer";

	override string GetName()
	{
		return "police/call-lifecycle";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_PoliceCalls.ResetStatics();
		LF_WantedReports.ResetStatics();
		ctx.True(LF_PoliceCalls.Install(), "the calls subscribe to the bus");

		int senior = LF_AccountStateBits.Encode(true, LF_Faction.POLICE, true, 0, LF_PoliceRanks.SENIOR);

		// A crime's real order: the wanted level rises, then the report is filed.
		LF_EventBus.Publish(LF_OnWantedChanged.Create(CRIMINAL, 0, 2));
		ctx.Equal(0, LF_PoliceCalls.OpenCount(), "no call is posted before the report");
		LF_WantedReports.Add(CRIMINAL, LF_WantedReports.OFFENCE_ROBBERY, "", "", "Square", false);
		LF_PoliceCall call = LF_PoliceCalls.FindOpenFor(CRIMINAL);
		ctx.True(call != null, "the report posts the call");
		if (!call)
			return;

		ctx.EqualStr("Square", call.m_sSite, "the call carries the report's place");
		LF_WantedReports.Add(CRIMINAL, LF_WantedReports.OFFENCE_VEHICLE_THEFT, "", "", "Harbour", false);
		ctx.Equal(1, LF_PoliceCalls.OpenCount(), "a second report posts no second call");
		ctx.EqualStr("Square", call.m_sSite, "a second report leaves the site as it was");

		// The suspect stops being wanted (served time, expiry, another officer's arrest): the call closes.
		ctx.True(LF_PoliceCalls.Take(call.m_iId, OFFICER, senior), "an officer takes the call");
		LF_EventBus.Publish(LF_OnWantedChanged.Create(CRIMINAL, 2, 0));
		ctx.True(call.m_bClosed, "a cleared wanted level closes the call");
		ctx.Equal(0, LF_PoliceCalls.OpenCount(), "no open call is left");
		ctx.False(LF_PoliceCalls.Complete(call.m_iId, OFFICER), "a closed call cannot be completed");

		// The report came first: the rise posts at once with the report's place.
		LF_WantedReports.Add(REPORTED_FIRST, LF_WantedReports.OFFENCE_ROBBERY, "", "", "Chapel", false);
		LF_EventBus.Publish(LF_OnWantedChanged.Create(REPORTED_FIRST, 0, 1));
		LF_PoliceCall early = LF_PoliceCalls.FindOpenFor(REPORTED_FIRST);
		ctx.True(early != null, "a report filed first posts the call at the rise");
		if (early)
			ctx.EqualStr("Chapel", early.m_sSite, "the call carries the place of the report filed first");

		// A wanted level that clears before any report was filed leaves no call behind.
		LF_EventBus.Publish(LF_OnWantedChanged.Create(ADMIN, 0, 1));
		LF_EventBus.Publish(LF_OnWantedChanged.Create(ADMIN, 1, 0));
		LF_WantedReports.Add(ADMIN, LF_WantedReports.OFFENCE_ROBBERY, "", "", "Quay", false);
		ctx.True(LF_PoliceCalls.FindOpenFor(ADMIN) == null, "a report filed after the level cleared posts no call");

		LF_PoliceCalls.ResetStatics();
		LF_WantedReports.ResetStatics();
		// The reset dropped the game mode's subscription; put it back for later tests.
		LF_PoliceCalls.Install();
	}
}
