// red-proof: Observed 2026-11-17 `tools/cli test --no-build --only LF_Test_WorkAnimReset` with the Remove branch of LF_WorkAnim.ResetStatics disabled failed `work/anim-reset` with `- the reset takes the queued watch off the queue (expected 1, got 0)`. Restored: green.
// tier: WORLD
//! #2419: a work-animation reset takes its repeating Watch call off the call queue. Each reset used
//! to leak a 400 ms ticker. The queue defers a removal made inside its own pass (a test runs inside
//! one), so the test reads the removal counter, not the queue.
class LF_Test_WorkAnimReset : LF_Test
{
	override string GetName()
	{
		return "work/anim-reset";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_WorkAnim.ResetStatics();
		LF_WorkAnim.SetSilent(false);
		IEntity worker = LF_Utils.SpawnEntityPrefab(LF_Contraband.ITEM, "1000 0 1000");
		ctx.NotNull(worker, "a stand-in worker spawns");
		if (!worker)
			return;

		int removals = LF_WorkAnim.GetWatchRemovalCount();
		ctx.True(LF_WorkAnim.Begin(worker, worker.GetOrigin(), LF_EWorkAnimKind.PICK, 5), "work starts and queues the watch");
		ctx.True(GetGame().GetCallqueue().GetRemainingTime(LF_WorkAnim.Watch) > 0, "the watch is queued while work runs");

		LF_WorkAnim.ResetStatics();
		ctx.Equal(removals + 1, LF_WorkAnim.GetWatchRemovalCount(), "the reset takes the queued watch off the queue");
		ctx.Equal(0, LF_WorkAnim.GetActiveCount(), "the reset ends every session");

		LF_WorkAnim.ResetStatics();
		ctx.Equal(removals + 1, LF_WorkAnim.GetWatchRemovalCount(), "a reset with no watch queued removes nothing");

		SCR_EntityHelper.DeleteEntityAndChildren(worker);
	}
}
