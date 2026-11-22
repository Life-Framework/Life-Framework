// red-proof: RED-OK 2026-11-22 `tools\cli test --no-build --red LF_Test_StashContainerRefill` (the key grant skipped):
// [LFTEST]   - the refilled stock moves again (expected 3, got 0)
// Green with `--only` after the hook was off.

//! The short container (#4195, crime B3): a tally clerk leaves one container short on the books.
//! Only a player who carries the clerk's key can open it; the stock moves to that player once and the
//! container stays empty until its refill timer runs out, then the same stock is back.
//! The key and goods here are stand-ins (a house key, beer and an empty bottle): the pack sets the real
//! key, cartons and bottles on its container prefab.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_StashContainerRefill : LF_Test
{
	static const ResourceName KEY = "{3735389521255DBE}Prefabs/Items/HouseKeyItem.et";
	static const ResourceName CARTON = "{4CBB1F9A04D15772}Prefabs/Items/Drinks/BeerBottle.et";
	//! The fixture jacket holds three small items; a backpack gives the whole stock room.
	static const ResourceName PACK = "{06B68C58B72EAAC6}Prefabs/Items/Equipment/Backpacks/Backpack_ALICE_Medium.et";
	static const ResourceName BOTTLE = "{2B0213949BE91F38}Prefabs/Items/Resources/EmptyBeerBottle.et";

	override string GetName()
	{
		return "crime/stash-container-refill";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_StashContainerStock stock = new LF_StashContainerStock();
		stock.SetKeyPrefab(KEY);
		stock.AddEntry(CARTON, 2);
		stock.AddEntry(BOTTLE, 1);
		stock.SetRefillSeconds(600);

		ctx.True(stock.IsStocked(1000), "a fresh container is stocked");

		IEntity player = LF_TestCharacterFixture.Spawn(ctx);
		ctx.NotNull(player, "the fixture player spawns");
		if (!player)
			return;

		SCR_InventoryStorageManagerComponent storage = LF_Component<SCR_InventoryStorageManagerComponent>.Find(player);
		IEntity pack = LF_Utils.SpawnEntityPrefab(PACK, player.GetOrigin());
		ctx.True(storage && pack && storage.TryInsertItem(pack, EStoragePurpose.PURPOSE_LOADOUT_PROXY), "the fixture player wears a backpack");

		// Locked without the key.
		ctx.False(stock.HasKey(player), "a player without the key has no key");
		ctx.Equal(LF_EStashTake.NO_KEY, stock.Check(player, 1000), "the container refuses a player without the key");
		ctx.Equal(0, stock.Take(player, 1000), "nothing moves without the key");
		ctx.Equal(0, LF_InventoryUtils.GetAmount(player, CARTON), "no carton reached the keyless player");
		ctx.True(stock.IsStocked(1000), "a refused open leaves the stock in place");

		int added;
		if (!RedProof())
			added = LF_InventoryUtils.AddAmount(player, KEY, 1);

		ctx.Equal(1, added, "the player is handed the clerk's key");
		ctx.True(stock.HasKey(player), "the player now carries the key");

		// The stock moves to the player once.
		ctx.Equal(LF_EStashTake.OK, stock.Check(player, 1000), "the key opens the container");
		ctx.Equal(3, stock.Take(player, 1000), "the whole stock moves on the first take");
		ctx.Equal(2, LF_InventoryUtils.GetAmount(player, CARTON), "the player holds both cartons");
		ctx.Equal(1, LF_InventoryUtils.GetAmount(player, BOTTLE), "the player holds the bottle");
		ctx.False(stock.IsStocked(1000), "the container is empty after the take");
		ctx.Equal(LF_EStashTake.EMPTY, stock.Check(player, 1300), "a second open before the refill finds it empty");
		ctx.Equal(0, stock.Take(player, 1300), "a second take moves nothing");
		ctx.Equal(2, LF_InventoryUtils.GetAmount(player, CARTON), "no extra carton after the second take");

		// The refill timer restores it.
		ctx.False(stock.IsStocked(1599), "still empty one second before the refill");
		ctx.True(stock.IsStocked(1600), "the refill timer restores the stock");
		ctx.Equal(3, stock.Take(player, 1600), "the refilled stock moves again");
		ctx.Equal(4, LF_InventoryUtils.GetAmount(player, CARTON), "the refilled cartons reached the player");

		LF_TestCharacterFixture.Delete(player);
	}
}
