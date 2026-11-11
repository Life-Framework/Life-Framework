// red-proof: 2026-11-12, GetAmount skipped the equipped-weapon storage; `--only LF_Test_PickaxeCount` failed "a carried pickaxe counts as 1 (expected 1, got 0)" and "the inventory tool check sees the carried pickaxe". Fixed, green.
//------------------------------------------------------------------------------------------------
//! tool/pickaxe-count: a pickaxe is a weapon-slot item. It lands in the equipped-weapon storage,
//! which GetDepositItemCountByResource does not search, so GetAmount counted it as 0 and the
//! gather tool check (LF_GatherAction.LF_HasGatherTool, inventory mode) never saw a carried pickaxe.
// tier: WORLD
class LF_Test_PickaxeCount : LF_Test
{
	static const ResourceName PICKAXE = "{23341E2840F5ACB4}Prefabs/Tools/Pickaxe.et";

	override string GetName()
	{
		return "tool/pickaxe-count";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity body = LF_TestCharacterFixture.Spawn(ctx);
		if (!body)
			return;

		ctx.Equal(0, LF_InventoryUtils.GetAmount(body, PICKAXE), "no pickaxe before one is given");
		int added = LF_InventoryUtils.AddAmount(body, PICKAXE, 1);
		ctx.Equal(1, added, "the pickaxe is given");
		ctx.True(LF_TestCharacterFixture.HasItem(body, PICKAXE), "the pickaxe is on the character");
		ctx.Equal(1, LF_InventoryUtils.GetAmount(body, PICKAXE), "a carried pickaxe counts as 1");
		ctx.True(LF_GatherAction.HasToolIn(body, PICKAXE, true), "the inventory tool check sees the carried pickaxe");
		ctx.Equal(1, LF_InventoryUtils.RemoveAmount(body, PICKAXE, 1), "the pickaxe can be taken back");
		ctx.Equal(0, LF_InventoryUtils.GetAmount(body, PICKAXE), "none is left after removal");
		LF_TestCharacterFixture.Delete(body);
	}
}
