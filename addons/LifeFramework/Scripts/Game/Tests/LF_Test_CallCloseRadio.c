// red-proof: 2026-11-23 RED PROOF OK, batched with LF_Test_CallCloseRadioCar: `tools\cli test --no-build --red LF_Test_CallCloseRadio` (RedProof() skips every radio close, so no call closes, the caller is never marked, and the sort and board-tag checks fail).
//! The car radio closes the call the officer holds (#4585): resolved, no one found and unfounded close it
//! through LF_CallOutcome.Close; an officer holding no call, another officer, an off-duty officer and a far
//! officer are refused and the call stays open. Three unfounded radio closes mark the caller: their next
//! call is taken after a newer normal call (LF_PoliceCalls.OldestOpenFor) and their dispatch line carries
//! the low-priority tag (LF_Dispatch.BoardLine).
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_CallCloseRadio : LF_Test
{
	protected static const string OFFICER = "radio-officer";
	protected static const string OTHER_OFFICER = "radio-other";
	protected static const string CALLER = "radio-caller";
	protected static const string BYSTANDER = "radio-bystander";
	protected static const int DAY = 4100;

	override string GetName()
	{
		return "police/call-close-radio";
	}

	//------------------------------------------------------------------------------------------------
	protected int OnDuty()
	{
		return LF_AccountStateBits.Encode(true, LF_Faction.POLICE, true, 0, LF_PoliceRanks.PATROL);
	}

	//------------------------------------------------------------------------------------------------
	//! The radio close, skipped under the red proof.
	protected bool Radio(string officer, int state, LF_CallOutcomeKind outcome, vector at, out string key)
	{
		string arg;
		if (RedProof())
			return false;

		return LF_CallCloseAction.ServerClose(officer, state, outcome, at, key, arg);
	}

	//------------------------------------------------------------------------------------------------
	//! Posts a call at the scene for the caller; the officer takes it.
	protected LF_PoliceCall PostAndTake(LF_TestContext ctx, string caller, vector scene, string officer)
	{
		LF_PoliceCall call = LF_PoliceCalls.Post(LF_PoliceCalls.KIND_FINE, caller, "Square", LF_PoliceRanks.PATROL, true, scene);
		ctx.True(call != null, "the call posts for " + caller);
		if (call)
			ctx.True(LF_PoliceCalls.Take(call.m_iId, officer, OnDuty()), "the officer takes the call of " + caller);

		return call;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_PoliceCalls.ResetStatics();
		LF_CallAbuse.ResetStatics();
		LF_ConvictionRecord.SetTodayForTest(DAY);
		LF_WantedReports.ClearFor(CALLER);
		LF_WantedReports.ClearFor(BYSTANDER);
		vector scene = Vector(100, 0, 100);
		vector atScene = Vector(110, 0, 100);
		vector farAway = Vector(900, 0, 900);
		string key, arg;

		// No call held: refused with the "take one at the board" line.
		ctx.False(LF_CallCloseAction.ServerClose(OFFICER, OnDuty(), LF_CallOutcomeKind.RESOLVED, atScene, key, arg), "an officer holding no call closes nothing");
		ctx.EqualStr("#LF-CallOutcome_NoneHeld", key, "the officer is told they hold no call");

		// Resolved at the scene closes the held call.
		LF_PoliceCall call = PostAndTake(ctx, BYSTANDER, scene, OFFICER);
		ctx.True(Radio(OFFICER, OnDuty(), LF_CallOutcomeKind.RESOLVED, atScene, key), "the radio closes the held call as resolved");
		if (call)
		{
			ctx.True(call.m_bClosed, "the resolved call is closed");
			ctx.Equal(LF_CallOutcomeKind.RESOLVED, call.m_iOutcome, "the outcome is resolved");
		}

		// Another officer's call: this officer holds none, the call stays open.
		call = PostAndTake(ctx, BYSTANDER, scene, OTHER_OFFICER);
		ctx.False(LF_CallCloseAction.ServerClose(OFFICER, OnDuty(), LF_CallOutcomeKind.UNFOUNDED, atScene, key, arg), "an officer cannot close another officer's call");
		if (call)
			ctx.False(call.m_bClosed, "another officer's call stays open");

		// Off duty: refused.
		int offDuty = LF_AccountStateBits.Encode(true, LF_Faction.POLICE, false, 0, LF_PoliceRanks.PATROL);
		ctx.False(LF_CallCloseAction.ServerClose(OTHER_OFFICER, offDuty, LF_CallOutcomeKind.RESOLVED, atScene, key, arg), "an off-duty officer closes nothing");

		// Far from the scene: refused, the call stays open; then no one found at the scene closes it.
		ctx.False(LF_CallCloseAction.ServerClose(OTHER_OFFICER, OnDuty(), LF_CallOutcomeKind.NO_ONE_FOUND, farAway, key, arg), "a close far from the scene is refused");
		ctx.EqualStr("#LF-CallOutcome_TooFar", key, "the far officer is told to get closer");
		if (call)
			ctx.False(call.m_bClosed, "the far close leaves the call open");

		ctx.True(Radio(OTHER_OFFICER, OnDuty(), LF_CallOutcomeKind.NO_ONE_FOUND, atScene, key), "no one found at the scene closes it");
		if (call)
			ctx.Equal(LF_CallOutcomeKind.NO_ONE_FOUND, call.m_iOutcome, "the outcome is no one found");

		// A crime report's call has no caller: closing it unfounded charges nobody, not even the suspect.
		PostAndTake(ctx, BYSTANDER, scene, OFFICER);
		ctx.True(Radio(OFFICER, OnDuty(), LF_CallOutcomeKind.UNFOUNDED, atScene, key), "a crime call closes unfounded");
		ctx.Equal(0, LF_CallAbuse.CountFor(BYSTANDER), "an unfounded crime call counts nothing against its suspect");

		// Two held calls: the radio closes the one at the officer's scene, not the older one.
		LF_PoliceCall older = PostAndTake(ctx, "radio-far-suspect", farAway, OFFICER);
		LF_PoliceCall here = PostAndTake(ctx, BYSTANDER, scene, OFFICER);
		ctx.True(Radio(OFFICER, OnDuty(), LF_CallOutcomeKind.RESOLVED, atScene, key), "with two calls held, the radio closes one at the scene");
		if (older && here)
		{
			ctx.True(here.m_bClosed, "the call at the officer's scene is the one closed");
			ctx.False(older.m_bClosed, "the older far call stays open");
		}

		LF_PoliceCalls.CloseFor("radio-far-suspect", "test");

		// Three unfounded radio closes of calls the caller phoned in charge and mark the caller (LF_CallAbuse).
		for (int i = 0; i < LF_CallAbuse.UNFOUNDED_LIMIT; i++)
		{
			LF_CallAbuse.SetCaller(PostAndTake(ctx, CALLER, scene, OFFICER), CALLER);
			ctx.True(Radio(OFFICER, OnDuty(), LF_CallOutcomeKind.UNFOUNDED, atScene, key), "an unfounded radio close " + (i + 1));
		}

		ctx.True(LF_CallAbuse.IsMarked(CALLER), "three unfounded radio closes mark the caller");

		// The marked caller's call is posted first, a normal call after it: the board's take sorts the low one last.
		LF_PoliceCall low = LF_PoliceCalls.Post(LF_PoliceCalls.KIND_FINE, CALLER, "Square", LF_PoliceRanks.PATROL);
		LF_CallAbuse.SetCaller(low, CALLER);
		LF_PoliceCall normal = LF_PoliceCalls.Post(LF_PoliceCalls.KIND_FINE, BYSTANDER, "Square", LF_PoliceRanks.PATROL);
		LF_PoliceCall next = LF_PoliceCalls.OldestOpenFor(LF_PoliceRanks.PATROL);
		ctx.True(next != null && next == normal, "a newer normal call is taken before an older low-priority call");
		if (normal)
			LF_PoliceCalls.CloseFor(BYSTANDER, "test");

		next = LF_PoliceCalls.OldestOpenFor(LF_PoliceRanks.PATROL);
		ctx.True(next != null && next == low, "the low-priority call is still taken when it is the only one");

		// A crime report of the marked player is never tagged (Report publishes it with the suspect's uid).
		LF_Dispatch.ClearLog();
		LF_Dispatch.Report(CALLER, null, LF_WantedReports.OFFENCE_ROBBERY, vector.Zero, 0);
		array<string> crimeEntries = {};
		LF_Dispatch.BuildBoardEntries(1, crimeEntries);
		string tag = WidgetManager.Translate(LF_CallAbuse.LOW_PRIORITY_KEY);
		if (crimeEntries.Count() == 1)
			ctx.False(LF_Dispatch.BoardLine(crimeEntries[0]).Contains(tag), "the marked player's own crime report carries no tag");
		else
			ctx.True(false, "the crime report reaches the dispatch log");

		// The dispatch line of a call the marked caller phones in carries the tag; another caller's does not.
		array<int> nobody = {};
		LF_OnServiceCall marked = LF_OnServiceCall.Create(CALLER, "police", "Square", LF_WantedReports.OFFENCE_ROBBERY, "", vector.Zero, nobody);
		LF_OnServiceCall plain = LF_OnServiceCall.Create(BYSTANDER, "police", "Square", LF_WantedReports.OFFENCE_ROBBERY, "", vector.Zero, nobody);
		LF_Dispatch.Receive(plain);
		LF_Dispatch.Receive(marked);
		array<string> entries = {};
		LF_Dispatch.BuildBoardEntries(2, entries);
		ctx.Equal(2, entries.Count(), "both dispatches are on the board");
		if (entries.Count() == 2)
		{
			string markedLine = LF_Dispatch.BoardLine(entries[0]);
			string plainLine = LF_Dispatch.BoardLine(entries[1]);
			ctx.True(markedLine.Contains(tag), "the marked caller's dispatch line carries the low-priority tag: " + markedLine);
			ctx.False(plainLine.Contains(tag), "another caller's dispatch line has no tag: " + plainLine);
			ctx.True(plainLine.Contains(WidgetManager.Translate("#LF-Dispatch_BoardLoud")), "the flags still read the loud alarm: " + plainLine);
		}

		LF_Dispatch.ClearLog();
		LF_WantedReports.ClearFor(CALLER);
		LF_WantedReports.ClearFor(BYSTANDER);
		LF_CallAbuse.ResetStatics();
		LF_ConvictionRecord.SetTodayForTest(-1);
		LF_PoliceCalls.ResetStatics();
		// The reset dropped the game mode's subscription; put it back for later tests.
		LF_PoliceCalls.Install();
	}
}
