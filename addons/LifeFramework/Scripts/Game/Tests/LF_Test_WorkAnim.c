// red-proof: observed 2026-11-10 with `tools/cli test --no-build --only LF_Test_WorkAnim`: the range rule widened to 99999 failed it with `- walking out of range stops it (expected 2, got 0)`; restored; green.
//------------------------------------------------------------------------------------------------
//! work/anim (#513): the lockpick and the vault drill play a working pose through the server's
//! work-animation sessions. The graph and the six base-game clips load, the loiter data drives the
//! graph the way the NPC attachment expects, one session runs per worker, and a cancel, a finish,
//! walking out of range or a dead worker stops it. Broadcasts are silenced; the test counts the
//! starts and stops the server decided.
// tier: WORLD
class LF_Test_WorkAnim : LF_Test
{
	override string GetName()
	{
		return "work/anim";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.True(Resource.Load(LF_WorkAnimPlayer.PICK_GRAPH).IsValid(), "the pick graph loads");
		ctx.True(Resource.Load(LF_WorkAnimPlayer.PICK_GRAPH_INSTANCE).IsValid(), "the pick graph instance loads");
		ctx.True(Resource.Load(LF_WorkAnimPlayer.DRILL_GRAPH).IsValid(), "the drill graph loads");
		ctx.True(Resource.Load(LF_WorkAnimPlayer.DRILL_GRAPH_INSTANCE).IsValid(), "the drill graph instance loads");
		ctx.True(Resource.Load("{0B35C06638AD1D59}anims/anm/player/actions/supplies/repair/p_erc_supplies_repair_low_in.anm").IsValid(), "the pick in clip loads");
		ctx.True(Resource.Load("{6B012F0582412B4B}anims/anm/player/actions/supplies/repair/p_erc_supplies_repair_low_loop.anm").IsValid(), "the pick loop clip loads");
		ctx.True(Resource.Load("{E4E7E0D435FFE4D5}anims/anm/player/actions/supplies/repair/p_erc_supplies_repair_low_out.anm").IsValid(), "the pick out clip loads");
		ctx.True(Resource.Load("{961352FBBDBB1AD7}anims/anm/player/actions/supplies/build/p_erc_supplies_demolitionUS_low_in.anm").IsValid(), "the drill in clip loads");
		ctx.True(Resource.Load("{4B74C849B2079E41}anims/anm/player/actions/supplies/build/p_erc_supplies_demolitionUS_low_loop.anm").IsValid(), "the drill loop clip loads");
		ctx.True(Resource.Load("{CB17AB4A75069805}anims/anm/player/actions/supplies/build/p_erc_supplies_demolitionUS_low_out.anm").IsValid(), "the drill out clip loads");

		SCR_LoiterCustomAnimData data = LF_WorkAnimPlayer.CreateAnimData(LF_EWorkAnimKind.DRILL);
		ctx.EqualStr(SCR_CustomAnimData_Properties.BINDING_NAME_NPC, data.m_sGraphBindingName, "the drill graph binds to the NPC attachment");
		ctx.EqualStr(LF_WorkAnimPlayer.DRILL_GRAPH, data.m_sGraphName, "the drill loiter names the drill graph");
		ctx.EqualStr(LF_WorkAnimPlayer.PICK_GRAPH, LF_WorkAnimPlayer.CreateAnimData(LF_EWorkAnimKind.PICK).m_sGraphName, "the pick loiter names the pick graph");
		ctx.EqualStr("Gestures", data.m_sControlVariableName, "the Gestures variable drives the graph");
		ctx.Equal(562, data.m_iControlVariableValue, "Gestures 562 enters the drill state");
		ctx.Equal(561, LF_WorkAnimPlayer.CreateAnimData(LF_EWorkAnimKind.PICK).m_iControlVariableValue, "Gestures 561 enters the pick state");
		ctx.True(data.m_sCommandName.IsEmpty(), "no command: a command would bypass the control variable");

		ctx.Equal(LF_EWorkAnimStep.KEEP, LF_WorkAnim.Step(true, 1, 1, 10), "healthy work keeps animating");
		ctx.Equal(LF_EWorkAnimStep.STOP_DOWN, LF_WorkAnim.Step(false, 1, 1, 10), "a downed worker stops it");
		ctx.Equal(LF_EWorkAnimStep.STOP_RANGE, LF_WorkAnim.Step(true, LF_WorkAnim.MAX_RANGE + 1, 1, 10), "walking out of range stops it");
		ctx.Equal(LF_EWorkAnimStep.STOP_TIMEOUT, LF_WorkAnim.Step(true, 1, 10 + LF_WorkAnim.SLACK_SECONDS + 1, 10), "an overrun stops it");

		ctx.True(LF_WorkAnim.IsDone(true, 6, 6), "self-ending work is done when its time is up");
		ctx.False(LF_WorkAnim.IsDone(true, 5, 6), "self-ending work runs until then");
		ctx.False(LF_WorkAnim.IsDone(false, 60, 6), "work its owner ends is never ended by the clock alone");

		LF_WorkAnim.ResetStatics();
		LF_WorkAnim.SetSilent(true);
		IEntity worker = LF_TestCharacterFixture.Spawn(ctx);
		if (worker)
			RunSessions(ctx, worker);

		LF_TestCharacterFixture.Delete(worker);
		LF_WorkAnim.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunSessions(LF_TestContext ctx, IEntity worker)
	{
		vector door = worker.GetOrigin() + "1 0 0";
		ctx.False(LF_WorkAnim.Begin(worker, door, LF_EWorkAnimKind.NONE, 5), "no kind, no animation");
		ctx.False(LF_WorkAnim.Begin(worker, door, LF_EWorkAnimKind.PICK, 0), "no duration, no animation");
		ctx.Equal(0, LF_WorkAnim.GetStartCount(), "nothing started");

		ctx.True(LF_WorkAnim.Begin(worker, door, LF_EWorkAnimKind.PICK, 5), "the pick starts the animation");
		ctx.True(LF_WorkAnim.Begin(worker, door, LF_EWorkAnimKind.PICK, 5), "asking again keeps the one animation");
		ctx.Equal(1, LF_WorkAnim.GetStartCount(), "one start");
		LF_WorkAnim.Watch();
		ctx.Equal(1, LF_WorkAnim.GetActiveCount(), "a healthy pass keeps it running");

		LF_WorkAnim.End(worker, "test cancel");
		ctx.Equal(0, LF_WorkAnim.GetActiveCount(), "a cancel stops it");
		LF_WorkAnim.End(worker, "test cancel again");
		ctx.Equal(1, LF_WorkAnim.GetStopCount(), "a second end is not a second stop");

		ctx.True(LF_WorkAnim.Begin(worker, worker.GetOrigin() + "30 0 0", LF_EWorkAnimKind.DRILL, 5), "the drill starts it");
		LF_WorkAnim.Watch();
		ctx.Equal(0, LF_WorkAnim.GetActiveCount(), "a worker out of range of the work is stopped");
		ctx.Equal(2, LF_WorkAnim.GetStopCount(), "two stops");
	}
}
