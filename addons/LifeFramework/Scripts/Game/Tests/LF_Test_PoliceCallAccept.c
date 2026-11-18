// red-proof: ran `tools\cli test --no-build --only LF_Test_PoliceCallAccept` before LF_PoliceCalls existed
// (2026-11-18); the run failed to compile. The RedProof() hook skips the officer's take for `--red`.

//! Police calls (#2477, parent #234): a first wanted report posts a FINE call; an officer takes it only
//! on duty and at or above its rank; one officer per call; Complete closes it once.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_PoliceCallAccept : LF_Test
{
	protected static const string SUSPECT = "police-call-suspect";
	protected static const string OFFICER_A = "police-call-officer-a";
	protected static const string OFFICER_B = "police-call-officer-b";

	override string GetName()
	{
		return "police/call-accept";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_PoliceCalls.ResetStatics();
		LF_WantedReports.ResetStatics();

		int senior = LF_AccountStateBits.Encode(true, LF_Faction.POLICE, true, 0, LF_PoliceRanks.SENIOR);
		int patrol = LF_AccountStateBits.Encode(true, LF_Faction.POLICE, true, 0, LF_PoliceRanks.PATROL);
		int offDuty = LF_AccountStateBits.Encode(true, LF_Faction.POLICE, false, 0, LF_PoliceRanks.SENIOR);

		LF_PoliceCall call = LF_PoliceCalls.Post(LF_PoliceCalls.KIND_FINE, SUSPECT, "test site", LF_PoliceRanks.SENIOR);
		ctx.True(call != null, "a call is posted");
		if (!call)
			return;

		ctx.Equal(1, LF_PoliceCalls.OpenCount(), "one open call");
		ctx.EqualStr(SUSPECT, call.m_sSuspectUid, "the call names the suspect");
		ctx.EqualStr("test site", call.m_sSite, "the call names the site");
		ctx.Equal(LF_PoliceRanks.SENIOR, call.m_iMinRank, "the call carries its minimum rank");

		ctx.False(LF_PoliceCalls.Take(call.m_iId, OFFICER_A, offDuty), "an off-duty officer is refused");
		ctx.False(LF_PoliceCalls.Take(call.m_iId, OFFICER_A, patrol), "an on-duty officer below the rank is refused");
		ctx.EqualStr("", call.m_sOfficerUid, "a refused take leaves the call open");

		bool taken;
		if (!RedProof())
			taken = LF_PoliceCalls.Take(call.m_iId, OFFICER_A, senior);

		ctx.True(taken, "an on-duty officer at the rank takes it");
		ctx.EqualStr(OFFICER_A, call.m_sOfficerUid, "the call names its officer");
		ctx.False(LF_PoliceCalls.Take(call.m_iId, OFFICER_B, senior), "a second officer is refused");

		ctx.False(LF_PoliceCalls.Complete(call.m_iId, OFFICER_B), "only the officer who took it completes it");
		ctx.True(LF_PoliceCalls.Complete(call.m_iId, OFFICER_A), "Complete closes the call");
		ctx.False(LF_PoliceCalls.Complete(call.m_iId, OFFICER_A), "Complete closes it once");
		ctx.Equal(0, LF_PoliceCalls.OpenCount(), "no open call is left");

		// The bus path: a first wanted report (0 to above 0) posts a FINE call at the report's place.
		LF_WantedReports.Add(SUSPECT + "-bus", LF_WantedReports.OFFENCE_ROBBERY, "", "", "Square", false);
		ctx.True(LF_PoliceCalls.Install(), "the calls subscribe to the bus");
		LF_EventBus.Publish(LF_OnWantedChanged.Create(SUSPECT + "-bus", 0, 2));
		ctx.Equal(1, LF_PoliceCalls.OpenCount(), "a first wanted report posts a call");
		LF_PoliceCall posted = LF_PoliceCalls.FindOpenFor(SUSPECT + "-bus");
		ctx.True(posted != null, "the call is for the reported suspect");
		if (posted)
		{
			ctx.EqualStr(LF_PoliceCalls.KIND_FINE, posted.m_sKind, "the call is a fine");
			ctx.EqualStr("Square", posted.m_sSite, "the site is where the report was filed");
		}

		LF_EventBus.Publish(LF_OnWantedChanged.Create(SUSPECT + "-bus", 2, 3));
		LF_EventBus.Publish(LF_OnWantedChanged.Create(SUSPECT + "-bus", 0, 1));
		ctx.Equal(1, LF_PoliceCalls.OpenCount(), "a raise or a repeat does not post a second call");

		LF_PoliceCalls.ResetStatics();
		LF_WantedReports.ResetStatics();
	}
}
