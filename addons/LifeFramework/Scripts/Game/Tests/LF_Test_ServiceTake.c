// red-proof: observed 2026-11-09 with `toolsi test --no-build --only LF_Test_ServiceTake`: LF_ServiceBoard.Take ignoring the per-call taker limit failed it with `- the second medic loses: one taker per medic call (expected 2, got 0)`. Restored; green.
//------------------------------------------------------------------------------------------------
//! Counts LF_OnServiceCallTaken events.
class LF_TestTakenCounter : LF_EventSubscriber
{
	int m_iCount;
	string m_sLastCaller;
	string m_sLastTaker;

	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnServiceCallTaken taken = LF_OnServiceCallTaken.Cast(busEvent);
		if (!taken)
			return false;

		m_iCount++;
		m_sLastCaller = taken.m_sCallerUid;
		m_sLastTaker = taken.m_sTakerUid;
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! comms/take (#251): two medics take the same call and one wins; a civilian cannot take; the
//! board shows the call taken; police allow two takers; each take publishes one event carrying
//! the caller's uid.
// tier: WORLD
class LF_Test_ServiceTake : LF_Test
{
	override string GetName()
	{
		return "comms/take";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_EventBus.ResetStatics();
		LF_Dispatch.ResetStatics();
		LF_ServiceBoard.ResetStatics();
		LF_ServiceCallDispatch.ResetStatics();
		LF_ServiceCallDispatch.Install();
		LF_ServiceBoard.Install();
		LF_TestTakenCounter counter = new LF_TestTakenCounter();
		LF_EventBus.Subscribe(LF_OnServiceCallTaken, counter);

		LF_ServiceCandidate medicA = LF_ServiceCandidate.Make(1, false, LF_EJobType.MEDIC);
		LF_ServiceCandidate medicB = LF_ServiceCandidate.Make(2, false, LF_EJobType.MEDIC);
		LF_ServiceCandidate civilian = LF_ServiceCandidate.Make(3, false, LF_EJobType.UNEMPLOYED);
		LF_ServiceCandidate officerA = LF_ServiceCandidate.Make(4, true, LF_EJobType.UNEMPLOYED);
		LF_ServiceCandidate officerB = LF_ServiceCandidate.Make(5, true, LF_EJobType.UNEMPLOYED);
		LF_ServiceCandidate officerC = LF_ServiceCandidate.Make(6, true, LF_EJobType.UNEMPLOYED);
		array<ref LF_ServiceCandidate> everyone = {};
		everyone.Insert(medicA);
		everyone.Insert(medicB);
		everyone.Insert(civilian);
		everyone.Insert(officerA);
		everyone.Insert(officerB);
		everyone.Insert(officerC);

		int now = System.GetTickCount();
		array<string> entries = {};

		ctx.Equal(LF_EServiceTake.NO_CALL, LF_ServiceTake.ServerTake(1, "medic-a", "medic", everyone), "an empty board has nothing to take");

		LF_ServiceCalls.Place("caller-uid", "medic", "Village payphone", "#LF-Payphone_Summary_Emergency", "96.5 0 263", everyone);
		LF_ServiceBoard.BuildEntries("medic", now, 10, entries);
		ctx.True(entries.Count() == 1 && entries[0].Contains("|0|"), "the new call shows untaken");

		ctx.Equal(LF_EServiceTake.NOT_ON_DUTY, LF_ServiceTake.ServerTake(3, "civ", "medic", everyone), "a civilian cannot take a medic call");
		ctx.Equal(LF_EServiceTake.NOT_ON_DUTY, LF_ServiceTake.ServerTake(4, "officer-a", "medic", everyone), "an officer cannot take a medic call");
		ctx.Equal(0, counter.m_iCount, "refused takes publish nothing");

		ctx.Equal(LF_EServiceTake.TAKEN, LF_ServiceTake.ServerTake(1, "medic-a", "medic", everyone), "the first medic takes the call");
		ctx.Equal(LF_EServiceTake.ALREADY_TAKEN, LF_ServiceTake.ServerTake(2, "medic-b", "medic", everyone), "the second medic loses: one taker per medic call");
		ctx.Equal(LF_EServiceTake.ALREADY_YOURS, LF_ServiceTake.ServerTake(1, "medic-a", "medic", everyone), "the winner taking again is told it is theirs");
		ctx.Equal(1, counter.m_iCount, "exactly one taken event for the one take");
		ctx.True(counter.m_sLastCaller == "caller-uid", "the event names the caller");
		ctx.True(counter.m_sLastTaker == "medic-a", "the event names the taker");

		LF_ServiceBoard.BuildEntries("medic", now, 10, entries);
		ctx.True(entries.Count() == 1 && entries[0].Contains("|1|"), "the board shows the call taken");

		ctx.Equal(2, LF_ServiceCallsConfig.MaxTakers("police"), "police config allows two takers");
		// Police allow two takers per call (LF_ServiceCalls.conf).
		LF_ServiceCalls.Place("caller-uid", "police", "Village payphone", "#LF-Payphone_Summary_Crime", "96.5 0 263", everyone);
		ctx.Equal(LF_EServiceTake.TAKEN, LF_ServiceTake.ServerTake(4, "officer-a", "police", everyone), "the first officer takes the police call");
		ctx.Equal(LF_EServiceTake.TAKEN, LF_ServiceTake.ServerTake(5, "officer-b", "police", everyone), "a second officer may also take it");
		ctx.Equal(LF_EServiceTake.ALREADY_TAKEN, LF_ServiceTake.ServerTake(6, "officer-c", "police", everyone), "a third officer is told it is taken");
		ctx.Equal(3, counter.m_iCount, "three takes published three events in all");

		// A call past its expiry cannot be taken.
		int expiry = LF_ServiceCallsConfig.BoardExpirySeconds();
		LF_ServiceBoardCall none;
		ctx.Equal(LF_EServiceTake.NO_CALL, LF_ServiceBoard.Take("medic", "medic-b", 1, now + (expiry + 1) * 1000, none), "an expired call is gone");

		LF_EventBus.Unsubscribe(LF_OnServiceCallTaken, counter);
		LF_ServiceBoard.ResetStatics();
		LF_EventBus.ResetStatics();
		LF_Dispatch.ResetStatics();
		LF_ServiceCallDispatch.ResetStatics();
	}
}
