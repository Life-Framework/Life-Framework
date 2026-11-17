// red-proof: RedProof() skips the ask: expected lines "the quest log ask reaches its handler on the server", "the quest log reply reaches a locally owned body" and "the client cache holds the encoded log" (run it with `--red`).
// tier: WORLD
//! #2619: a listen host's own quest log must arrive. PushLog replies with Rpc(RpcDo_Log), RplRcver.Owner.
//! Observed on build 26266485: when this machine owns the replicated body, that Rpc runs the handler
//! here at once, so PushLog needs no direct call. This test keeps it that way.
class LF_Test_QuestLogHostReply : LF_Test
{
	protected int m_iReceived;

	override string GetName()
	{
		return "quests/log-host-reply";
	}

	protected void OnLog(string serialized)
	{
		m_iReceived++;
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity entity = LF_TestCharacterFixture.Spawn(ctx);
		if (!entity)
		{
			ctx.Fail("the fixture character did not spawn");
			return;
		}

		ctx.True(Replication.IsServer(), "the test boot is the server");
		ctx.True(LF_NetworkUtils.IsOwner(entity), "the fixture body is owned by this machine");
		RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
		ctx.True(rpl && rpl.Id().IsValid(), "the fixture body is registered for replication (a real Rpc, not a local fallback)");

		LF_QuestTrackerComponent tracker = LF_QuestTrackerComponent.Cast(entity.FindComponent(LF_QuestTrackerComponent));
		ctx.True(tracker != null, "the fixture character carries a quest tracker");
		if (!tracker)
		{
			LF_TestCharacterFixture.Delete(entity);
			return;
		}

		m_iReceived = 0;
		tracker.GetOnLogReceived().Insert(OnLog);

		int pushes = tracker.GetLogPushes();
		if (!RedProof())
			tracker.AskRequestLog();
		ctx.Equal(pushes + 1, tracker.GetLogPushes(), "the quest log ask reaches its handler on the server");
		ctx.Equal(1, m_iReceived, "the quest log reply reaches a locally owned body");

		string expected = LF_QuestLogFormat.Encode(tracker.GetTracker().GetStates());
		ctx.True(m_iReceived == 1 && tracker.GetClientLogCache() == expected, "the client cache holds the encoded log");

		tracker.GetOnLogReceived().Remove(OnLog);
		LF_TestCharacterFixture.Delete(entity);
	}
}
