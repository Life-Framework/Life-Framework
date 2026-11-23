// red-proof: 2026-11-23 `test --no-build --red LF_Test_EveronContrabandItems` RED-OK: with the `RedProof()` hook swapping the real carton and bottle for an apple, `no cartons are left (expected 0, got 3)` and `no bottle is left (expected 0, got 3)` fail; with the real items it is green.
//! #4198 (crime B2): the pack's cigarette carton and vodka bottle are real items. Each carries the
//! contraband marker (carton = tobacco, bottle = alcohol) with no prefab registration, both go into a field
//! pack, and a stop search of a car lists the real items by class and the seizure takes them once. Deletes what it spawns.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronContrabandItems : LF_Test
{
	static const ResourceName CARTON = "{37C18698D9B51EAE}Prefabs/Items/Contraband/LF_CigaretteCarton.et";
	static const ResourceName VODKA = "{23AE52677634DDD2}Prefabs/Items/Contraband/LF_VodkaBottle.et";
	static const ResourceName APPLE = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
	static const ResourceName PACK = "{06B68C58B72EAAC6}Prefabs/Items/Equipment/Backpacks/Backpack_ALICE_Medium.et";
	static const ResourceName CAR = "{54C3CC22DEBD57BE}Prefabs/Vehicles/Wheeled/S105/S105_beige.et";

	override string GetName()
	{
		return "everon/contraband-items";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_VehicleSearchGoods.ResetStatics();

		ResourceName carton = CARTON;
		ResourceName vodka = VODKA;
		if (RedProof())
		{
			carton = APPLE;
			vodka = APPLE;
		}

		IEntity robber = LF_TestCharacterFixture.Spawn(ctx);
		if (!robber)
			return;

		InventoryStorageManagerComponent inventory = LF_InventoryUtils.GetResponsibleStorageManager(robber);
		IEntity pack = LF_Utils.SpawnEntityPrefab(PACK, robber.GetOrigin());
		ctx.True(pack != null && inventory.TryInsertItem(pack, EStoragePurpose.PURPOSE_LOADOUT_PROXY), "the character wears a field pack");

		// Both items go into the pack's own storage, the insert the juice bottle failed on 0.2.1 (#4369).
		BaseInventoryStorageComponent packStorage;
		if (pack)
			packStorage = BaseInventoryStorageComponent.Cast(pack.FindComponent(BaseInventoryStorageComponent));
		ctx.True(packStorage != null, "the pack has a storage");

		IEntity cartonItem = LF_Utils.SpawnEntityPrefab(carton, robber.GetOrigin());
		IEntity vodkaItem = LF_Utils.SpawnEntityPrefab(vodka, robber.GetOrigin());
		ctx.True(cartonItem != null, "the carton spawns");
		ctx.True(vodkaItem != null, "the bottle spawns");
		if (packStorage && cartonItem)
			ctx.True(inventory.TryInsertItemInStorage(cartonItem, packStorage), "the carton goes into the backpack");
		if (packStorage && vodkaItem)
			ctx.True(inventory.TryInsertItemInStorage(vodkaItem, packStorage), "the bottle goes into the backpack");

		ctx.Equal(LF_EVehicleSearchGoodsClass.TOBACCO, LF_VehicleSearchGoods.ClassOf(cartonItem), "the carton is tobacco without any registration");
		ctx.Equal(LF_EVehicleSearchGoodsClass.ALCOHOL, LF_VehicleSearchGoods.ClassOf(vodkaItem), "the bottle is alcohol without any registration");
		ctx.Equal(LF_EVehicleSearchGoodsClass.LEGAL, LF_VehicleSearchGoods.ClassOf(robber), "a character is not classed goods");

		IEntity car = LF_Utils.SpawnEntityPrefab(CAR, robber.GetOrigin() + "6 0 3");
		ctx.True(car != null, "the test car spawns");
		if (car)
		{
			ctx.Equal(2, LF_InventoryUtils.AddAmount(car, carton, 2), "two cartons go in the trunk");
			ctx.Equal(1, LF_InventoryUtils.AddAmount(car, vodka, 1), "a bottle goes in the trunk");

			string listing;
			ctx.Equal(3, LF_VehicleSearchGoods.Search(car, 0.0, 1000, listing), "the search finds the three classed units");
			ctx.True(listing.Contains("2 tobacco"), string.Format("the find lists the cartons as tobacco (got '%1')", listing));
			ctx.True(listing.Contains("1 alcohol"), string.Format("the find lists the bottle as alcohol (got '%1')", listing));

			ctx.Equal(3, LF_VehicleSearchGoods.Seize(car, 2000), "the seizure takes the cartons and the bottle");
			ctx.Equal(0, LF_InventoryUtils.GetAmount(car, carton), "no cartons are left");
			ctx.Equal(0, LF_InventoryUtils.GetAmount(car, vodka), "no bottle is left");
			LF_TestCharacterFixture.Delete(car);
		}

		LF_TestCharacterFixture.Delete(robber);
		LF_VehicleSearchGoods.ResetStatics();
	}
}
