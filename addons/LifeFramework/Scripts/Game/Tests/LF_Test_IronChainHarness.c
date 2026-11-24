// red-proof: #4856 2026-11-24, the harness stand-ins moved to the copper refinery (the iron smelter has no gate to set): same RedProof() hooks. Earlier: #3244 2026-11-24, `tools\cli test --no-build --red LF_Test_IronChainHarness`: RED PROOF OK (RedProof() skips the level set and the coal grant: the smelter still refuses the worker, coalgive 3 adds 0). Green with `--only`.

//! harness/iron-chain (#3244): the session proof's stand-ins. The iron smelter is open at SMELTING 1 (#4856); the copper refinery keeps SMELTING 15, over the 2000 daily cap, so no
//! one earns a gate that high in a day: the proof sets it with `levelset` (LF_JobLevels.SetLevelUnchecked, #4773) and
//! the refinery then reads that level and lets the worker in. `coalgive` puts coal lumps in the pack, clamped to 1..20.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_IronChainHarness : LF_Test
{
	protected static const string WORKER = "test-iron-chain-harness";
	protected static const string JOB = "SMELTING";
	protected static const string MACHINE = "refinery-copper";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "harness/iron-chain";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ConfigService.Reset();
		LF_JobsConfig.Reload();
		LF_ResourceMachineDef machine = LF_ResourceChainCatalog.FindMachine(MACHINE);
		ctx.NotNull(machine, MACHINE + " has a machine row");
		LF_JobTrackDef track = LF_JobLevels.Track(JOB);
		ctx.NotNull(track, JOB + " has a track");
		if (!machine || !track)
			return;

		LF_JobLevels.Forget(WORKER);
		ctx.Equal(1, LF_JobLevels.LevelOf(WORKER, JOB), "a new worker is SMELTING level 1");
		ctx.True(track.XpForLevel(machine.m_iMinLevel) > track.m_iDailyXpCap, "the gate costs more XP than one day can earn, so a proof must set it");

		// The smelter turns a level-1 worker away in words.
		string refusal = LF_ProcessAction.LevelRefusal(machine, LF_JobLevels.LevelOf(WORKER, JOB));
		ctx.True(!refusal.IsEmpty(), "level 1 is refused at the refinery");

		// Red proof: the level is never set, as before #3244.
		int got = 0;
		if (!RedProof())
			got = LF_JobLevels.SetLevelUnchecked(WORKER, JOB, machine.m_iMinLevel);

		ctx.Equal(track.XpForLevel(machine.m_iMinLevel), got, "the set returns the XP now held");
		ctx.Equal(machine.m_iMinLevel, LF_JobLevels.LevelOf(WORKER, JOB), "the worker is at the refinery's level");
		ctx.Equal(track.XpForLevel(machine.m_iMinLevel), LF_JobLevels.XpOf(WORKER, JOB), "the XP is exactly what the level costs");
		ctx.EqualStr("", LF_ProcessAction.LevelRefusal(machine, LF_JobLevels.LevelOf(WORKER, JOB)), "the refinery lets the worker in");

		// The set books no cap: the first real award still pays.
		int xpBefore = LF_JobLevels.XpOf(WORKER, JOB);
		int paid = LF_JobLevels.AddXp(WORKER, JOB, 10, "test");
		ctx.True(paid > 0, "an award after the set still pays (the set booked no daily cap)");
		ctx.Equal(xpBefore + paid, LF_JobLevels.XpOf(WORKER, JOB), "the award adds to the set XP");

		// Back down, and a level the table stops below.
		ctx.Equal(0, LF_JobLevels.SetLevelUnchecked(WORKER, JOB, 1), "setting level 1 drops the XP to 0");
		ctx.Equal(0, LF_JobLevels.XpOf(WORKER, JOB), "level 1 is 0 XP");
		int top = track.m_aLevelThresholds.Count() + 1;
		LF_JobLevels.SetLevelUnchecked(WORKER, JOB, 5);
		ctx.Equal(-1, LF_JobLevels.SetLevelUnchecked(WORKER, JOB, top + 1), "a level past the table is refused");
		ctx.Equal(5, LF_JobLevels.LevelOf(WORKER, JOB), "a refused set changes nothing");
		ctx.Equal(-1, LF_JobLevels.SetLevelUnchecked(WORKER, "NO_SUCH_JOB", 3), "an unknown job is refused");
		ctx.Equal(-1, LF_JobLevels.SetLevelUnchecked("", JOB, 3), "an empty account id is refused");
		LF_JobLevels.Forget(WORKER);

		CheckCoal(ctx);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckCoal(LF_TestContext ctx)
	{
		IEntity worker = LF_TestCharacterFixture.Spawn(ctx);
		ctx.NotNull(worker, "the worker spawns");
		if (!worker)
			return;

		int added = 0;
		if (!RedProof())
			added = LF_MiningHarness.GiveCoal(worker, 3);

		ctx.Equal(3, added, "coalgive 3 adds three lumps");
		ctx.Equal(3, LF_InventoryUtils.GetAmount(worker, LF_MiningHarness.COAL_LUMP), "the pack holds three coal lumps");
		ctx.Equal(1, LF_MiningHarness.GiveCoal(worker, 0), "a count under 1 gives one lump");
		int most = LF_MiningHarness.GiveCoal(worker, 500);
		ctx.True(most > 0 && most <= LF_MiningHarness.MAX_COAL, "a huge count is clamped to " + LF_MiningHarness.MAX_COAL);
		ctx.Equal(0, LF_MiningHarness.GiveCoal(null, 1), "no character gets nothing");
		LF_TestCharacterFixture.Delete(worker);
	}
}
