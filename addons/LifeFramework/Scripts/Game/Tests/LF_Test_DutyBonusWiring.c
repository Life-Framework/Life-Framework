// red-proof: 2026-11-23 `tools/cli test --no-build --red LF_Test_DutyBonusWiring,LF_Test_DutyBonusPersist,LF_Test_PoliceCallPay`: RED PROOF OK; "a resolved call earns the $15 bonus (expected 15, got 0)" (the close skipped). Green with --only.
//! The outcome bonuses are wired into play (#4319): a call the officer closes as resolved earns the $15 duty
//! bonus once, no one found and unfounded earn nothing, and resolving your own crime earns nothing. The game
//! mode's LF_MedicDutyBonus.Install is proven by its boot log line (other tests reset the event bus).
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_DutyBonusWiring : LF_Test
{
	protected static const string OFFICER = "duty-wiring-officer";

	override string GetName()
	{
		return "jobs/duty-bonus-wiring";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{

		LF_PoliceCalls.ResetStatics();
		LF_DutyBonus.ResetStatics();
		LF_PlayerAccount officer = LF_PlayerAccountManager.GetOrCreate(OFFICER);
		officer.SetFaction(LF_Faction.POLICE);
		officer.SetPoliceRank(LF_PoliceRanks.PATROL);
		officer.SetOnDuty(true);

		vector scene = Vector(90000, 0, 90000);
		vector atScene = Vector(90005, 0, 90000);
		string key, arg;

		LF_PoliceCall resolved = TakenCall(ctx, "duty-wiring-suspect-0", scene);
		if (!resolved)
			return;

		bool closed;
		if (!RedProof())
			closed = LF_CallOutcome.Close(resolved.m_iId, OFFICER, LF_CallOutcomeKind.RESOLVED, atScene, key, arg);

		ctx.True(closed, "the officer at the scene closes the call as resolved");
		ctx.Equal(LF_DutyBonus.CALL_RESOLVED_AMOUNT, LF_DutyBonus.Pending(officer), "a resolved call earns the $15 bonus");
		ctx.False(LF_CallOutcome.Close(resolved.m_iId, OFFICER, LF_CallOutcomeKind.RESOLVED, atScene, key, arg), "a closed call cannot be closed again");
		ctx.Equal(LF_DutyBonus.CALL_RESOLVED_AMOUNT, LF_DutyBonus.Pending(officer), "the bonus is earned once");

		LF_PoliceCall empty = TakenCall(ctx, "duty-wiring-suspect-1", scene);
		LF_PoliceCall unfounded = TakenCall(ctx, "duty-wiring-suspect-2", scene);
		if (!empty || !unfounded)
			return;

		ctx.True(LF_CallOutcome.Close(empty.m_iId, OFFICER, LF_CallOutcomeKind.NO_ONE_FOUND, atScene, key, arg), "no one found closes");
		ctx.True(LF_CallOutcome.Close(unfounded.m_iId, OFFICER, LF_CallOutcomeKind.UNFOUNDED, atScene, key, arg), "unfounded closes");
		ctx.Equal(LF_DutyBonus.CALL_RESOLVED_AMOUNT, LF_DutyBonus.Pending(officer), "no one found and unfounded earn nothing");

		// An officer resolving the call posted for their own crime earns nothing.
		LF_PoliceCall own = TakenCall(ctx, OFFICER, scene);
		if (!own)
			return;

		ctx.True(LF_CallOutcome.Close(own.m_iId, OFFICER, LF_CallOutcomeKind.RESOLVED, atScene, key, arg), "the own-crime call still closes");
		ctx.Equal(LF_DutyBonus.CALL_RESOLVED_AMOUNT, LF_DutyBonus.Pending(officer), "resolving your own crime earns nothing");
		ctx.True(LF_DutyBonus.SameAccount(OFFICER, OFFICER), "a uid is its own account");
		ctx.False(LF_DutyBonus.SameAccount(OFFICER, "duty-wiring-suspect-0"), "two players are two accounts");

		officer.SetOnDuty(false);
		LF_DutyBonus.ResetStatics();
		LF_PoliceCalls.ResetStatics();
		// The reset dropped the game mode's subscription; put it back for later tests.
		LF_PoliceCalls.Install();
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
}
