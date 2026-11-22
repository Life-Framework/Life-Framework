// red-proof: `tools\cli test --no-build --red LF_Test_CallOutcomes` (RedProof() skips the close, so no outcome is recorded and the close checks fail).
//! Call outcomes (#4203, PO-D1): an officer closes a call they hold as resolved, no one found or
//! unfounded; the outcome is recorded on the call; a close from outside the scene radius is refused
//! with a localized reason and leaves the call open.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_CallOutcomes : LF_Test
{
	protected static const string OFFICER = "outcome-officer";
	protected static const string OTHER = "outcome-other";

	override string GetName()
	{
		return "police/call-outcomes";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_PoliceCall TakenCall(LF_TestContext ctx, string suspect, vector scene)
	{
		int patrol = LF_AccountStateBits.Encode(true, LF_Faction.POLICE, true, 0, LF_PoliceRanks.PATROL);
		LF_PoliceCall call = LF_PoliceCalls.Post(LF_PoliceCalls.KIND_FINE, suspect, "Square", LF_PoliceRanks.PATROL, true, scene);
		if (!call)
		{
			ctx.True(false, "the call posts");
			return null;
		}

		ctx.True(LF_PoliceCalls.Take(call.m_iId, OFFICER, patrol), "the officer takes the call");
		return call;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_PoliceCalls.ResetStatics();

		vector scene = Vector(100, 0, 100);
		vector atScene = Vector(110, 0, 100);
		vector far = Vector(100 + LF_PoliceCallPay.Radius() + 500, 0, 100);
		string key, arg;

		// Each outcome is recorded on the call and closes it.
		array<int> kinds = {LF_CallOutcomeKind.RESOLVED, LF_CallOutcomeKind.NO_ONE_FOUND, LF_CallOutcomeKind.UNFOUNDED};
		array<string> expectKeys = {"#LF-CallOutcome_Resolved", "#LF-CallOutcome_NoOneFound", "#LF-CallOutcome_Unfounded"};
		for (int i = 0; i < kinds.Count(); i++)
		{
			string name = LF_CallOutcome.Name(kinds[i]);
			LF_PoliceCall call = TakenCall(ctx, "outcome-suspect-" + i, scene);
			if (!call)
				return;

			ctx.Equal(LF_CallOutcomeKind.NONE, call.m_iOutcome, "an open call has no outcome: " + name);
			bool closed = false;
			if (!RedProof())
				closed = LF_CallOutcome.Close(call.m_iId, OFFICER, kinds[i], atScene, key, arg);

			ctx.True(closed, "the officer at the scene closes it: " + name);
			ctx.Equal(kinds[i], call.m_iOutcome, "the outcome is recorded on the call: " + name);
			ctx.True(call.m_bClosed, "the call is closed: " + name);
			ctx.EqualStr(expectKeys[i], key, "the officer reads the closing line: " + name);
		}

		// Far from the scene: refused in words, the call stays open and has no outcome.
		LF_PoliceCall distant = TakenCall(ctx, "outcome-suspect-far", scene);
		if (!distant)
			return;

		bool farClosed = LF_CallOutcome.Close(distant.m_iId, OFFICER, LF_CallOutcomeKind.UNFOUNDED, far, key, arg);
		ctx.False(farClosed, "closing from far away is refused");
		ctx.EqualStr("#LF-CallOutcome_TooFar", key, "the refusal is a localized reason");
		ctx.True(arg.ToInt() > 0, "the reason names the distance to get within");
		ctx.False(distant.m_bClosed, "a refused close leaves the call open");
		ctx.Equal(LF_CallOutcomeKind.NONE, distant.m_iOutcome, "a refused close records no outcome");

		// Only the officer who holds the call may close it; no outcome means no close.
		ctx.False(LF_CallOutcome.Close(distant.m_iId, OTHER, LF_CallOutcomeKind.RESOLVED, atScene, key, arg), "another officer cannot close it");
		ctx.EqualStr("#LF-CallOutcome_NotHeld", key, "the reason says the call is not theirs");
		ctx.False(LF_CallOutcome.Close(distant.m_iId, OFFICER, LF_CallOutcomeKind.NONE, atScene, key, arg), "no outcome chosen closes nothing");

		// Back at the scene it closes, once.
		ctx.True(LF_CallOutcome.Close(distant.m_iId, OFFICER, LF_CallOutcomeKind.UNFOUNDED, atScene, key, arg), "back at the scene it closes");
		ctx.False(LF_CallOutcome.Close(distant.m_iId, OFFICER, LF_CallOutcomeKind.RESOLVED, atScene, key, arg), "a closed call cannot be closed again");
		ctx.Equal(LF_CallOutcomeKind.UNFOUNDED, distant.m_iOutcome, "the first outcome stands");

		LF_PoliceCalls.ResetStatics();
		// The reset dropped the game mode's subscription; put it back for later tests.
		LF_PoliceCalls.Install();
	}
}
