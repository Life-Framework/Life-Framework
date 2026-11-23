// red-proof: 2026-11-23 RED-OK police/phoned-call: `tools/cli.mjs test --no-build --red LF_Test_PhonedPoliceCall` (batched with the pack leg's police-car-rows test) (RedProof() skips every phone call, so no call posts, none is taken or closed and the caller is never marked).
//! A player's 911 call becomes a police call an officer can take and close (#4606). A police service call
//! published through LF_ServiceCalls.Place posts a PHONED call at the phone's position with the caller named
//! on it; a second call while it is open posts nothing new; the board's take hands it to an on-duty officer
//! and the car radio closes it at the scene. A crime report posts no phoned call. Three unfounded closes of
//! a player's phoned calls mark them, and their next phoned call is posted low priority and taken after a
//! newer normal call. An officer who phoned the call in earns no bonus for closing it.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_PhonedPoliceCall : LF_Test
{
	protected static const string OFFICER = "phoned-officer";
	protected static const string CALLER = "phoned-caller";
	protected static const string OTHER = "phoned-other";
	protected static const int DAY = 4606;

	override string GetName()
	{
		return "police/phoned-call";
	}

	//------------------------------------------------------------------------------------------------
	protected int OnDuty()
	{
		return LF_AccountStateBits.Encode(true, LF_Faction.POLICE, true, 0, LF_PoliceRanks.PATROL);
	}

	//------------------------------------------------------------------------------------------------
	//! One on-duty officer, so the call is answered.
	protected array<ref LF_ServiceCandidate> Officers()
	{
		array<ref LF_ServiceCandidate> players = {};
		players.Insert(LF_ServiceCandidate.Make(1, true, LF_EJobType.UNEMPLOYED));
		return players;
	}

	//------------------------------------------------------------------------------------------------
	//! The caller phones the police from a payphone at the phone's position; skipped under the red proof.
	protected void Phone(string callerUid, vector phone)
	{
		if (RedProof())
			return;

		LF_ServiceCalls.Place(callerUid, "police", "Square", "#LF-Service_Police", phone, Officers());
	}

	//------------------------------------------------------------------------------------------------
	//! The officer takes the oldest call at the board and closes it from the car radio with outcome.
	protected LF_PoliceCall TakeAndClose(LF_TestContext ctx, string officer, LF_CallOutcomeKind outcome, vector at)
	{
		LF_PoliceCall call = LF_PoliceCalls.OldestOpenFor(LF_PoliceRanks.PATROL);
		if (!call)
			return null;

		ctx.True(LF_PoliceCalls.Take(call.m_iId, officer, OnDuty()), "the officer takes call #" + call.m_iId);
		string key, arg;
		ctx.True(LF_CallCloseAction.ServerClose(officer, OnDuty(), outcome, at, key, arg), "the radio closes call #" + call.m_iId + ": " + key);
		return call;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_PoliceCalls.ResetStatics();
		LF_CallAbuse.ResetStatics();
		LF_Dispatch.ClearLog();
		LF_ServiceCallDispatch.Install();
		LF_ConvictionRecord.SetTodayForTest(DAY);
		LF_WantedReports.ClearFor(CALLER);
		LF_WantedReports.ClearFor(OTHER);
		vector phone = Vector(200, 0, 300);
		vector atScene = Vector(210, 0, 300);

		// The 911 call becomes a takeable call at the phone, the caller named on it.
		Phone(CALLER, phone);
		LF_PoliceCall call = LF_PoliceCalls.FindOpenPhonedBy(CALLER);
		ctx.True(call != null, "a 911 call from the payphone posts a police call");
		if (call)
		{
			ctx.EqualStr(LF_PoliceCalls.KIND_PHONED, call.m_sKind, "the call is a phoned call");
			ctx.EqualStr(CALLER, call.m_sCallerUid, "the caller is named on the call");
			ctx.True(call.m_sSuspectUid.IsEmpty(), "a phoned call has no suspect");
			ctx.True(call.m_bHasPosition && vector.DistanceXZ(call.m_vPosition, phone) < 0.1, "the call's scene is the phone");
			ctx.False(call.m_bLowPriority, "a first-time caller's call is normal priority");
		}

		// Calling again while the call is open posts nothing new.
		int open = LF_PoliceCalls.OpenCount();
		Phone(CALLER, phone);
		ctx.Equal(open, LF_PoliceCalls.OpenCount(), "a second call while the first is open posts no new call");

		// A crime report posts no phoned call (it posts its own call through the wanted level).
		LF_Dispatch.Report(OTHER, null, LF_WantedReports.OFFENCE_ROBBERY, vector.Zero, 0);
		ctx.True(LF_PoliceCalls.FindOpenPhonedBy(OTHER) == null, "a crime report is not a phoned call");

		// The officer takes it at the board and resolves it from the car radio at the phone.
		LF_PoliceCall taken = TakeAndClose(ctx, OFFICER, LF_CallOutcomeKind.RESOLVED, atScene);
		ctx.True(taken != null && taken == call, "the board hands the officer the phoned call");
		if (call)
		{
			ctx.True(call.m_bClosed, "the phoned call is closed");
			ctx.Equal(LF_CallOutcomeKind.RESOLVED, call.m_iOutcome, "the phoned call closed as resolved");
		}

		// Three unfounded closes of the caller's phoned calls charge and mark them.
		for (int i = 0; i < LF_CallAbuse.UNFOUNDED_LIMIT; i++)
		{
			Phone(CALLER, phone);
			TakeAndClose(ctx, OFFICER, LF_CallOutcomeKind.UNFOUNDED, atScene);
		}

		ctx.Equal(LF_CallAbuse.UNFOUNDED_LIMIT, LF_CallAbuse.CountFor(CALLER), "each unfounded close counts against the caller");
		ctx.True(LF_CallAbuse.IsMarked(CALLER), "three unfounded closes mark the caller");

		// The marked caller phones first, another player after: the newer normal call is taken first.
		Phone(CALLER, phone);
		Phone(OTHER, phone);
		LF_PoliceCall low = LF_PoliceCalls.FindOpenPhonedBy(CALLER);
		LF_PoliceCall normal = LF_PoliceCalls.FindOpenPhonedBy(OTHER);
		ctx.True(low != null && low.m_bLowPriority, "the marked caller's next call is low priority");
		ctx.True(normal != null && LF_PoliceCalls.OldestOpenFor(LF_PoliceRanks.PATROL) == normal, "a newer normal call is taken before the low-priority call");

		// An officer closing a call they phoned in themselves earns no bonus (no bonus is even tried).
		if (normal)
		{
			LF_PoliceCalls.Take(normal.m_iId, OTHER, OnDuty());
			int refusals = LF_DutyBonus.GetRefusalCount();
			string key, arg;
			ctx.True(LF_CallCloseAction.ServerClose(OTHER, OnDuty(), LF_CallOutcomeKind.RESOLVED, atScene, key, arg), "the officer closes their own phoned call");
			ctx.Equal(refusals, LF_DutyBonus.GetRefusalCount(), "no bonus is tried for an officer's own phoned call");
		}

		// An automatic tip (the chain smell lead) is takeable but names nobody: closing it unfounded counts nothing.
		int tipCount = LF_CallAbuse.CountFor(OTHER);
		LF_PoliceCall tip;
		if (!RedProof())
		{
			LF_ServiceCalls.Place(OTHER, "police", "Square", "#LF-Chain_SmellLead", phone, Officers(), true);
			tip = LF_PoliceCalls.OldestOpenFor(LF_PoliceRanks.PATROL);
		}

		ctx.True(tip != null && tip != normal && tip != low, "a tip posts a takeable call");
		if (tip)
		{
			ctx.True(tip.m_sCallerUid.IsEmpty(), "a tip names no caller");
			LF_PoliceCalls.Take(tip.m_iId, OFFICER, OnDuty());
			string tipKey, tipArg;
			LF_CallOutcome.Close(tip.m_iId, OFFICER, LF_CallOutcomeKind.UNFOUNDED, atScene, tipKey, tipArg);
			ctx.Equal(tipCount, LF_CallAbuse.CountFor(OTHER), "an unfounded tip counts nothing against the player who noticed it");
		}

		LF_PoliceCalls.ResetStatics();
		LF_CallAbuse.ResetStatics();
		LF_WantedReports.ClearFor(CALLER);
		LF_WantedReports.ClearFor(OTHER);
		LF_Dispatch.ClearLog();
		LF_ConvictionRecord.SetTodayForTest(-1);
		// The reset dropped the game mode's subscription; put it back for later tests.
		LF_PoliceCalls.Install();
	}
}
