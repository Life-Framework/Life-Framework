// red-proof: observed 2026-11-15 with `tools\cli test --no-build --red LF_Test_SessionSpawnEvent`: RED-OK, skipping the spawn call failed it with `one spawn publishes one event (expected 1, got 0)`. The batched hook is the red proof; no separate pre-implementation build was made.
//------------------------------------------------------------------------------------------------
//! Collects LF_OnPlayerSpawnedEvent records.
class LF_TestSpawnCollector : LF_EventSubscriber
{
	ref array<ref LF_OnPlayerSpawnedEvent> m_aSeen = {};

	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnPlayerSpawnedEvent spawned = LF_OnPlayerSpawnedEvent.Cast(busEvent);
		if (!spawned)
			return false;

		m_aSeen.Insert(spawned);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! session/spawn-event (#1446): one LF_OnPlayerSpawnedEvent per spawn; the first spawn of an
//! account uid in a session is flagged, a second spawn of the same uid is not, and a reset
//! (world teardown) makes the next spawn a first again.
// tier: LOGIC
class LF_Test_SessionSpawnEvent : LF_Test
{
	override string GetName()
	{
		return "session/spawn-event";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_EventBus.ResetStatics();
		LF_Session.ResetStatics();
		LF_TestSpawnCollector collector = new LF_TestSpawnCollector();
		LF_EventBus.Subscribe(LF_OnPlayerSpawnedEvent, collector);

		if (!RedProof())
			LF_Session.OnPlayerSpawned(3, "uid-a");

		ctx.Equal(1, collector.m_aSeen.Count(), "one spawn publishes one event");
		if (collector.m_aSeen.Count() < 1)
			return;

		ctx.Equal(3, collector.m_aSeen[0].m_iPlayerId, "the event carries the player id");
		ctx.True(collector.m_aSeen[0].m_sAccountUid == "uid-a", "the event carries the account uid");
		ctx.True(collector.m_aSeen[0].m_bFirstThisSession, "the first spawn of a uid is firstThisSession");

		LF_Session.OnPlayerSpawned(3, "uid-a");
		ctx.Equal(2, collector.m_aSeen.Count(), "a respawn publishes another event");
		if (collector.m_aSeen.Count() < 2)
			return;

		ctx.True(!collector.m_aSeen[1].m_bFirstThisSession, "a second spawn of the same uid is not first");

		LF_Session.OnPlayerSpawned(4, "uid-b");
		ctx.Equal(3, collector.m_aSeen.Count(), "another player publishes its own event");
		if (collector.m_aSeen.Count() < 3)
			return;

		ctx.True(collector.m_aSeen[2].m_bFirstThisSession, "a different uid starts its own first spawn");

		LF_Session.ResetStatics();
		LF_Session.OnPlayerSpawned(3, "uid-a");
		ctx.Equal(4, collector.m_aSeen.Count(), "a spawn after the world reset still publishes");
		if (collector.m_aSeen.Count() < 4)
			return;

		ctx.True(collector.m_aSeen[3].m_bFirstThisSession, "the reset forgets which uids were seen");

		LF_EventBus.ResetStatics();
	}
}
