// red-proof: #4861 2026-11-24: `tools\cli test --no-build --red LF_Test_MachineSteps` (RedProof() reads the steps at level 0, so no step applies and the bars and seconds stay at the base row).

//! processing/machine-steps (#4861): a machine row's level steps raise the batch cap and cut the run time for a worker
//! at or above the step level, and never the other way. Checked on a made-up row and on the shipped iron smelter.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_MachineSteps : LF_Test
{
	override string GetName()
	{
		return "processing/machine-steps";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_MachineStep Step(int level, int batches, int seconds)
	{
		LF_MachineStep step = new LF_MachineStep();
		step.m_iLevel = level;
		step.m_iMaxBatches = batches;
		step.m_iProcessSeconds = seconds;
		return step;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ResourceMachineDef row = new LF_ResourceMachineDef();
		row.m_iMaxBatches = 5;
		row.m_iProcessSeconds = 60;
		row.m_aSteps = new array<ref LF_MachineStep>();
		row.m_aSteps.Insert(Step(2, 6, 0));
		row.m_aSteps.Insert(Step(7, 0, 45));
		row.m_aSteps.Insert(Step(9, 4, 50));

		int level = 7;
		if (RedProof())
			level = 0;

		ctx.Equal(5, LF_ProcessAction.MaxBatchesAt(row, 1), "level 1 keeps the row's cap");
		ctx.Equal(60, LF_ProcessAction.SecondsAt(row, 1), "level 1 keeps the row's time");
		ctx.Equal(6, LF_ProcessAction.MaxBatchesAt(row, level), "level 7 has the level 2 cap");
		ctx.Equal(45, LF_ProcessAction.SecondsAt(row, level), "level 7 has the 45 s run");
		ctx.Equal(6, LF_ProcessAction.MaxBatchesAt(row, 9), "a step never lowers the cap");
		ctx.Equal(45, LF_ProcessAction.SecondsAt(row, 9), "a step never lengthens a run");

		row.m_iProcessSeconds = 0;
		ctx.Equal(0, LF_ProcessAction.SecondsAt(row, 30), "an instant machine stays instant");
		ctx.Equal(1, LF_ProcessAction.MaxBatchesAt(null, 30), "no row runs one batch");

		LF_ResourceMachineDef smelter = LF_ResourceChainCatalog.FindMachine("smelter-iron");
		ctx.NotNull(smelter, "the iron smelter row loads");
		if (!smelter)
			return;

		ctx.Equal(5, LF_ProcessAction.MaxBatchesAt(smelter, 1), "SMELTING 1: 5 bars a run");
		ctx.Equal(60, LF_ProcessAction.SecondsAt(smelter, 1), "SMELTING 1: 60 s a run");
		ctx.Equal(6, LF_ProcessAction.MaxBatchesAt(smelter, 2), "SMELTING 2: 6 bars");
		ctx.Equal(7, LF_ProcessAction.MaxBatchesAt(smelter, 5), "SMELTING 5: 7 bars");
		ctx.Equal(45, LF_ProcessAction.SecondsAt(smelter, 7), "SMELTING 7: 45 s");
		ctx.Equal(8, LF_ProcessAction.MaxBatchesAt(smelter, 8), "SMELTING 8: 8 bars");
		ctx.Equal(35, LF_ProcessAction.SecondsAt(smelter, 9), "SMELTING 9: 35 s");
		ctx.Equal(10, LF_ProcessAction.MaxBatchesAt(smelter, 16), "SMELTING 16: 10 bars");
		ctx.Equal(10, LF_ProcessAction.MaxBatchesAt(smelter, 30), "SMELTING 30 keeps the top cap");
	}
}
