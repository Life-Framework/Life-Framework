// red-proof: `tools\cli test --no-build --red LF_Test_CallAbuse` (RedProof() skips the unfounded closes, so no count, no charge and no mark appear and the checks fail).
//! 911 abuse (#4421, PO-D2): an officer closing a caller's calls as unfounded counts against the caller;
//! the third one in a game day files one false-report charge and the caller's next calls are posted low
//! priority; a day roll clears the count and the mark. Another caller and other outcomes are untouched.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_CallAbuse : LF_Test
{
	protected static const string OFFICER = "abuse-officer";
	protected static const string CALLER = "abuse-caller";
	protected static const string BYSTANDER = "abuse-bystander";
	protected static const int DAY = 4000;

	override string GetName()
	{
		return "police/call-abuse";
	}

	//------------------------------------------------------------------------------------------------
	//! Posts a call for the caller, the officer takes it and closes it with the outcome at the scene.
	protected bool CloseCall(LF_TestContext ctx, string caller, LF_CallOutcomeKind outcome, string label)
	{
		vector scene = Vector(100, 0, 100);
		int patrol = LF_AccountStateBits.Encode(true, LF_Faction.POLICE, true, 0, LF_PoliceRanks.PATROL);
		LF_PoliceCall call = LF_PoliceCalls.Post(LF_PoliceCalls.KIND_FINE, caller, "Square", LF_PoliceRanks.PATROL, true, scene);
		if (!call)
		{
			ctx.True(false, "the call posts: " + label);
			return false;
		}

		LF_CallAbuse.SetCaller(call, caller);

		ctx.True(LF_PoliceCalls.Take(call.m_iId, OFFICER, patrol), "the officer takes the call: " + label);
		string key, arg;
		bool closed = false;
		if (!RedProof())
			closed = LF_CallOutcome.Close(call.m_iId, OFFICER, outcome, Vector(110, 0, 100), key, arg);

		ctx.True(closed, "the officer closes it: " + label);
		return closed;
	}

	//------------------------------------------------------------------------------------------------
	protected int ChargesFor(string caller)
	{
		int charges;
		array<ref LF_WantedReportRecord> reports = LF_WantedReports.GetFor(caller);
		if (!reports)
			return 0;

		foreach (LF_WantedReportRecord report : reports)
		{
			if (report.m_sOffence == LF_CallAbuse.OFFENCE_FALSE_REPORT)
				charges++;
		}

		return charges;
	}

	//------------------------------------------------------------------------------------------------
	protected bool NextCallIsLow(LF_TestContext ctx, string caller, string label)
	{
		LF_PoliceCall call = LF_PoliceCalls.Post(LF_PoliceCalls.KIND_FINE, caller, "Square", LF_PoliceRanks.PATROL);
		if (!call)
		{
			ctx.True(false, "the next call posts: " + label);
			return false;
		}

		LF_CallAbuse.SetCaller(call, caller);

		bool low = call.m_bLowPriority;
		LF_PoliceCalls.CloseFor(caller, "test");
		return low;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_PoliceCalls.ResetStatics();
		LF_CallAbuse.ResetStatics();
		LF_ConvictionRecord.SetTodayForTest(DAY);
		LF_WantedReports.ClearFor(CALLER);
		LF_WantedReports.ClearFor(BYSTANDER);

		// Two unfounded closes file nothing and mark nobody.
		CloseCall(ctx, CALLER, LF_CallOutcomeKind.UNFOUNDED, "first");
		CloseCall(ctx, CALLER, LF_CallOutcomeKind.UNFOUNDED, "second");
		ctx.Equal(0, ChargesFor(CALLER), "two unfounded closes file nothing");
		ctx.Equal(2, LF_CallAbuse.CountFor(CALLER), "two are counted against the caller");
		ctx.False(NextCallIsLow(ctx, CALLER, "after two"), "two unfounded closes leave the next call normal");

		// A resolved close is not abuse.
		CloseCall(ctx, CALLER, LF_CallOutcomeKind.RESOLVED, "resolved");
		ctx.Equal(2, LF_CallAbuse.CountFor(CALLER), "a resolved close is not counted");

		// The third files one report with the law book's false-report code.
		CloseCall(ctx, CALLER, LF_CallOutcomeKind.UNFOUNDED, "third");
		ctx.Equal(1, ChargesFor(CALLER), "the third unfounded close files one report with the false-report code");
		ctx.True(LF_CallAbuse.IsMarked(CALLER), "the caller is marked");

		// The fourth does not file a second charge in the same day.
		CloseCall(ctx, CALLER, LF_CallOutcomeKind.UNFOUNDED, "fourth");
		ctx.Equal(1, ChargesFor(CALLER), "a fourth unfounded close the same day files no second report");

		// The caller's next call carries the low-priority flag; another caller's does not.
		ctx.True(NextCallIsLow(ctx, CALLER, "after the charge"), "the caller's next call carries the low-priority flag");
		LF_PoliceCall tagged = LF_PoliceCalls.Post(LF_PoliceCalls.KIND_FINE, CALLER, "Square", LF_PoliceRanks.PATROL);
		LF_CallAbuse.SetCaller(tagged, CALLER);
		ctx.EqualStr(LF_CallAbuse.LOW_PRIORITY_KEY, LF_CallAbuse.BoardTag(tagged), "the board tag names the mark");
		LF_PoliceCalls.CloseFor(CALLER, "test");
		ctx.False(NextCallIsLow(ctx, BYSTANDER, "bystander"), "another caller's call stays normal");
		ctx.Equal(0, ChargesFor(BYSTANDER), "another caller is not charged");

		// The day roll resets the count and the mark.
		LF_ConvictionRecord.SetTodayForTest(DAY + 1);
		ctx.Equal(0, LF_CallAbuse.CountFor(CALLER), "the count resets at the day roll");
		ctx.False(LF_CallAbuse.IsMarked(CALLER), "the mark resets at the day roll");
		ctx.False(NextCallIsLow(ctx, CALLER, "next day"), "the next day's call is normal again");

		LF_WantedReports.ClearFor(CALLER);
		LF_WantedReports.ClearFor(BYSTANDER);
		LF_CallAbuse.ResetStatics();
		LF_ConvictionRecord.SetTodayForTest(-1);
		LF_PoliceCalls.ResetStatics();
		// The reset dropped the game mode's subscription; put it back for later tests.
		LF_PoliceCalls.Install();
	}
}
