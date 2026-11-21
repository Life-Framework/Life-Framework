// red-proof: 2026-11-19 `tools\cli test --no-build --red LF_Test_BreakInRules`: RED PROOF OK (RedProof() skips every LF_BreakInRules.Tick act, so no point advances). Green without the hook.
//! crime/break-in-rules: the staged break-in as pure logic. One of two points worked does not
//! advance; both advance at the configured rate; a gap past the grace falls the stage back and sets
//! the reset flag; a gap under it does not; the progress word matches each band. The shipped conf
//! loads with sane numbers.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_BreakInRules : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "crime/break-in-rules";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_BreakInConfig Numbers()
	{
		LF_BreakInConfig config = new LF_BreakInConfig();
		config.m_iStageCount = 3;
		config.m_fSecondsPerStage = 20;
		config.m_fGraceSeconds = 3;
		config.m_fHalfwayFrom = 0.34;
		config.m_fNearlyFrom = 0.75;
		return config;
	}

	//------------------------------------------------------------------------------------------------
	protected void Work(LF_BreakInState state, int worked, float seconds, LF_BreakInConfig config)
	{
		if (RedProof())
			return;

		LF_BreakInRules.Tick(state, worked, 2, seconds, config);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_BreakInConfig config = Numbers();

		// One of two points worked: nothing advances, however long it runs.
		LF_BreakInState one = new LF_BreakInState();
		Work(one, 1, 10, config);
		ctx.EqualFloat(0, one.m_fProgress, 0.0001, "one of two points worked does not advance");
		ctx.Equal(0, one.m_iStage, "one of two points: still stage 0");

		// Both: the configured rate, 20 s a stage, so 10 s is half a stage.
		LF_BreakInState both = new LF_BreakInState();
		Work(both, 2, 10, config);
		ctx.EqualFloat(0.5, both.m_fProgress, 0.0001, "both points advance at the configured rate");
		Work(both, 2, 10, config);
		ctx.Equal(1, both.m_iStage, "20 s of both points finishes stage 1");
		ctx.EqualFloat(0, both.m_fProgress, 0.0001, "the next stage starts at zero");

		// A gap under the grace neither resets nor loses progress.
		Work(both, 2, 10, config);
		Work(both, 1, 2.5, config);
		ctx.EqualFloat(0.5, both.m_fProgress, 0.0001, "a gap under the grace keeps the progress");
		ctx.False(LF_BreakInRules.ConsumeReset(both), "a gap under the grace sets no reset");
		Work(both, 2, 1, config);
		ctx.EqualFloat(0, both.m_fIdleSeconds, 0.0001, "work again restarts the idle clock");

		// A gap past the grace resets the stage once and flags the defenders.
		Work(both, 0, 3.5, config);
		ctx.EqualFloat(0, both.m_fProgress, 0.0001, "a gap past the grace resets the stage");
		ctx.Equal(1, both.m_iStage, "the finished stage stays finished");
		ctx.True(LF_BreakInRules.ConsumeReset(both), "a gap past the grace sets the reset flag");
		ctx.False(LF_BreakInRules.ConsumeReset(both), "the reset flag is read once");
		Work(both, 0, 10, config);
		ctx.False(LF_BreakInRules.ConsumeReset(both), "one gap resets once");

		// Idle with nothing to lose resets nothing: an approach is not a reset.
		LF_BreakInState idle = new LF_BreakInState();
		Work(idle, 0, 30, config);
		ctx.False(LF_BreakInRules.ConsumeReset(idle), "a break-in that never started has nothing to reset");

		// Through: all stages finished.
		LF_BreakInState done = new LF_BreakInState();
		Work(done, 2, 60, config);
		ctx.True(done.m_bThrough, "three stages of both points breaks through");
		ctx.Equal(LF_EBreakInProgress.THROUGH, LF_BreakInRules.ProgressOf(done, config), "through reads through");

		// The words: overall progress bands.
		ctx.Equal(LF_EBreakInProgress.NOT_STARTED, LF_BreakInRules.Band(0, false, config), "zero is not started");
		ctx.Equal(LF_EBreakInProgress.BARELY_STARTED, LF_BreakInRules.Band(0.2, false, config), "0.2 is barely started");
		ctx.Equal(LF_EBreakInProgress.HALFWAY, LF_BreakInRules.Band(0.34, false, config), "0.34 is halfway");
		ctx.Equal(LF_EBreakInProgress.HALFWAY, LF_BreakInRules.Band(0.6, false, config), "0.6 is halfway");
		ctx.Equal(LF_EBreakInProgress.NEARLY_THROUGH, LF_BreakInRules.Band(0.75, false, config), "0.75 is nearly through");
		ctx.Equal(LF_EBreakInProgress.NEARLY_THROUGH, LF_BreakInRules.Band(0.99, false, config), "0.99 is nearly through");
		ctx.True(LF_BreakInRules.WordKey(LF_EBreakInProgress.NOT_STARTED).IsEmpty(), "not started shows no word");
		ctx.EqualStr("#LF-BreakIn_Barely", LF_BreakInRules.WordKey(LF_EBreakInProgress.BARELY_STARTED), "barely started key");
		ctx.EqualStr("#LF-BreakIn_Halfway", LF_BreakInRules.WordKey(LF_EBreakInProgress.HALFWAY), "halfway key");
		ctx.EqualStr("#LF-BreakIn_Nearly", LF_BreakInRules.WordKey(LF_EBreakInProgress.NEARLY_THROUGH), "nearly through key");

		// The shipped conf loads and its numbers hang together.
		LF_BreakInConfig shipped = LF_BreakInConfig.Get();
		ctx.NotNull(shipped, "the shipped break-in conf loads");
		if (shipped)
		{
			ctx.True(shipped.m_iStageCount >= 1, "the conf has a stage");
			ctx.True(shipped.m_fSecondsPerStage > 0, "the conf stage takes time");
			ctx.True(shipped.m_fGraceSeconds >= 0, "the conf grace is not negative");
			ctx.True(shipped.m_fHalfwayFrom < shipped.m_fNearlyFrom, "the conf bands are ordered");
		}
	}
}
