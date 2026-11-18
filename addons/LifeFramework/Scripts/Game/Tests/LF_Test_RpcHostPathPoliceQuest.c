// red-proof: RedProof() skips both host-path asks: expected lines "the police arrest ask reaches its handler on the server" and "the quest log ask reaches its handler on the server" (run it with `--red`).
//! #2534: a listen host's own client-to-server ask must reach its handler. LF_AskPoliceArrest and
//! AskRequestLog branch on Replication.IsServer() and call the handler directly; before, the bare
//! Rpc was dropped on the host and nothing ran.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_RpcHostPathPoliceQuest : LF_Test
{
	override string GetName()
	{
		return "core/rpc-host-path-police-quest";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity entity = LF_TestCharacterFixture.Spawn(ctx);
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(entity);
		if (!character)
		{
			ctx.Fail("the fixture character is not a SCR_ChimeraCharacter");
			return;
		}

		ctx.True(Replication.IsServer(), "the test boot is the server");

		int before = character.LF_PoliceArrestRequests();
		if (!RedProof())
			character.LF_AskPoliceArrest("test-rpc-host-path-nobody");
		ctx.Equal(before + 1, character.LF_PoliceArrestRequests(), "the police arrest ask reaches its handler on the server");

		LF_QuestTrackerComponent tracker = LF_QuestTrackerComponent.Cast(entity.FindComponent(LF_QuestTrackerComponent));
		ctx.True(tracker != null, "the fixture character carries a quest tracker");
		if (tracker)
		{
			int pushes = tracker.GetLogPushes();
			if (!RedProof())
				tracker.AskRequestLog();
			ctx.Equal(pushes + 1, tracker.GetLogPushes(), "the quest log ask reaches its handler on the server");
		}

		LF_TestCharacterFixture.Delete(entity);
	}
}
