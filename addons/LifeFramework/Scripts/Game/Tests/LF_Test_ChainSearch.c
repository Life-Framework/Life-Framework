// red-proof: 2026-11-10 `tools/cli test --no-build --only LF_Test_ChainSearch` with the m_bContraband check dropped from LF_ChainSearch.IsGood failed `- an unflagged chain's output is not chain goods`, `- a search counts only the flagged units (expected 3, got 7)`. Restored; green.
// tier: LOGIC
//! Officer search for chain goods (#419): only goods of a chain flagged contraband are ever found,
//! never the input or an unflagged chain's output; a bigger load is harder to hide.
class LF_Test_ChainSearch : LF_Test
{
	protected static const ResourceName APPLE = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
	protected static const ResourceName JUICE = "{1DBD99E6347DE1AD}Prefabs/Items/Drinks/AppleJuice.et";
	protected static const ResourceName NUGGET = "{6853E2E2AE878611}Prefabs/Resources/Mining/IronNugget.et";
	protected static const ResourceName PLUM = "{C3E35D690317B5BE}Prefabs/Items/Food/Plum.et";

	override string GetName()
	{
		return "chains/search";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ChainDef Chain(string id, ResourceName input, ResourceName output, bool contraband, int bulk)
	{
		LF_ChainStepDef step = new LF_ChainStepDef();
		step.m_sStepId = "make";
		step.m_sStationType = "barrel";
		step.m_iWork = 5;
		step.m_aInputs = new array<ref LF_ProcessingInput>();
		step.m_aOutputs = new array<ref LF_ProcessingOutput>();

		LF_ProcessingInput stepIn = new LF_ProcessingInput();
		stepIn.m_InputPrefab = input;
		stepIn.m_iInputAmount = 1;
		step.m_aInputs.Insert(stepIn);

		LF_ProcessingOutput stepOut = new LF_ProcessingOutput();
		stepOut.m_OutputPrefab = output;
		stepOut.m_iOutputAmount = 1;
		step.m_aOutputs.Insert(stepOut);

		LF_ChainDef chain = new LF_ChainDef();
		chain.m_sChainId = id;
		chain.m_iTier = 1;
		chain.m_bContraband = contraband;
		chain.m_iBulk = bulk;
		chain.m_aSteps = new array<ref LF_ChainStepDef>();
		chain.m_aSteps.Insert(step);
		chain.m_aEquipment = new array<ref LF_ChainEquipmentDef>();
		return chain;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ResourceChainConfig config = new LF_ResourceChainConfig();
		config.m_aChains = new array<ref LF_ChainDef>();
		config.m_aChains.Insert(Chain("hooch", APPLE, JUICE, true, 2));
		config.m_aChains.Insert(Chain("legal", PLUM, NUGGET, false, 1));
		LF_ChainCatalog.ResetStatics();
		LF_ChainCatalog.Build(config, false);

		int bulk;
		ctx.True(LF_ChainSearch.IsGood(JUICE, bulk), "the flagged chain's output is chain goods");
		ctx.Equal(2, bulk, "the goods carry the chain's bulk");
		ctx.False(LF_ChainSearch.IsGood(NUGGET, bulk), "an unflagged chain's output is not chain goods");
		ctx.False(LF_ChainSearch.IsGood(APPLE, bulk), "an input of a flagged chain is not chain goods");

		array<ResourceName> carried = {APPLE, JUICE, NUGGET, PLUM};
		array<int> quantities = {5, 3, 4, 2};
		int units;
		int load;
		array<int> flagged = {};
		LF_ChainSearch.Tally(carried, quantities, units, load, flagged);
		ctx.Equal(3, units, "a search counts only the flagged units");
		ctx.Equal(6, load, "the load is units times bulk");
		ctx.Equal(1, flagged.Count(), "one carried entry is flagged");
		ctx.Equal(1, flagged[0], "the flagged entry is the goods");

		array<ResourceName> clean = {APPLE, NUGGET};
		array<int> cleanQuantities = {9, 9};
		LF_ChainSearch.Tally(clean, cleanQuantities, units, load, flagged);
		ctx.Equal(0, units, "a clean carry has no flagged units");
		ctx.False(LF_ChainSearch.Finds(load, 0.0), "nothing flagged is never found, whatever the roll");

		ctx.True(LF_ChainSearch.FindChance(4) > LF_ChainSearch.FindChance(1), "a bigger load is easier to find");
		ctx.False(LF_ChainSearch.Finds(1, 0.9), "a small load can stay hidden");
		ctx.True(LF_ChainSearch.Finds(1, 0.1), "a small load can be found");
		ctx.True(LF_ChainSearch.Finds(4, 0.99), "a bulky load is always found");

		LF_ChainCatalog.ResetStatics();
	}
}
