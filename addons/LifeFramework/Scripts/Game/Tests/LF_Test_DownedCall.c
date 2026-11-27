//------------------------------------------------------------------------------------------------
//! Records the medic calls and closes a test publishes.
class LF_DownedCallTestSink : LF_EventSubscriber
{
	ref array<ref LF_OnServiceCall> m_aCalls = {};
	ref array<string> m_aClosed = {};

	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnServiceCall call = LF_OnServiceCall.Cast(busEvent);
		if (call && call.m_sService == LF_DownedCalls.SERVICE)
			m_aCalls.Insert(call);

		LF_OnServiceCallClosed closed = LF_OnServiceCallClosed.Cast(busEvent);
		if (closed && closed.m_sService == LF_DownedCalls.SERVICE)
			m_aClosed.Insert(closed.m_sCallerUid);
		return false;
	}
}

//------------------------------------------------------------------------------------------------
// red-proof: 2026-11-27, cli test --no-build --red LF_Test_DownedCall: RED-OK. RedProof() skips LF_DownedCalls.Install, which is
//   the bug (#5152: nothing subscribed to LF_OnPlayerDowned but the incident log): no call is placed, and "a revive closes the
//   call", "a second downing opens a new call" and "a death closes the call" fail. Green with the subscriber.
//! medic/downed-call (#5152): a player who goes down pages the on-duty medic through the one service
//! call path (a medic board row at the body), the downed medic is not paged about themselves, an NPC
//! body pages nobody, and a revive or a death closes the call (board row gone).
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_DownedCall : LF_Test
{
	protected static const string VICTIM = "downed-call-victim";
	protected static const int VICTIM_ID = 9611;
	protected static const int MEDIC_ID = 9612;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "medic/downed-call";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_EventBus.ResetStatics();
		LF_ServiceBoard.ResetStatics();
		LF_ServiceBoard.Install();
		LF_DownedCalls.ResetStatics();
		if (!RedProof())
			LF_DownedCalls.Install();

		LF_DownedCallTestSink sink = new LF_DownedCallTestSink();
		LF_EventBus.Subscribe(LF_OnServiceCall, sink);
		LF_EventBus.Subscribe(LF_OnServiceCallClosed, sink);

		vector body = "1200 0 1300";
		int now = System.GetTickCount();
		array<string> entries = {};
		int before = LF_ServiceBoard.BuildEntries("medic", now, 50, entries);

		// The subscriber reads the connected players; drive the same path with test candidates.
		array<ref LF_ServiceCandidate> players = {};
		players.Insert(LF_ServiceCandidate.Make(VICTIM_ID, false, LF_EJobType.UNEMPLOYED));
		players.Insert(LF_ServiceCandidate.Make(MEDIC_ID, false, LF_EJobType.MEDIC));

		// The real path: LF_Downed publishes LF_OnPlayerDowned on the bus.
		LF_DownedCalls.SetTestPlayers(players, VICTIM_ID);
		LF_EventBus.Publish(LF_OnPlayerDowned.Create(VICTIM, "", body));

		ctx.Equal(1, sink.m_aCalls.Count(), "a downed player pages the on-duty medic");
		if (sink.m_aCalls.Count() == 1)
		{
			LF_OnServiceCall call = sink.m_aCalls[0];
			ctx.EqualStr(VICTIM, call.m_sCallerUid, "the downed player is the caller");
			ctx.EqualStr(LF_DownedCalls.SUMMARY, call.m_sSummary, "the call reads as a person down");
			ctx.True(vector.Distance(body, call.m_vPosition) < 0.1, "the call carries the body's position (the map mark)");
			ctx.True(call.m_aReceiverIds.Contains(MEDIC_ID), "the medic receives it");
			ctx.False(call.m_aReceiverIds.Contains(VICTIM_ID), "the downed player does not");
		}
		ctx.Equal(before + 1, LF_ServiceBoard.BuildEntries("medic", now, 50, entries), "the call is on the medic board");

		LF_EventBus.Publish(LF_OnPlayerRevived.Create(VICTIM, "medic-uid", body));
		ctx.True(sink.m_aClosed.Contains(VICTIM), "a revive closes the call");
		ctx.Equal(before, LF_ServiceBoard.BuildEntries("medic", now, 50, entries), "the board row leaves with it");

		// Down again, then die: the death closes it.
		LF_EventBus.Publish(LF_OnPlayerDowned.Create(VICTIM, "", body));
		ctx.True(LF_DownedCalls.IsOpen(VICTIM), "a second downing opens a new call");
		sink.m_aClosed.Clear();
		LF_EventBus.Publish(LF_OnPlayerKilled.Create(VICTIM, ""));
		ctx.True(sink.m_aClosed.Contains(VICTIM), "a death closes the call");
		ctx.False(LF_DownedCalls.IsOpen(VICTIM), "nothing stays open");

		// A downed medic alone is not paged about themselves.
		array<ref LF_ServiceCandidate> loneMedic = {};
		loneMedic.Insert(LF_ServiceCandidate.Make(MEDIC_ID, false, LF_EJobType.MEDIC));
		sink.m_aCalls.Clear();
		ctx.True(LF_DownedCalls.OnDowned("downed-call-medic", body, loneMedic, MEDIC_ID) == null, "a lone medic who goes down places no call");
		ctx.Equal(0, sink.m_aCalls.Count(), "nobody is paged");

		// An NPC body (a casualty places its own call) pages nobody.
		ctx.True(LF_DownedCalls.OnDowned("npc", body, players, -1) == null, "an NPC body places no call");
		ctx.Equal(0, sink.m_aCalls.Count(), "still nobody paged");

		LF_EventBus.Unsubscribe(LF_OnServiceCall, sink);
		LF_EventBus.Unsubscribe(LF_OnServiceCallClosed, sink);
		// Leave the game's own subscriptions as a booted server has them.
		LF_DownedCalls.ResetStatics();
		LF_ServiceBoard.ResetStatics();
		LF_EventBus.ResetStatics();
		LF_ServiceBoard.Install();
		LF_DownedCalls.Install();
	}
}
