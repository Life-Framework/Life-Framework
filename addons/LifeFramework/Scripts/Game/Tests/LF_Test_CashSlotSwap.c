// red-proof: prefabs/cash-slot-swap, observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_CashSlotSwap`:
// skipping the legacy loop in LF_MoneyUtils.GetCash failed it with `- the old stacks still count after the swap (expected 237, got 0)`,
// `- 137 left (expected 137, got 0)` and `- both count (expected 187, got 50)`. Restored; green.

//------------------------------------------------------------------------------------------------
//! Cash after a pack swaps the `cash` slot (#1130). A character holds 237 cash under the core
//! MoneyStack, then a pack points the slot at another prefab: the stacks still count, are spent
//! first, and new cash is the slot's prefab. A prefab the save alias table (#965) leads to the slot
//! prefab counts as the same cash. Save format untouched: the stacks are plain prefab items.
// tier: WORLD
class LF_Test_CashSlotSwap : LF_Test
{
	static const ResourceName CORE_CASH = "{5439738849229352}Prefabs/Items/Currencies/MoneyStack.et";
	static const ResourceName PACK_CASH = "{6853E2E2AE878611}Prefabs/Resources/Mining/IronNugget.et";
	static const ResourceName OLD_CASH = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "prefabs/cash-slot-swap";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_SaveIds.ResetStatics();
		LF_ConfigService.Reset();
		LF_ConfigService.SetPackSource(new LF_ConfigPackSource());
		LF_PrefabSlots.ResetStatics();
		ctx.Equal(0, LF_MoneyUtils.LegacyCashPrefabs().Count(), "core cash: nothing older to count");

		IEntity character = LF_TestCharacterFixture.Spawn(ctx);
		if (!character)
			return;

		ctx.Equal(237, LF_MoneyUtils.AddCash(character, 237), "237 cash saved under the core prefab");
		ctx.Equal(237, LF_InventoryUtils.GetAmount(character, CORE_CASH), "the stacks are core MoneyStack");

		// A pack swaps the slot after that save.
		LF_ConfigService.SetPackSource(new LF_Test_PrefabSlotsPackStub());
		LF_PrefabSlots.ResetStatics();
		ctx.EqualStr(PACK_CASH, LF_MoneyUtils.CashPrefab(), "the pack swapped the cash slot");
		array<ResourceName> legacy = LF_MoneyUtils.LegacyCashPrefabs();
		ctx.Equal(1, legacy.Count(), "the core cash is the one older prefab");
		if (legacy.Count() == 1)
			ctx.EqualStr(CORE_CASH, legacy[0], "the older prefab is the core MoneyStack");

		ctx.Equal(237, LF_MoneyUtils.GetCash(character), "the old stacks still count after the swap");
		ctx.Equal(237, LF_MoneyUtils.GetCash(LF_InventoryUtils.GetResponsibleStorageManager(character)), "counted the same through the storage manager");

		ctx.Equal(100, LF_MoneyUtils.RemoveCash(character, 100), "100 is paid out of the old stacks");
		ctx.Equal(137, LF_MoneyUtils.GetCash(character), "137 left");
		ctx.Equal(50, LF_MoneyUtils.AddCash(character, 50), "50 earned lands as the slot's prefab");
		ctx.Equal(50, LF_InventoryUtils.GetAmount(character, PACK_CASH), "the new cash is the pack prefab");
		ctx.Equal(137, LF_InventoryUtils.GetAmount(character, CORE_CASH), "the old stacks are untouched by the earning");
		ctx.Equal(187, LF_MoneyUtils.GetCash(character), "both count");

		ctx.Equal(150, LF_MoneyUtils.RemoveCash(character, 150), "150 is paid: old stacks first, then the new ones");
		ctx.Equal(0, LF_InventoryUtils.GetAmount(character, CORE_CASH), "the old stacks are spent");
		ctx.Equal(37, LF_InventoryUtils.GetAmount(character, PACK_CASH), "the rest came from the new stacks");
		ctx.Equal(37, LF_MoneyUtils.GetCash(character), "37 left");
		ctx.Equal(37, LF_MoneyUtils.RemoveCash(character, 500), "a request over the balance removes what there is");
		LF_TestCharacterFixture.Delete(character);

		// An alias (#965) that leads to the slot prefab names more cash of an older save.
		LF_SaveIds.ResetStatics();
		LF_SaveIds.Add(LF_SaveIds.KIND_PREFAB, OLD_CASH, PACK_CASH);
		LF_PrefabSlots.ResetStatics();
		legacy = LF_MoneyUtils.LegacyCashPrefabs();
		ctx.Equal(2, legacy.Count(), "the core cash and the aliased prefab both count");
		bool hasOld;
		foreach (ResourceName prefab : legacy)
		{
			if (LF_Utils.SamePrefab(prefab, OLD_CASH))
				hasOld = true;
		}

		ctx.True(hasOld, "the prefab the alias table leads to the slot prefab is counted");

		LF_SaveIds.ResetStatics();
		LF_ConfigService.Reset();
		LF_PrefabSlots.ResetStatics();
	}
}
