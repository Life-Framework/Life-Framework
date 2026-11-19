// red-proof: before the rows existed (2026-11-19) the first run failed "smelter-iron is in the chain config",
// "refinery-copper is in the chain config", "cement-works is in the chain config" and the three node rows.
// `tools\cli test --no-build --red LF_Test_MiningChainRecipes` flips the expected gate to a number no row has.

//! The three mining chains' rows (#3232): each plant is a two-input machine with its level gate and
//! batch cap, and the new gather nodes are usable with the right tool and job family.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_MiningChainRecipes : LF_Test
{
	override string GetName()
	{
		return "mining/chain-recipes";
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckMachine(LF_TestContext ctx, string id, string levelJob, int minLevel, int batches, int outAmount)
	{
		LF_ResourceMachineDef machine = LF_ResourceChainCatalog.FindMachine(id);
		ctx.NotNull(machine, id + " is in the chain config");
		if (!machine)
			return;

		ctx.True(LF_ResourceChainCatalog.IsMachineUsable(machine), id + " is usable");
		ctx.Equal(2, machine.m_aInputs.Count(), id + " takes two inputs");
		ctx.Equal(1, machine.m_aOutputs.Count(), id + " makes one product");
		ctx.EqualStr(levelJob, machine.m_sLevelJob, id + " gate job");
		if (RedProof())
			minLevel = minLevel + 100;

		ctx.Equal(minLevel, machine.m_iMinLevel, id + " gate level");
		ctx.Equal(batches, machine.m_iMaxBatches, id + " batch cap");
		ctx.True(machine.m_iProcessSeconds > 0, id + " takes time");
		ctx.Equal(outAmount, machine.m_aOutputs[0].m_iOutputAmount, id + " output per batch");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckNode(LF_TestContext ctx, string id, string family, int stock, bool regrows)
	{
		LF_ResourceNodeDef node = LF_ResourceChainCatalog.FindNode(id);
		ctx.NotNull(node, id + " node is in the chain config");
		if (!node)
			return;

		ctx.True(LF_ResourceChainCatalog.IsNodeUsable(node), id + " node is usable");
		ctx.True(!node.m_ToolPrefab.IsEmpty(), id + " needs a tool");
		ctx.EqualStr(family, LF_GatherTools.JobFamily(node), id + " job family");
		ctx.Equal(stock, node.m_iMaxGathers, id + " stock");
		ctx.True(regrows == (node.m_fRegrowMs > 0), id + " regrows by stock");
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		CheckMachine(ctx, "smelter-iron", "MINER", 4, 5, 1);
		CheckMachine(ctx, "refinery-copper", "MINER", 3, 3, 1);
		CheckMachine(ctx, "cement-works", "", 0, 10, 2);

		CheckNode(ctx, "coal", "MINING", 6, true);
		CheckNode(ctx, "limestone", "DIGGING", 20, true);
		CheckNode(ctx, "sand-beach", "DIGGING", 10, true);

		LF_ResourceNodeDef limestone = LF_ResourceChainCatalog.FindNode("limestone");
		if (limestone)
			ctx.Equal(180000, limestone.m_fRegrowMs, "limestone grows back in 3 minutes");
	}
}
