// red-proof: the pure helpers are exercised through EL_BaseCrop's own math.
// Make IsStageDue return nowMs > start + duration (strict) and this test fails
// at the exact-boundary assertions; drop the CanGatherAtStage out-of-range guard
// and the past-final-stage assertion fails. Red run pending: blocked by the
// parallel-workstream rule (tools\cli test is serial-only this session), the
// mutation above fails the next `tools\cli test --tier fast`.
// tier: LOGIC
class EL_Test_CropStages : EL_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "crop/stage-progression";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(EL_TestContext ctx)
	{
		// Stage duration: authored in minutes, compared against world ms
		EL_CropStage quarter = new EL_CropStage();
		quarter.m_fStageTime = 0.2;
		ctx.EqualFloat(12000, quarter.GetDurationMs(), 0.001, "0.2 min stage is 12000 ms");

		EL_CropStage minute = new EL_CropStage();
		minute.m_fStageTime = 1;
		ctx.EqualFloat(60000, minute.GetDurationMs(), 0.001, "1 min stage is 60000 ms");

		EL_CropStage zero = new EL_CropStage();
		zero.m_fStageTime = 0;
		ctx.EqualFloat(0, zero.GetDurationMs(), 0.001, "0 min stage is 0 ms");

		// Advance threshold: due when now crosses start + duration
		ctx.False(EL_BaseCrop.IsStageDue(1000, 12000, 1000), "not due at stage start");
		ctx.False(EL_BaseCrop.IsStageDue(1000, 12000, 12999), "not due before the boundary");
		ctx.True(EL_BaseCrop.IsStageDue(1000, 12000, 13000), "due exactly at the boundary");
		ctx.True(EL_BaseCrop.IsStageDue(1000, 12000, 13001), "due after the boundary");

		// Simulated CabbageCrop timeline: 0.2 + 0.2 + 1 minutes
		ref array<ref EL_CropStage> stages = new array<ref EL_CropStage>();
		EL_CropStage seed = new EL_CropStage();
		seed.m_fStageTime = 0.2;
		seed.m_bCanGather = 0;
		stages.Insert(seed);

		EL_CropStage growing = new EL_CropStage();
		growing.m_fStageTime = 0.2;
		growing.m_bCanGather = 0;
		stages.Insert(growing);

		EL_CropStage ripe = new EL_CropStage();
		ripe.m_fStageTime = 1;
		ripe.m_bCanGather = 1;
		stages.Insert(ripe);

		float start = 0;
		float now = 0;
		int stage = 0;

		// Seed stage
		ctx.True(EL_BaseCrop.IsStageDue(start, stages[stage].GetDurationMs(), now) == false, "seed not due at t=0");
		now = 12000;
		ctx.True(EL_BaseCrop.IsStageDue(start, stages[stage].GetDurationMs(), now), "seed due at 12s");
		start = now;
		stage++;

		// Growing stage
		now = 24000;
		ctx.True(EL_BaseCrop.IsStageDue(start, stages[stage].GetDurationMs(), now), "growing due at 24s");
		start = now;
		stage++;

		// Ripe stage lasts a minute; at 60s it is still not final
		now = 84000;
		ctx.True(EL_BaseCrop.IsStageDue(start, stages[stage].GetDurationMs(), now), "ripe due at 84s");
		stage++;

		// Stage index now equals count: final reached, crop idles or deletes by flag
		ctx.True(EL_BaseCrop.IsFinalStageReached(stage, stages.Count()), "stage index past the last stage is final");
		ctx.True(EL_BaseCrop.IsFinalStageReached(stages.Count(), stages.Count()), "index == count is final");
		ctx.False(EL_BaseCrop.IsFinalStageReached(stages.Count() - 1, stages.Count()), "last real stage is not final");
		ctx.False(EL_BaseCrop.IsFinalStageReached(0, stages.Count()), "first stage is not final");

		// Can-gather: only the Ripe stage gathers; past final the crop cannot gather
		ctx.False(EL_BaseCrop.CanGatherAtStage(stages, 0), "seed stage cannot gather");
		ctx.False(EL_BaseCrop.CanGatherAtStage(stages, 1), "growing stage cannot gather");
		ctx.True(EL_BaseCrop.CanGatherAtStage(stages, 2), "ripe stage can gather");
		ctx.False(EL_BaseCrop.CanGatherAtStage(stages, 3), "past final stage cannot gather");
		ctx.False(EL_BaseCrop.CanGatherAtStage(stages, 99), "far out-of-range cannot gather");
		ctx.False(EL_BaseCrop.CanGatherAtStage(stages, -1), "pre-init stage index cannot gather");
		ctx.False(EL_BaseCrop.CanGatherAtStage(null, 0), "no stages cannot gather");
	}
}