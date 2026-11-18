// red-proof: RED-OK items/balaclava-worn (fails with the hook on, 7 assertions), observed 2026-11-18 with
// `tools\cli test --no-build --red LF_Test_BalaclavaWorn`; RedProof() skips the equip. Green with it off: passed=2 failed=0.
//! The balaclava (#1997) is a wearable: its prefab spawns, takes the head-cover slot of the character's
//! equipped loadout when inserted the way the Equip action does, and carries the cloth component with
//! the worn and ground models. The shape a player sees is judged by `tools\cli look item` and the
//! state pictures of `tools\cli asset run balaclava`; this proves the item equips.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_BalaclavaWorn : LF_Test
{
	protected static const ResourceName BALACLAVA = "{F1971C6519B5DC9F}Prefabs/Characters/HeadGear/LF/Balaclava.et";

	override string GetName()
	{
		return "items/balaclava-worn";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity character = LF_TestCharacterFixture.Spawn(ctx);
		if (!character)
			return;

		InventoryStorageManagerComponent storage = LF_InventoryUtils.GetResponsibleStorageManager(character);
		IEntity mask = LF_Utils.SpawnEntityPrefab(BALACLAVA, character.GetOrigin());
		ctx.True(mask != null, "the balaclava prefab spawns");
		if (!mask || !storage)
		{
			LF_TestCharacterFixture.Delete(mask);
			LF_TestCharacterFixture.Delete(character);
			return;
		}

		BaseLoadoutClothComponent cloth = BaseLoadoutClothComponent.Cast(mask.FindComponent(BaseLoadoutClothComponent));
		ctx.True(cloth != null, "the balaclava is a cloth item");
		if (cloth)
			ctx.True(LoadoutHeadCoverArea.Cast(cloth.GetAreaType()) != null, "it takes the head-cover slot");

		bool worn;
		if (!RedProof())
			worn = storage.TryInsertItem(mask, EStoragePurpose.PURPOSE_LOADOUT_PROXY);
		ctx.True(worn, "the character puts it on");

		InventoryItemComponent item = InventoryItemComponent.Cast(mask.FindComponent(InventoryItemComponent));
		InventoryStorageSlot slot;
		if (item)
			slot = item.GetParentSlot();
		ctx.True(slot != null && EquipedLoadoutStorageComponent.Cast(slot.GetStorage()) != null, "it sits in the character's equipped-loadout storage");

		LF_TestCharacterFixture.Delete(mask);
		LF_TestCharacterFixture.Delete(character);
	}
}
