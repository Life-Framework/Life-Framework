// red-proof: 2026-11-17 `tools/cli test --no-build --red LF_Test_ScannerRegistry,...`: RED-OK comms/scanner-registry, "a spawned scanner is in the registry" failed with the hook on. Green with the fix (--only, 8 passed).
// tier: WORLD
//! #2404: a police call visits the scanner registry, not every player's inventory. Every scanner
//! registers on the server and leaves on delete, and its holder is the carrier whether the scanner
//! sits in a pocket or inside a worn backpack, so the registry finds what the inventory walk found.
class LF_Test_ScannerRegistry : LF_Test
{
	override string GetName()
	{
		return "comms/scanner-registry";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity character = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
		if (!character)
			return;

		IEntity pocketEntity = LF_Utils.SpawnEntityPrefab(LF_Scanner.PREFAB, character.GetOrigin());
		IEntity bagEntity = LF_Utils.SpawnEntityPrefab(LF_Scanner.PREFAB, character.GetOrigin());
		ctx.True(pocketEntity != null && bagEntity != null, "two scanners spawn");
		if (!pocketEntity || !bagEntity)
		{
			Cleanup(character, pocketEntity, bagEntity);
			return;
		}

		LF_ScannerComponent pocket = LF_ScannerComponent.Cast(pocketEntity.FindComponent(LF_ScannerComponent));
		LF_ScannerComponent bag = LF_ScannerComponent.Cast(bagEntity.FindComponent(LF_ScannerComponent));
		ctx.True(pocket != null && bag != null, "both carry LF_ScannerComponent");
		if (!pocket || !bag)
		{
			Cleanup(character, pocketEntity, bagEntity);
			return;
		}

		bool registered = LF_ScannerComponent.All().Find(pocket) >= 0 && LF_ScannerComponent.All().Find(bag) >= 0;
		if (RedProof())
			registered = false;
		ctx.True(registered, "a spawned scanner is in the registry");
		ctx.True(pocket.Holder() == pocketEntity, "a scanner on the ground is held by nobody but itself");

		InventoryStorageManagerComponent manager = LF_InventoryUtils.GetResponsibleStorageManager(character);
		ctx.True(manager.TryInsertItem(pocketEntity), "a scanner goes into the character's inventory");
		array<IEntity> backpacks = LF_InventoryUtils.FindItemsByPrefab(manager, LF_TestCarriedInventoryKit.BACKPACK_PREFAB);
		BaseInventoryStorageComponent backpackStorage;
		if (!backpacks.IsEmpty())
			backpackStorage = BaseInventoryStorageComponent.Cast(backpacks[0].FindComponent(BaseInventoryStorageComponent));
		ctx.NotNull(backpackStorage, "the worn backpack has a storage");
		if (backpackStorage)
			ctx.True(manager.TryInsertItemInStorage(bagEntity, backpackStorage), "a scanner goes inside the worn backpack");

		ctx.True(pocket.Holder() == character, "a carried scanner's holder is the carrier");
		ctx.True(bag.Holder() == character, "a scanner inside a worn backpack is held by the carrier too");

		// The registry finds what the old inventory walk found.
		array<LF_ScannerComponent> walked = {};
		LF_Scanner.FindCarried(character, walked);
		int heldByCharacter;
		foreach (LF_ScannerComponent scanner : LF_ScannerComponent.All())
		{
			if (scanner && scanner.Holder() == character)
				heldByCharacter++;
		}
		ctx.Equal(walked.Count(), heldByCharacter, "the registry and the inventory walk agree on the carrier's scanners");
		ctx.Equal(2, heldByCharacter, "both scanners are the carrier's");

		SCR_EntityHelper.DeleteEntityAndChildren(bagEntity);
		ctx.True(LF_ScannerComponent.All().Find(bag) < 0, "a deleted scanner leaves the registry");

		Cleanup(character, pocketEntity, null);
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(IEntity character, IEntity a, IEntity b)
	{
		if (a)
			SCR_EntityHelper.DeleteEntityAndChildren(a);

		if (b)
			SCR_EntityHelper.DeleteEntityAndChildren(b);

		if (character)
			LF_TestCharacterFixture.Delete(character);
	}
}
