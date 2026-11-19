// red-proof: observed with `tools\cli test --no-build --red LF_Test_RangeRun`: RedProof() skips the first start, so the lane runs no round and "a start raises the lane" fails. Before Feature/Range existed the test did not compile, which is the red on main.
//------------------------------------------------------------------------------------------------
//! range/run (#3184, #3169): the range keeps score. A round counts only the starter's hits, ends
//! at all-down or at the time limit, a best is replaced only by a better time, and a second start
//! while a round runs is refused. The pure rules run first, then the lane with real targets.

[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_RangeRun : LF_Test
{
	protected static const ResourceName RANGE_TARGET = "{4D1A82E96B30C7F5}Prefabs/Props/Range/LF_RangeTarget.et";
	protected static const string LANE = "test-range-lane";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "range/run";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		RunRules(ctx);
		RunBests(ctx);
		RunLane(ctx);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunRules(LF_TestContext ctx)
	{
		// A round counts only hits from the shooter who started it.
		LF_RangeRound round = new LF_RangeRound(LANE, "uid-a", 7, 3, 1000);
		ctx.False(round.RecordHit("uid-b", "t1", 2000), "a hit from another shooter is not counted");
		ctx.Equal(0, round.Hits(), "no hit counted for the other shooter");
		ctx.True(round.RecordHit("uid-a", "t1", 2000), "the starter's hit is counted");
		ctx.False(round.RecordHit("uid-a", "t1", 2100), "the same target counts once");
		ctx.Equal(1, round.Hits(), "one distinct target is down");
		ctx.False(round.IsOver(), "a round with a target still up is running");

		// A round ends at all-down.
		ctx.True(round.RecordHit("uid-a", "t2", 3000), "the second target counts");
		ctx.True(round.RecordHit("uid-a", "t3", 15200), "the last target counts");
		ctx.True(round.IsOver(), "a round ends when every target is down");
		ctx.True(round.Cleared(), "all down is a clear");
		ctx.Equal(14200, round.ElapsedMs(), "the clear time runs from the start to the last hit");
		ctx.False(round.RecordHit("uid-a", "t1", 16000), "a finished round takes no more hits");

		// A round ends at the time limit.
		LF_RangeRound slow = new LF_RangeRound(LANE, "uid-a", 7, 3, 1000);
		slow.RecordHit("uid-a", "t1", 5000);
		ctx.False(slow.Tick(1000 + LF_RangeRound.LIMIT_MS - 1), "a round is still running just before the limit");
		ctx.True(slow.Tick(1000 + LF_RangeRound.LIMIT_MS), "a round ends at the time limit");
		ctx.True(slow.IsOver(), "a timed-out round is over");
		ctx.False(slow.Cleared(), "a timed-out round is not a clear");
		ctx.Equal(1, slow.Hits(), "a timed-out round keeps its hits");
		ctx.Equal(LF_RangeRound.LIMIT_MS, slow.ElapsedMs(), "a timed-out round reads the full limit");
		ctx.False(slow.RecordHit("uid-a", "t2", 1000 + LF_RangeRound.LIMIT_MS + 50), "a hit after the limit is refused");
		ctx.Equal(1, slow.Hits(), "a late hit does not count");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunBests(LF_TestContext ctx)
	{
		LF_RangeBests.ResetStatics();
		ctx.Equal(0, LF_RangeBests.Get("uid-a"), "no best before a first clear");
		ctx.True(LF_RangeBests.Offer("uid-a", 14200), "a first clear sets the best");
		ctx.Equal(14200, LF_RangeBests.Get("uid-a"), "the best is stored");
		ctx.False(LF_RangeBests.Offer("uid-a", 15000), "a slower clear does not replace the best");
		ctx.False(LF_RangeBests.Offer("uid-a", 14200), "an equal clear does not replace the best");
		ctx.Equal(14200, LF_RangeBests.Get("uid-a"), "the best is unchanged by a slower clear");
		ctx.True(LF_RangeBests.Offer("uid-a", 12800), "a faster clear replaces the best");
		ctx.Equal(12800, LF_RangeBests.Get("uid-a"), "the best is the faster time");
		ctx.Equal(0, LF_RangeBests.Get("uid-b"), "a best belongs to one player record key");
		LF_RangeBests.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunLane(LF_TestContext ctx)
	{
		array<IEntity> spawned = {};
		vector spot = "20 0 20";
		for (int i = 0; i < 3; i++)
		{
			IEntity target = LF_Utils.SpawnEntityPrefab(RANGE_TARGET, spot);
			if (!target)
			{
				ctx.Fail("range target spawns");
				Cleanup(spawned);
				return;
			}
			spawned.Insert(target);
			spot[0] = spot[0] + 2;
		}

		SCR_FiringRangeTarget first = SCR_FiringRangeTarget.Cast(spawned[0]);
		SCR_FiringRangeTarget second = SCR_FiringRangeTarget.Cast(spawned[1]);
		SCR_FiringRangeTarget third = SCR_FiringRangeTarget.Cast(spawned[2]);
		ctx.True(first != null && second != null && third != null, "the spawned targets are range targets");
		LF_RangeLaneComponent lane = LF_RangeLaneComponent.Cast(spawned[0].FindComponent(LF_RangeLaneComponent));
		ctx.True(lane != null, "the range target carries the lane component");
		if (ctx.FailureCount() > 0)
		{
			Cleanup(spawned);
			return;
		}

		// Lane membership is a component with an id string; the test lane is set on each target.
		foreach (IEntity each : spawned)
		{
			LF_RangeLaneComponent member = LF_RangeLaneComponent.Cast(each.FindComponent(LF_RangeLaneComponent));
			if (member)
				member.SetLaneId(LANE);
		}

		LF_RangeRun.ResetStatics();
		LF_RangeBests.ResetStatics();

		first.SetState(ETargetState.TARGET_DOWN);
		if (!RedProof())
			ctx.Equal(LF_ERangeStart.OK, LF_RangeRun.StartAs(LANE, "uid-a", 0, 10000), "a start is accepted on an idle lane");
		ctx.True(LF_RangeRun.IsRunning(LANE), "a round is running on the lane");
		ctx.Equal(ETargetState.TARGET_UP, first.GetState(), "a start raises the lane's targets");

		// A second start while a round runs is refused.
		ctx.Equal(LF_ERangeStart.RUNNING, LF_RangeRun.StartAs(LANE, "uid-b", 0, 10500), "a second start while a round runs is refused");
		ctx.True(LF_RangeRun.IsRunning(LANE), "the first round is still the one running");

		// Only the starter's hits count.
		LF_RangeRun.OnHit(first, "uid-b", 11000);
		ctx.Equal(0, LF_RangeRun.HitsOf(LANE), "another shooter's hit on a target does not count");
		LF_RangeRun.OnHit(first, "uid-a", 12000);
		LF_RangeRun.OnHit(second, "uid-a", 13000);
		ctx.Equal(2, LF_RangeRun.HitsOf(LANE), "the starter's two hits count");
		ctx.True(LF_RangeRun.IsRunning(LANE), "two of three down: the round is still running");

		// Ends at all-down and stores the best.
		LF_RangeRun.OnHit(third, "uid-a", 24200);
		ctx.False(LF_RangeRun.IsRunning(LANE), "all down ends the round");
		ctx.Equal(14200, LF_RangeBests.Get("uid-a"), "a clear stores the time as the best");

		// A new round that times out leaves the best alone, and a better clear replaces it.
		ctx.Equal(LF_ERangeStart.OK, LF_RangeRun.StartAs(LANE, "uid-a", 0, 30000), "a new round starts after the last one ended");
		LF_RangeRun.OnHit(first, "uid-a", 31000);
		LF_RangeRun.TickAt(30000 + LF_RangeRound.LIMIT_MS);
		ctx.False(LF_RangeRun.IsRunning(LANE), "the time limit ends the round on the lane");
		ctx.Equal(14200, LF_RangeBests.Get("uid-a"), "a timed-out round does not touch the best");

		ctx.Equal(LF_ERangeStart.OK, LF_RangeRun.StartAs(LANE, "uid-a", 0, 100000), "a third round starts");
		LF_RangeRun.OnHit(first, "uid-a", 101000);
		LF_RangeRun.OnHit(second, "uid-a", 102000);
		LF_RangeRun.OnHit(third, "uid-a", 112800);
		ctx.Equal(12800, LF_RangeBests.Get("uid-a"), "a faster clear replaces the best");

		LF_RangeRun.ResetStatics();
		LF_RangeBests.ResetStatics();
		Cleanup(spawned);
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(array<IEntity> spawned)
	{
		foreach (IEntity entity : spawned)
		{
			if (entity)
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
		}
	}
}
