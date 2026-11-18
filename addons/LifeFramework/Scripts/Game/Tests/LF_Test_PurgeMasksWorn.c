// red-proof: RED-OK items/purge-masks-worn (fails with the hook on, 14 assertions), observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_PurgeMasksWorn`; RedProof() skips the equip of both masks. Hook off: green.
//! The two face masks (doll and hockey, tools/assets/doll-mask and hockey-mask) are wearables like the
//! balaclava (#1997): each prefab spawns, is a cloth item for the head-cover slot, and goes into the
//! character's equipped-loadout storage the way the Equip action inserts it. The shape a player sees is
//! judged by `tools\cli look item` and the state pictures of `tools\cli asset run`; this proves they equip.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_PurgeMasksWorn : LF_Test
{
	protected static const ResourceName DOLL_MASK = "{C243CFB581136C99}Prefabs/Characters/HeadGear/LF/DollMask.et";
	protected static const ResourceName HOCKEY_MASK = "{F049C702E4FE8063}Prefabs/Characters/HeadGear/LF/HockeyMask.et";

	override string GetName()
	{
		return "items/purge-masks-worn";
	}

	override void Run(LF_TestContext ctx)
	{
		WearMask(ctx, DOLL_MASK, "the doll mask");
		WearMask(ctx, HOCKEY_MASK, "the hockey mask");
	}

	protected void WearMask(LF_TestContext ctx, ResourceName prefab, string label)
	{
		IEntity character = LF_TestCharacterFixture.Spawn(ctx);
		if (!character)
			return;

		InventoryStorageManagerComponent storage = LF_InventoryUtils.GetResponsibleStorageManager(character);
		IEntity mask = LF_Utils.SpawnEntityPrefab(prefab, character.GetOrigin());
		ctx.True(mask != null, label + " prefab spawns");
		if (!mask || !storage)
		{
			LF_TestCharacterFixture.Delete(mask);
			LF_TestCharacterFixture.Delete(character);
			return;
		}

		BaseLoadoutClothComponent cloth = BaseLoadoutClothComponent.Cast(mask.FindComponent(BaseLoadoutClothComponent));
		ctx.True(cloth != null, label + " is a cloth item");
		if (cloth)
			ctx.True(LoadoutHeadCoverArea.Cast(cloth.GetAreaType()) != null, label + " takes the head-cover slot");

		bool worn;
		if (!RedProof())
			worn = storage.TryInsertItem(mask, EStoragePurpose.PURPOSE_LOADOUT_PROXY);
		ctx.True(worn, "the character puts on " + label);

		InventoryItemComponent item = InventoryItemComponent.Cast(mask.FindComponent(InventoryItemComponent));
		InventoryStorageSlot slot;
		if (item)
			slot = item.GetParentSlot();
		ctx.True(slot != null && EquipedLoadoutStorageComponent.Cast(slot.GetStorage()) != null, label + " sits in the character's equipped-loadout storage");

		LF_TestCharacterFixture.Delete(mask);
		LF_TestCharacterFixture.Delete(character);
	}
}
