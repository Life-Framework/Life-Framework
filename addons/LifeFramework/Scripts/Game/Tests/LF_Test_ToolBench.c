// red-proof: 2026-11-23, `--red LF_Test_ToolBench` skips the paid repair: RED-OK, "the fee is taken (expected 38, got 50)", "one scrap is taken (expected 1, got 2)". Green with `--only` after.
//! tool/bench (#4187, WK-W9): a repair at the hardware bench restores a worn tool to full, takes the
//! fee and one scrap part on the server, and refuses without cash or scrap, taking nothing and saying
//! why in words.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_ToolBench : LF_Test
{
	// A core item stands in for the pack's scrap metal: the bench takes whatever prefab it is given.
	static const ResourceName SCRAP_STAND_IN = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";

	override string GetName()
	{
		return "tool/bench";
	}

	override void Run(LF_TestContext ctx)
	{
		int fee = LF_ToolBench.FEE;
		ctx.Equal(12, fee, "a repair costs $12");

		// Refusals are said in words, one key each.
		ctx.True(LF_ToolBench.ResultKey(LF_EToolBenchResult.NO_MONEY) != LF_ToolBench.ResultKey(LF_EToolBenchResult.NO_SCRAP), "no cash and no scrap read differently");
		ctx.True(LF_ToolBench.ResultKey(LF_EToolBenchResult.NO_MONEY).StartsWith("#LF-"), "the no-cash refusal is a string key");

		IEntity character = LF_TestCharacterFixture.Spawn(ctx);
		if (!character)
			return;

		LF_ToolCondition tool = LF_ToolCondition.Create(LF_ToolCondition.TIER_STANDARD);
		tool.m_fCondition = 20;

		// No cash: refused, nothing taken, the tool stays worn.
		LF_InventoryUtils.AddAmount(character, SCRAP_STAND_IN, 1);
		LF_EToolBenchResult broke = LF_ToolBench.Repair(character, tool, SCRAP_STAND_IN, fee);
		ctx.Equal(LF_EToolBenchResult.NO_MONEY, broke, "no cash refuses the repair");
		ctx.Equal(1, LF_InventoryUtils.GetAmount(character, SCRAP_STAND_IN), "the scrap stays when the cash is short");
		ctx.EqualFloat(20, tool.m_fCondition, 0.001, "a refused repair leaves the tool worn");

		// Cash but no scrap: refused, the cash stays.
		LF_InventoryUtils.RemoveAmount(character, SCRAP_STAND_IN, 1);
		LF_MoneyUtils.AddCash(character, 50);
		LF_EToolBenchResult noScrap = LF_ToolBench.Repair(character, tool, SCRAP_STAND_IN, fee);
		ctx.Equal(LF_EToolBenchResult.NO_SCRAP, noScrap, "no scrap refuses the repair");
		ctx.Equal(50, LF_MoneyUtils.GetCash(character), "the cash stays when the scrap is missing");
		ctx.EqualFloat(20, tool.m_fCondition, 0.001, "still worn without scrap");

		// Cash and scrap: repaired to full, $12 and one scrap gone.
		LF_InventoryUtils.AddAmount(character, SCRAP_STAND_IN, 2);
		LF_EToolBenchResult done = LF_EToolBenchResult.FAILED;
		if (!RedProof())
			done = LF_ToolBench.Repair(character, tool, SCRAP_STAND_IN, fee);
		ctx.Equal(LF_EToolBenchResult.DONE, done, "cash and scrap repair the tool");
		ctx.EqualFloat(LF_ToolCondition.FULL, tool.m_fCondition, 0.001, "the repair restores full condition");
		ctx.Equal(38, LF_MoneyUtils.GetCash(character), "the fee is taken");
		ctx.Equal(1, LF_InventoryUtils.GetAmount(character, SCRAP_STAND_IN), "one scrap is taken");

		// A full tool is not repaired again.
		LF_EToolBenchResult again = LF_ToolBench.Repair(character, tool, SCRAP_STAND_IN, fee);
		ctx.Equal(LF_EToolBenchResult.ALREADY_FULL, again, "a full tool is refused");
		ctx.Equal(38, LF_MoneyUtils.GetCash(character), "no fee for a full tool");

		// The per-player store keeps one tool per kind.
		LF_ToolBench.ResetStatics();
		LF_ToolCondition pick = LF_ToolBench.ToolOf("uid-a", LF_ToolBench.KIND_PICKAXE);
		pick.m_fCondition = 5;
		ctx.EqualFloat(5, LF_ToolBench.ToolOf("uid-a", LF_ToolBench.KIND_PICKAXE).m_fCondition, 0.001, "the store returns the same pickaxe");
		ctx.EqualFloat(LF_ToolCondition.FULL, LF_ToolBench.ToolOf("uid-a", LF_ToolBench.KIND_AXE).m_fCondition, 0.001, "another kind is its own tool");
		ctx.True(LF_ToolBench.ToolOf("", LF_ToolBench.KIND_AXE) == null, "no uid, no tool");
		LF_ToolBench.ResetStatics();

		LF_TestCharacterFixture.Delete(character);
	}
}
