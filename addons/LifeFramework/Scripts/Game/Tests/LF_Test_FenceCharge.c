// red-proof: 2026-11-10 `tools/cli test --no-build --only LF_Test_FenceCharge` with LF_Charge.et's ActionsManagerComponent GUID back at the duplicate {FAE0195B7214D19E} failed `- the fence charge lands in the inventory (expected 1, got 0)`; restored to the inherited {5AA692DC30B9C623}; green.
// tier: WORLD
//! The fence charge (LF_Charge.et) spawns, goes into a character's inventory and counts as a
//! breach charge for LF_Breach. Deletes everything it spawns.
class LF_Test_FenceCharge : LF_Test
{
	override string GetName()
	{
		return "crime/fence-charge";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity character = LF_TestCharacterFixture.Spawn(ctx);
		if (!character)
			return;

		ctx.False(LF_Breach.HasCharge(character), "no charge before the handover");
		ctx.Equal(1, LF_InventoryUtils.AddAmount(character, LF_Breach.FENCE_CHARGE_PREFAB, 1), "the fence charge lands in the inventory");
		ctx.True(LF_TestCharacterFixture.HasItem(character, LF_Breach.FENCE_CHARGE_PREFAB), "the character holds the fence charge");
		ctx.True(LF_Breach.HasCharge(character), "LF_Breach accepts the fence charge");

		LF_TestCharacterFixture.Delete(character);
	}
}
