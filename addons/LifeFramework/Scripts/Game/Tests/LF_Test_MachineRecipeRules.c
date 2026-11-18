// red-proof: `tools\cli test --no-build --red LF_Test_MachineRecipeRules` (2026-11-18) printed
// RED PROOF OK. With the row lacking its gate and cap it failed "the row's cap is read (expected 5,
// got 1)", "plenty runs the cap (expected 5, got 1)" and "7 ore and 3 coal run 3 batches (expected 3, got 1)".

//! Machine rules for the mining chains (#3226): the job-level gate, whole batches up to a cap,
//! the first missing input named, and scaled rows that leave the catalog row alone.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_MachineRecipeRules : LF_Test
{
	static const ResourceName ORE = "{6853E2E2AE878611}Prefabs/Resources/Mining/IronNugget.et";
	// Any second prefab stands in for the reagent until D2 adds coal.
	static const ResourceName REAGENT = "{8F81BD0BF343CF49}Prefabs/Resources/Mining/GoldNugget.et";
	static const ResourceName BAR = "{BCD1E8CD7F975074}Prefabs/Resources/Mining/IronBar.et";

	override string GetName()
	{
		return "processing/machine-recipe-rules";
	}

	protected LF_ResourceMachineDef Smelter()
	{
		LF_ResourceMachineDef machine = new LF_ResourceMachineDef();
		machine.m_sMachineId = "test-smelter";
		machine.m_aInputs = new array<ref LF_ProcessingInput>();
		machine.m_aOutputs = new array<ref LF_ProcessingOutput>();

		LF_ProcessingInput ore = new LF_ProcessingInput();
		ore.m_InputPrefab = ORE;
		ore.m_iInputAmount = 2;
		machine.m_aInputs.Insert(ore);

		LF_ProcessingInput coal = new LF_ProcessingInput();
		coal.m_InputPrefab = REAGENT;
		coal.m_iInputAmount = 1;
		machine.m_aInputs.Insert(coal);

		LF_ProcessingOutput bar = new LF_ProcessingOutput();
		bar.m_OutputPrefab = BAR;
		bar.m_iOutputAmount = 1;
		machine.m_aOutputs.Insert(bar);

		// Red proof: the row without its gate and cap, as every machine was before #3226.
		if (!RedProof())
		{
			machine.m_sLevelJob = "MINER";
			machine.m_iMinLevel = 4;
			machine.m_iMaxBatches = 5;
		}
		return machine;
	}

	override void Run(LF_TestContext ctx)
	{
		LF_ResourceMachineDef smelter = Smelter();
		ctx.True(LF_ProcessAction.IsRecipeConfigured(smelter.m_aInputs, smelter.m_aOutputs), "two-input smelter recipe is configured");

		// Level gate: below refuses in the miner's words, at or above passes, no job never refuses.
		ctx.True(!LF_ProcessAction.LevelRefusal(smelter, 2).IsEmpty(), "MINER 2 is refused at a MINER 4 smelter");
		ctx.EqualStr("#LF-Level_Near_Miner", LF_ProcessAction.LevelRefusal(smelter, 3), "one level short reads the near miner words");
		ctx.True(LF_ProcessAction.LevelRefusal(smelter, 4).IsEmpty(), "MINER 4 passes");
		ctx.True(LF_ProcessAction.LevelRefusal(null, 1).IsEmpty(), "a prefab recipe with no row has no gate");

		LF_ResourceMachineDef open = Smelter();
		open.m_sLevelJob = "";
		ctx.True(LF_ProcessAction.LevelRefusal(open, 1).IsEmpty(), "no job, no gate");

		// Batches: as many whole sets as held, capped; a short reagent stops everything.
		array<int> plenty = {20, 10};
		array<int> threeSets = {7, 3};
		array<int> noCoal = {8, 0};
		int cap = LF_ProcessAction.MaxBatches(smelter);
		ctx.Equal(5, cap, "the row's cap is read");
		ctx.Equal(1, LF_ProcessAction.MaxBatches(null), "no row runs one batch");
		ctx.Equal(5, LF_ProcessAction.BatchesFor(plenty, smelter.m_aInputs, cap), "plenty runs the cap");
		ctx.Equal(3, LF_ProcessAction.BatchesFor(threeSets, smelter.m_aInputs, cap), "7 ore and 3 coal run 3 batches");
		ctx.Equal(0, LF_ProcessAction.BatchesFor(noCoal, smelter.m_aInputs, cap), "no coal runs nothing");
		ctx.Equal(1, LF_ProcessAction.BatchesFor(plenty, smelter.m_aInputs, 0), "a zero cap still runs one batch");

		// The refusal names the reagent, not "no items".
		ctx.Equal(1, LF_ProcessAction.MissingInputIndex(noCoal, smelter.m_aInputs), "coal is the missing input");
		ctx.Equal(-1, LF_ProcessAction.MissingInputIndex(threeSets, smelter.m_aInputs), "nothing is missing with one set");
		ctx.True(LF_ProcessAction.NeedsText(REAGENT) != LF_ProcessAction.NeedsText(ResourceName.Empty), "the refusal names the item");

		// Scaled rows carry batches times the amounts and leave the catalog row as it was.
		array<ref LF_ProcessingInput> inputs = LF_ProcessAction.ScaleInputs(smelter.m_aInputs, 3);
		array<ref LF_ProcessingOutput> outputs = LF_ProcessAction.ScaleOutputs(smelter.m_aOutputs, 3);
		ctx.Equal(6, inputs[0].m_iInputAmount, "3 batches take 6 ore");
		ctx.Equal(3, inputs[1].m_iInputAmount, "3 batches take 3 coal");
		ctx.Equal(3, outputs[0].m_iOutputAmount, "3 batches make 3 bars");
		ctx.Equal(2, smelter.m_aInputs[0].m_iInputAmount, "the catalog row is untouched");
	}
};
