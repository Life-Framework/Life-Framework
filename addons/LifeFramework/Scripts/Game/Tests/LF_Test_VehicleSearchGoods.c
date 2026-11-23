// red-proof: 2026-11-23 `test --red LF_Test_VehicleSearchGoods` RED-OK: with the seizure skipped, "no cartons are left (expected 0, got 2)", "no bottle is left (expected 0, got 1)", "a second seizure takes nothing (expected 0, got 3)"; with it, green.
//! A stop search for contraband in a car (crime B7, #4196): the search lists the goods in the car's storage
//! by class, a seizure removes exactly those goods once, and legal goods stay in the car. Stand-in items
//! are registered as classed goods (the pack's carton and bottle carry the marker component). Deletes what it spawns.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_VehicleSearchGoods : LF_Test
{
	static const ResourceName CAR = "{54C3CC22DEBD57BE}Prefabs/Vehicles/Wheeled/S105/S105_beige.et";
	static const ResourceName TOBACCO = "{C3E35D690317B5BE}Prefabs/Items/Food/Plum.et";
	static const ResourceName ALCOHOL = "{A231A5F8D479B5DC}Prefabs/Items/Drinks/WaterBottle.et";
	static const ResourceName LEGAL = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";

	override string GetName()
	{
		return "police/vehicle-search-goods";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_VehicleSearchGoods.ResetStatics();
		LF_VehicleSearchGoods.Register(TOBACCO, LF_EVehicleSearchGoodsClass.TOBACCO);
		LF_VehicleSearchGoods.Register(ALCOHOL, LF_EVehicleSearchGoodsClass.ALCOHOL);

		IEntity anchor = LF_TestCharacterFixture.Spawn(ctx);
		if (!anchor)
			return;

		IEntity car = LF_Utils.SpawnEntityPrefab(CAR, anchor.GetOrigin() + "6 0 3");
		ctx.True(car != null, "the test car spawns");
		if (!car)
		{
			LF_TestCharacterFixture.Delete(anchor);
			return;
		}

		ctx.Equal(2, LF_InventoryUtils.AddAmount(car, TOBACCO, 2), "two cartons go in the trunk");
		ctx.Equal(1, LF_InventoryUtils.AddAmount(car, ALCOHOL, 1), "a bottle goes in the trunk");
		ctx.Equal(3, LF_InventoryUtils.AddAmount(car, LEGAL, 3), "three legal items go in the trunk");

		int nowMs = 1000;
		string listing;
		ctx.Equal(0, LF_VehicleSearchGoods.Seize(car, nowMs), "nothing is seized before a search finds it");
		ctx.Equal(0, LF_VehicleSearchGoods.Search(car, 0.99, nowMs, listing), "a high roll finds nothing");
		ctx.False(LF_VehicleSearchGoods.HasPending(car, nowMs), "a clean search marks nothing");

		ctx.Equal(3, LF_VehicleSearchGoods.Search(car, 0.0, nowMs, listing), "the search finds the three classed units");
		ctx.True(listing.Contains("2 tobacco"), string.Format("the find lists the cartons as tobacco (got '%1')", listing));
		ctx.True(listing.Contains("1 alcohol"), string.Format("the find lists the bottle as alcohol (got '%1')", listing));
		ctx.False(listing.Contains("3"), "legal goods are not in the find");
		ctx.True(LF_VehicleSearchGoods.HasPending(car, nowMs), "the find waits for the seizure");

		int seized = 0;
		if (!RedProof())
			seized = LF_VehicleSearchGoods.Seize(car, nowMs + 1000);
		ctx.Equal(3, seized, "the seizure takes the three classed units");
		ctx.Equal(0, LF_InventoryUtils.GetAmount(car, TOBACCO), "no cartons are left");
		ctx.Equal(0, LF_InventoryUtils.GetAmount(car, ALCOHOL), "no bottle is left");
		ctx.Equal(3, LF_InventoryUtils.GetAmount(car, LEGAL), "the legal goods stay in the car");
		ctx.Equal(0, LF_VehicleSearchGoods.Seize(car, nowMs + 2000), "a second seizure takes nothing");

		LF_TestCharacterFixture.Delete(car);
		LF_TestCharacterFixture.Delete(anchor);
		LF_VehicleSearchGoods.ResetStatics();
	}
}
