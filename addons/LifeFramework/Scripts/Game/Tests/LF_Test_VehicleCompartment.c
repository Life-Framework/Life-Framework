// red-proof: 2026-11-19 with `test --no-build --red LF_Test_VehicleCompartment`: RED PROOF OK (full search found 0 of 2, quick-search take-nothing check failed). Review fix: with AllowsAccessFrom returning true (no HandleAccessValidation) "a stranger's inventory may not take from the compartment" and "an unowned car's compartment refuses everyone" failed; green with the owner check.
//! vehicles/compartment (#3434, Q9 of #1035): a fitted hidden compartment is a second storage that hangs off the
//! vehicle, not in its inventory tree. The quick search (LF_ChainSearch.SearchVehicle) lists the normal storage only
//! and misses it; the full search (LF_VehicleCompartment.SearchFull) lists it too and seizes it, once. The test
//! deletes what it spawns.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_VehicleCompartment : LF_Test
{
	protected static const ResourceName UAZ = "{C8B16ABCA57679A2}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469_covered_CIV_blue.et";
	protected static const vector SPOT = "140 0 60";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "vehicles/compartment";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ChainCatalog.ResetStatics();
		LF_ChainCatalog.Build(LF_ResourceChainCatalog.GetConfig());

		vector at = SPOT;
		at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]);
		IEntity vehicle = LF_Utils.SpawnEntityPrefab(UAZ, at);
		ctx.NotNull(vehicle, "a UAZ469 spawns");
		if (vehicle)
			Check(ctx, vehicle);

		if (vehicle)
			SCR_EntityHelper.DeleteEntityAndChildren(vehicle);

		LF_ChainSearch.ResetSearchCooldowns();
		LF_ChainCatalog.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	//! Vanilla's inventory can reach the compartment's storage too (the nearby list, which "Open the
	//! panel" itself relies on), so the server refuses a take by anyone but the car's owner.
	protected void CheckAccess(LF_TestContext ctx, IEntity vehicle, IEntity panel)
	{
		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(vehicle);
		LF_VehicleCompartmentStorageComponent compartment = LF_VehicleCompartmentStorageComponent.Cast(panel.FindComponent(LF_VehicleCompartmentStorageComponent));
		ctx.NotNull(vehicleLock, "the car has a lock with an owner key");
		ctx.NotNull(compartment, "the compartment has its storage manager");
		if (!vehicleLock || !compartment)
			return;

		IEntity owner = LF_TestCharacterFixture.Spawn(ctx);
		IEntity stranger = LF_TestCharacterFixture.Spawn(ctx);
		if (owner && stranger)
		{
			InventoryStorageManagerComponent ownerManager = LF_InventoryUtils.GetResponsibleStorageManager(owner);
			InventoryStorageManagerComponent strangerManager = LF_InventoryUtils.GetResponsibleStorageManager(stranger);
			vehicleLock.SetOwnerUid("lf-test-compartment-owner");
			LF_Utils.SetUidForTests(owner, "lf-test-compartment-owner");
			ctx.True(compartment.AllowsAccessFrom(ownerManager), "the car's owner may take from the compartment");
			LF_Utils.SetUidForTests(stranger, "lf-test-compartment-stranger");
			ctx.False(compartment.AllowsAccessFrom(strangerManager), "a stranger's inventory may not take from the compartment");
			vehicleLock.SetOwnerUid("");
			ctx.False(compartment.AllowsAccessFrom(strangerManager), "an unowned car's compartment refuses everyone");
			LF_Utils.SetUidForTests(null, "");
		}

		LF_TestCharacterFixture.Delete(owner);
		LF_TestCharacterFixture.Delete(stranger);
	}

	//------------------------------------------------------------------------------------------------
	protected void Check(LF_TestContext ctx, IEntity vehicle)
	{
		ctx.True(LF_VehicleCompartment.Find(vehicle) == null, "a stock car has no compartment");
		ctx.Equal(0, LF_VehicleCompartment.SearchFull(vehicle, 0.0), "a stock car's full search finds nothing");

		LF_VehicleWorldSave.Hold(vehicle); // an owned car: LF_VehicleOwnership holds every car it spawns (#3614)
		IEntity panel = LF_VehicleCompartment.Fit(vehicle);
		ctx.NotNull(panel, "fitting builds the compartment");
		if (!panel)
			return;

		ctx.False(LF_VehicleWorldSave.IsSaved(panel), "the compartment of a car held out of the engine save is held too (no orphan after a restart)");
		CheckAccess(ctx, vehicle, panel);

		ctx.True(LF_VehicleCompartment.Fit(vehicle) == panel, "fitting twice keeps the one compartment");
		ctx.True(LF_VehicleCompartment.Find(vehicle) == panel, "the fitted compartment is found again");

		InventoryStorageManagerComponent bag = LF_InventoryUtils.GetResponsibleStorageManager(panel);
		ctx.NotNull(bag, "the compartment takes goods");
		if (!bag)
			return;

		if (!RedProof())
			ctx.Equal(2, LF_InventoryUtils.AddAmount(bag, LF_Test_AlcoholDrinks.MOONSHINE, 2), "two bottles go in the compartment");

		// Quick search: the normal storage only.
		int units;
		int load;
		array<IEntity> goods = {};
		LF_ChainSearch.Collect(vehicle, goods, units, load);
		ctx.Equal(0, goods.Count(), "the quick search's inventory walk does not see the compartment");
		ctx.Equal(0, LF_ChainSearch.SearchVehicle(vehicle, 0.0), "the quick search misses the compartment");
		ctx.Equal(2, LF_InventoryUtils.GetAmount(bag, LF_Test_AlcoholDrinks.MOONSHINE), "a quick search takes nothing from it");

		// Full search: finds and seizes it, once.
		ctx.Equal(2, LF_VehicleCompartment.SearchFull(vehicle, 0.0), "the full search finds both bottles");
		ctx.Equal(0, LF_InventoryUtils.GetAmount(bag, LF_Test_AlcoholDrinks.MOONSHINE), "the full search seized them");
		ctx.Equal(0, LF_VehicleCompartment.SearchFull(vehicle, 0.0), "a second full search finds nothing");

		// An ordinary item in the compartment is never seized.
		LF_InventoryUtils.AddAmount(bag, "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et", 1);
		ctx.Equal(0, LF_VehicleCompartment.SearchFull(vehicle, 0.0), "an apple in the compartment is not chain goods");
		ctx.Equal(1, LF_InventoryUtils.GetAmount(bag, "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et"), "the apple stays");

		// A roll above the find chance misses even a full search (the same roll the quick search uses).
		if (!RedProof())
			LF_InventoryUtils.AddAmount(bag, LF_Test_AlcoholDrinks.MOONSHINE, 1);

		int bulk;
		LF_ChainSearch.IsGood(LF_Test_AlcoholDrinks.MOONSHINE, bulk);
		if (LF_ChainSearch.FindChance(bulk) < 0.99)
		{
			ctx.Equal(0, LF_VehicleCompartment.SearchFull(vehicle, 0.99), "a full search can miss on a high roll");
			ctx.Equal(1, LF_InventoryUtils.GetAmount(bag, LF_Test_AlcoholDrinks.MOONSHINE), "a missed search seizes nothing");
		}
	}
}
