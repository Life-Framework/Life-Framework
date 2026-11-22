// red-proof: 2026-11-22, `--red LF_Test_TreeFallPlan,LF_Test_TreeFallLogs`: RED PROOF OK (all 2 listed test(s) fail with the hook on); the hook skips the log plan / the drop, so the yield-count, ground and sweep checks fail.
//! lumber/tree-fall-logs: the logs a fell drops are the real pickup items. DropLogs spawns one shoulder-carry
//! log item per log along the fallen line, each resting on the ground; hardwood trees drop the hardwood
//! log; the sweep keeps fresh logs, forgets one a player carried away (never deleting it) and deletes the
//! ones left lying past their time, so nothing piles up.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_TreeFallLogs : LF_Test
{
	override string GetName()
	{
		return "lumber/tree-fall-logs";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_TreeFall.ResetStatics();
		ctx.EqualStr(LF_FellAction.HARDWOOD_LOG_PREFAB, LF_FellAction.LogPrefabOf(LF_TreeWoodClass.HARDWOOD), "a hardwood tree drops the hardwood log");
		ctx.EqualStr(LF_FellAction.LOG_PREFAB, LF_FellAction.LogPrefabOf(LF_TreeWoodClass.SOFTWOOD), "a softwood tree drops the softwood log");

		BaseWorld world = GetGame().GetWorld();
		vector foot = LF_TestDebrisProbe.Gate() + "40 0 40";
		foot[1] = world.GetSurfaceY(foot[0], foot[2]);
		array<IEntity> logs = {};
		if (!RedProof())
			LF_TreeFall.DropLogs(LF_FellAction.LOG_PREFAB, foot, 90, 12, 3, logs);

		ctx.Equal(3, logs.Count(), "three logs from the yield are three log items");
		ctx.Equal(logs.Count(), LF_TreeFall.Book().Count(), "every dropped log is in the loose-log book");
		foreach (IEntity log : logs)
		{
			vector at = log.GetOrigin();
			ctx.NotNull(LF_Component<LF_ShoulderCarryComponent>.Find(log), "a dropped log is the shoulder-carry pickup item");
			ctx.True(at[0] > foot[0], "a log lies along the fallen line");
			ctx.InRange(at[1] - LF_TreeFall.GroundY(at), -0.6, 0.8, "a log rests on the ground, neither floating nor sunk");
		}

		int now = LF_FelledTrees.NowS();
		ctx.Equal(0, LF_TreeFall.SweepAt(now + 10), "a fresh log is not swept");
		if (logs.Count() == 3)
		{
			// A player carried the first log away: it leaves the book and survives the sweep.
			logs[0].SetOrigin(foot + "-30 0 0");
			logs[0].Update();
			int deleted = LF_TreeFall.SweepAt(now + LF_TreeFall.LOG_LIFE_S + 1);
			ctx.Equal(2, deleted, "the two logs left lying past their time are deleted");
			ctx.True(world.FindEntityByID(logs[0].GetID()) != null, "the log a player moved is never deleted");
			ctx.Equal(0, LF_TreeFall.Book().Count(), "the book is empty after the sweep");
			LF_TestCharacterFixture.Delete(logs[0]);
		}
		else
		{
			foreach (IEntity log : logs)
			{
				LF_TestCharacterFixture.Delete(log);
			}
		}

		LF_TreeFall.ResetStatics();
	}
}
