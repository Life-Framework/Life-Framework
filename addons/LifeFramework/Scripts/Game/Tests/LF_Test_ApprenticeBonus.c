// red-proof: observed 2026-11-23 with `tools\cli test --no-build --red LF_Test_ApprenticeBonus`: RED PROOF OK, with the subscriber off the learner got 75 not 85 and the master 215 not 225 (no bonus paid)
//! level/apprentice-bonus (0.3.0 A10, #4407): a character of level 10 or more in a trade who earns XP within 15 m of
//! a character under level 10 in the same trade adds a small bonus (10% of the award, at least 1) to both of them;
//! the award can be the master's or the learner's. Nothing is paid at 30 m, with both under 10 or both 10 or more,
//! for the bonus's own award (reason "apprentice": no loop), or for the same pair again inside 60 s.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_ApprenticeBonus : LF_Test
{
	static const string MASTER = "test-apprentice-master";
	static const string LEARNER = "test-apprentice-learner";
	static const string JOB = "MINER";
	static const string OTHER_JOB = "FARMER";

	override string GetName()
	{
		return "level/apprentice-bonus";
	}

	//------------------------------------------------------------------------------------------------
	static LF_JobsConfig MakeConfig()
	{
		LF_JobsConfig config = new LF_JobsConfig();
		// Level 12 at 110 XP, level 10 at 90, level 3 at 20.
		array<int> ladder = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110};
		config.m_aTracks = {};
		config.m_aTracks.Insert(LF_JobTrackDef.Make(JOB, ladder));
		config.m_aTracks.Insert(LF_JobTrackDef.Make(OTHER_JOB, ladder));
		config.m_aXpAwards = {};
		config.m_aLicenceGates = {};
		config.m_iJobXpFirstLevel = 500;
		config.m_fJobXpLevelGrowth = 2;
		return config;
	}

	//------------------------------------------------------------------------------------------------
	protected void ForgetAll()
	{
		LF_JobLevels.Forget(MASTER);
		LF_JobLevels.Forget(LEARNER);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_JobsConfig.UseForTest(MakeConfig());
		LF_Apprentice.ResetStatics();
		ForgetAll();

		// Pure numbers first.
		ctx.Equal(10, LF_Apprentice.BonusFor(100), "the bonus is 10% of the award");
		ctx.Equal(1, LF_Apprentice.BonusFor(3), "a small award still pays at least 1");
		ctx.Equal(0, LF_Apprentice.BonusFor(0), "no award, no bonus");
		ctx.True(LF_Apprentice.IsPair(12, 3), "a master and a learner are a pair");
		ctx.True(LF_Apprentice.IsPair(3, 12), "a learner and a master are a pair");
		ctx.False(LF_Apprentice.IsPair(3, 4), "two learners are not a pair");
		ctx.False(LF_Apprentice.IsPair(10, 30), "two masters are not a pair");
		ctx.True(LF_Apprentice.IsPair(10, 9), "level 10 is a master and level 9 a learner");

		// Levels set up with the subscriber off, so the setup earns no bonus.
		ctx.Equal(110, LF_JobLevels.AddXp(MASTER, JOB, 110, "setup"), "the master gets XP");
		ctx.Equal(20, LF_JobLevels.AddXp(LEARNER, JOB, 20, "setup"), "the learner gets XP");
		ctx.Equal(12, LF_JobLevels.LevelOf(MASTER, JOB), "the master is level 12");
		ctx.Equal(3, LF_JobLevels.LevelOf(LEARNER, JOB), "the learner is level 3");

		// Red proof: with the subscriber off no award pays a bonus and the checks below fail.
		if (!RedProof())
			LF_Apprentice.EnsureSubscribed();

		IEntity masterBody = LF_TestCharacterFixture.Spawn(ctx);
		IEntity learnerBody = LF_TestCharacterFixture.Spawn(ctx);
		if (masterBody && learnerBody)
		{
			LF_Apprentice.SetBodyForTest(MASTER, masterBody);
			LF_Apprentice.SetBodyForTest(LEARNER, learnerBody);
			RunPairs(ctx, masterBody, learnerBody);
		}

		LF_Apprentice.SetBodyForTest(MASTER, null);
		LF_Apprentice.SetBodyForTest(LEARNER, null);
		LF_TestCharacterFixture.Delete(masterBody);
		LF_TestCharacterFixture.Delete(learnerBody);
		LF_Apprentice.ResetStatics();
		ForgetAll();
		LF_JobsConfig.Reload();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunPairs(LF_TestContext ctx, IEntity masterBody, IEntity learnerBody)
	{
		learnerBody.SetOrigin(masterBody.GetOrigin() + "5 0 0");

		// A master's award at 5 m: both gain the bonus, once.
		ctx.Equal(100, LF_JobLevels.AddXp(MASTER, JOB, 100, "test"), "the award itself is paid in full");
		ctx.Equal(220, LF_JobLevels.XpOf(MASTER, JOB), "the master gets the bonus once (110 + 100 + 10)");
		ctx.Equal(30, LF_JobLevels.XpOf(LEARNER, JOB), "the learner gets the bonus (20 + 10)");

		// The same pair inside 60 s pays nothing more.
		ctx.Equal(50, LF_JobLevels.AddXp(LEARNER, JOB, 50, "test"), "the learner's award is paid in full");
		ctx.Equal(80, LF_JobLevels.XpOf(LEARNER, JOB), "inside 60 s the learner gets no second bonus (30 + 50)");
		ctx.Equal(220, LF_JobLevels.XpOf(MASTER, JOB), "inside 60 s the master gets no second bonus");

		// 61 s later, the learner's award pays both.
		float later = LF_JobGuards.Now() + 61;
		ctx.Equal(5, LF_Apprentice.OnAwarded(LEARNER, JOB, 50, "test", later), "a learner's award pays a bonus of 5 each");
		ctx.Equal(225, LF_JobLevels.XpOf(MASTER, JOB), "the master gets the learner-award bonus (220 + 5)");
		ctx.Equal(85, LF_JobLevels.XpOf(LEARNER, JOB), "the learner gets it too (80 + 5)");

		// The bonus's own award never pays a bonus.
		ctx.Equal(0, LF_Apprentice.OnAwarded(MASTER, JOB, 100, LF_Apprentice.REASON, later + 200), "an award with reason apprentice pays nothing");

		// 30 m apart: nothing.
		learnerBody.SetOrigin(masterBody.GetOrigin() + "30 0 0");
		ctx.Equal(0, LF_Apprentice.OnAwarded(MASTER, JOB, 100, "test", later + 400), "at 30 m nothing is paid");
		ctx.Equal(225, LF_JobLevels.XpOf(MASTER, JOB), "the master's XP did not move at 30 m");
		ctx.Equal(85, LF_JobLevels.XpOf(LEARNER, JOB), "the learner's XP did not move at 30 m");

		// Both under 10 in another trade: nothing, even at 5 m.
		learnerBody.SetOrigin(masterBody.GetOrigin() + "5 0 0");
		ctx.Equal(0, LF_Apprentice.OnAwarded(MASTER, OTHER_JOB, 100, "test", later + 600), "two learners in a trade earn no bonus");
		ctx.Equal(0, LF_JobLevels.XpOf(MASTER, OTHER_JOB), "no XP appeared in the other trade");

		// And back inside 15 m, a pair that cooled down pays again.
		ctx.Equal(10, LF_Apprentice.OnAwarded(MASTER, JOB, 100, "test", later + 800), "back at 5 m the bonus pays again");
	}
}
