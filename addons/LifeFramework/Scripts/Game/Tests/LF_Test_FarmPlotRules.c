// red-proof: observed 2026-11-10 with `tools\cli test --no-build --only LF_Test_FarmPlotRules`: making the stage boundary
// inclusive (grownSeconds <= stageEnd) failed `- half grown at one minute (expected 1, got 0)` and
// `- ripe at two minutes (expected 2, got 1)`. Restored; green.

// tier: LOGIC
//! The growth rules of a farm plot (#614): each stage but the last lasts its minutes, the last one
//! holds so a ripe crop waits, only a gatherable stage is ripe, and each refusal has words.
class LF_Test_FarmPlotRules : LF_Test
{
	override string GetName()
	{
		return "farm/plot-rules";
	}

	protected static LF_CropStage Stage(float minutes, bool canGather)
	{
		LF_CropStage stage = new LF_CropStage();
		stage.m_fStageTime = minutes;
		stage.m_bCanGather = canGather;
		return stage;
	}

	override void Run(LF_TestContext ctx)
	{
		array<ref LF_CropStage> stages = new array<ref LF_CropStage>();
		stages.Insert(Stage(1, false));
		stages.Insert(Stage(1, false));
		stages.Insert(Stage(1, true));

		ctx.Equal(0, LF_FarmPlotRules.StageForSeconds(stages, 0), "a fresh planting is sprouts");
		ctx.Equal(0, LF_FarmPlotRules.StageForSeconds(stages, 59.9), "still sprouts just before a minute");
		ctx.Equal(1, LF_FarmPlotRules.StageForSeconds(stages, 60), "half grown at one minute");
		ctx.Equal(1, LF_FarmPlotRules.StageForSeconds(stages, 119), "still half grown before two minutes");
		ctx.Equal(2, LF_FarmPlotRules.StageForSeconds(stages, 120), "ripe at two minutes");
		ctx.Equal(2, LF_FarmPlotRules.StageForSeconds(stages, 100000), "a ripe crop waits; it never runs past the last stage");
		ctx.EqualFloat(120, LF_FarmPlotRules.SecondsToLastStage(stages), 0.001, "two minutes from seed to ripe");

		ctx.Equal(LF_FarmPlotRules.EMPTY, LF_FarmPlotRules.StageForSeconds(null, 30), "no stages, nothing grows");
		ctx.Equal(LF_FarmPlotRules.EMPTY, LF_FarmPlotRules.StageForSeconds(new array<ref LF_CropStage>(), 30), "an empty stage list, nothing grows");

		array<ref LF_CropStage> single = new array<ref LF_CropStage>();
		single.Insert(Stage(5, true));
		ctx.Equal(0, LF_FarmPlotRules.StageForSeconds(single, 0), "a one-stage crop is its only stage at once");

		ctx.False(LF_FarmPlotRules.IsRipe(stages, LF_FarmPlotRules.EMPTY), "an empty plot is not ripe");
		ctx.False(LF_FarmPlotRules.IsRipe(stages, 0), "sprouts are not ripe");
		ctx.False(LF_FarmPlotRules.IsRipe(stages, 1), "half grown is not ripe");
		ctx.True(LF_FarmPlotRules.IsRipe(stages, 2), "the last stage is ripe");
		ctx.False(LF_FarmPlotRules.IsRipe(stages, 3), "an index past the list is not ripe");

		ctx.EqualStr("#LF-Farm_NeedSeeds", LF_FarmPlotRules.RefusalKey(LF_EFarmPlot.NO_SEEDS), "no seeds has words");
		ctx.EqualStr("#LF-Farm_NotRipe", LF_FarmPlotRules.RefusalKey(LF_EFarmPlot.NOT_RIPE), "not ripe has words");
		ctx.EqualStr("#LF-Farm_NotEmpty", LF_FarmPlotRules.RefusalKey(LF_EFarmPlot.NOT_EMPTY), "a planted plot has words");
		ctx.EqualStr("#LF-Farm_TooFar", LF_FarmPlotRules.RefusalKey(LF_EFarmPlot.TOO_FAR), "too far has words");
		ctx.EqualStr("", LF_FarmPlotRules.RefusalKey(LF_EFarmPlot.OK), "OK needs no words");
	}
}
