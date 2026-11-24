// red-proof: observed 2026-11-24 (#3243), batched run `tools\cli test --no-build --red LF_Test_OreNodeGate`: RedProof() skips LevelRefusal, so a gated row reads as open and every `refused` assertion fails.
//! A gather node row can carry a job-level gate (`m_sLevelJob`, `m_iMinLevel`): `LF_OreNodeComponent.LevelRefusal` gives
//! the refusal key in the job's words below the level and nothing at or above it, and a row with no job or a minimum of
//! 0 or 1 is open (the machine rule).
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_OreNodeGate : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "mining/ore-node-gate";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ResourceNodeDef Row(string job, int level)
	{
		LF_ResourceNodeDef row = new LF_ResourceNodeDef();
		row.m_sNodeId = "gate-test";
		row.m_sLevelJob = job;
		row.m_iMinLevel = level;
		return row;
	}

	//------------------------------------------------------------------------------------------------
	protected string Refusal(LF_ResourceNodeDef row, int level)
	{
		if (RedProof())
			return "";

		return LF_OreNodeComponent.LevelRefusal(row, level);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ResourceNodeDef coal = Row("MINER", 4);
		ctx.EqualStr("#LF-Level_Far_Miner", Refusal(coal, 1), "level 1 is refused at a MINER 4 node, far");
		ctx.EqualStr("#LF-Level_Near_Miner", Refusal(coal, 3), "level 3 is refused at a MINER 4 node, nearly there");
		ctx.EqualStr("", Refusal(coal, 4), "level 4 meets a MINER 4 node");
		ctx.EqualStr("", Refusal(coal, 12), "level 12 meets a MINER 4 node");

		ctx.EqualStr("", LF_OreNodeComponent.LevelRefusal(Row("", 4), 1), "a node naming no job is open");
		ctx.EqualStr("", LF_OreNodeComponent.LevelRefusal(Row("MINER", 0), 1), "a minimum of 0 is open");
		ctx.EqualStr("", LF_OreNodeComponent.LevelRefusal(Row("MINER", 1), 1), "a minimum of 1 is open");
		ctx.EqualStr("", LF_OreNodeComponent.LevelRefusal(null, 1), "no row is open");

		// The shipped core rows carry no gate: the plain iron and coal nodes stay open to a first-day miner.
		LF_ResourceNodeDef iron = LF_ResourceChainCatalog.FindNode("iron");
		ctx.NotNull(iron, "the core iron node exists");
		if (iron)
			ctx.EqualStr("", LF_OreNodeComponent.LevelRefusal(iron, 1), "the core iron node is open at level 1");
	}
}
