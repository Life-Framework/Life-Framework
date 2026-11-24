// red-proof: 2026-11-24, `LF_ToolConditionComponent.Held` read the right hand and left gadget only; at the hardware bench a drawn Worn pickaxe (the current weapon) refused every repair row with #LF-ToolBench_NoTool (#4865). `--red LF_Test_ToolHeldWeapon` leaves the pickaxe out of the weapon source, so "a drawn pickaxe is the held tool" and the bench repair fail.
//! tool/held-weapon (#4865): a tool drawn as the current weapon is the held tool. `Held` (the bench rows,
//! the wear readout, every wear and repair caller) reads the hands through LF_GatherTools.HeldItems, so a
//! drawn pickaxe is found, repaired by the bench, and still no axe. The draw animation outlasts a one-frame
//! test, so the weapon source is handed to CollectHeld directly; the live draw is proved in the session.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_ToolHeldWeapon : LF_Test
{
	static const ResourceName PICKAXE = "{23341E2840F5ACB4}Prefabs/Tools/Pickaxe.et";
	static const ResourceName SCRAP_STAND_IN = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";

	override string GetName()
	{
		return "tool/held-weapon";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity character = LF_TestCharacterFixture.Spawn(ctx);
		if (!character)
			return;

		IEntity pickaxe = LF_Utils.SpawnEntityPrefab(PICKAXE, character.GetOrigin() + "3 0.5 0");
		LF_ToolConditionComponent tool = LF_ToolConditionComponent.Of(pickaxe);
		ctx.NotNull(tool, "the pickaxe carries a tool condition");
		if (!tool)
		{
			LF_TestCharacterFixture.Delete(pickaxe);
			LF_TestCharacterFixture.Delete(character);
			return;
		}

		ctx.True(LF_ToolConditionComponent.Held(character, LF_ToolBench.KIND_PICKAXE) == null, "empty hands hold no pickaxe");

		// The pickaxe as the current weapon with both hand slots empty: what a player who drew it has.
		IEntity drawn = pickaxe;
		if (RedProof())
			drawn = null;
		array<IEntity> hands = {};
		LF_GatherTools.CollectHeld(null, null, drawn, hands);
		ctx.True(LF_ToolConditionComponent.HeldAmong(hands, LF_ToolBench.KIND_PICKAXE) == tool, "a drawn pickaxe is the held tool");
		ctx.True(LF_ToolConditionComponent.HeldAmong(hands, LF_ToolBench.KIND_AXE) == null, "a drawn pickaxe is no axe");

		// The bench: the held tool, found this way, is worn, then repaired.
		tool.SetCondition(40);
		LF_MoneyUtils.AddCash(character, 50);
		LF_InventoryUtils.AddAmount(character, SCRAP_STAND_IN, 1);
		LF_ToolConditionComponent found = LF_ToolConditionComponent.HeldAmong(hands, LF_ToolBench.KIND_PICKAXE);
		if (found)
		{
			LF_EToolBenchResult result = LF_ToolBench.Repair(character, found, SCRAP_STAND_IN, LF_ToolBench.FEE);
			ctx.Equal(LF_EToolBenchResult.DONE, result, "the bench repairs the drawn pickaxe");
			ctx.EqualFloat(LF_ToolCondition.FULL, tool.GetCondition(), 0.001, "the drawn pickaxe is back to full");
		}
		else
		{
			ctx.Fail("the bench found no tool in the hands (it would read #LF-ToolBench_NoTool)");
		}

		LF_TestCharacterFixture.Delete(pickaxe);
		LF_TestCharacterFixture.Delete(character);
	}
}
