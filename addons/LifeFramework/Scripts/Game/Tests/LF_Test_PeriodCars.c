// red-proof: 2026-11-14 `tools/cli test --no-build --only LF_Test_PeriodCars` with the Dana price expectation 1800 changed to 1801 failed `- the lot price comes from the vehicles config: ...V2101_beige.et (expected 1801, got 1800)`. Restored; green.
// tier: WORLD
//! The free 1.9 period cars (#981): the Dana 2101 and the Sputnik T601 are on the car lot with a
//! configured price, a locked child that loads, and no Rally DLC prefab. A bought Dana spawns locked,
//! with a plate and a key, like every other car on the lot.
class LF_Test_PeriodCars : LF_Test
{
	override string GetName()
	{
		return "vehicles/period-cars";
	}

	override void Run(LF_TestContext ctx)
	{
		array<ResourceName> catalogue = {LF_VehicleCatalog.DANA2101, LF_VehicleCatalog.SPUTNIK_T601};
		array<ResourceName> locked = {LF_VehicleCatalog.DANA2101_LOCKED, LF_VehicleCatalog.SPUTNIK_T601_LOCKED};
		array<int> price = {1800, 700};

		LF_VehiclePricesConfig config = LF_VehiclePricesConfig.Get();
		ctx.NotNull(config, "the shipped vehicle prices config loads");
		if (!config)
			return;

		for (int i = 0; i < catalogue.Count(); i++)
		{
			string tag = catalogue[i];
			ctx.True(Resource.Load(catalogue[i]).IsValid(), "the vanilla car loads: " + tag);
			ctx.True(Resource.Load(locked[i]).IsValid(), "the locked child loads: " + tag);
			ctx.Equal(price[i], config.PriceFor(LF_EVehiclePriceAction.BUY, catalogue[i]), "the lot price comes from the vehicles config: " + tag);
			ctx.Equal(price[i], config.PriceFor(LF_EVehiclePriceAction.BUY, locked[i]), "the locked child finds the same entry: " + tag);
			string vanillaPath = catalogue[i];
			string childPath = locked[i];
			vanillaPath.ToLower();
			childPath.ToLower();
			ctx.False(vanillaPath.Contains("rally"), "no Rally DLC prefab in the catalogue: " + tag);
			ctx.False(childPath.Contains("rally"), "no Rally DLC prefab in the spawn child: " + tag);
		}

		IEntity lotEntity = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.VEHICLE_LOT);
		ctx.NotNull(lotEntity, "Annex_Vehicle_Lot is placed");
		if (!lotEntity)
			return;
		LF_VehicleLotComponent lot = LF_Component<LF_VehicleLotComponent>.Find(lotEntity);
		ctx.NotNull(lot, "the lot carries LF_VehicleLotComponent");
		if (!lot)
			return;

		ctx.Equal(1800, lot.GetPrice(LF_VehicleCatalog.DANA2101), "the lot sells the Dana 2101");
		ctx.Equal(700, lot.GetPrice(LF_VehicleCatalog.SPUTNIK_T601), "the lot sells the Sputnik T601");

		string uid = "period-cars-e2e";
		LF_BankAccount account = LF_ATMManager.GetOrCreate(uid);
		ctx.NotNull(account, "the buyer has a bank account");
		if (!account)
			return;
		account.Deposit(1800);

		string vehicleId;
		int cashCharged;
		string reasonKey;
		ctx.True(lot.Buy(uid, LF_VehicleCatalog.DANA2101, null, vehicleId, cashCharged, reasonKey), "buy spawns the Dana");
		if (ctx.FailureCount() > 0)
			return;

		ctx.EqualStr(LF_VehicleCatalog.DANA2101, LF_VehicleOwnership.GetPrefab(vehicleId), "the record stores the vanilla Dana");
		ctx.False(LF_VehicleOwnership.GetPlate(vehicleId).IsEmpty(), "the bought Dana has a plate");
		ctx.True(LF_VehicleOwnership.IsLocked(vehicleId), "the Dana record is locked");

		IEntity vehicle = LF_VehicleOwnership.GetVehicle(vehicleId);
		ctx.NotNull(vehicle, "the bought Dana is in the world");
		if (!vehicle)
			return;
		ctx.True(LF_Utils.GetPrefabName(vehicle).Contains("V2101"), "the spawn is the Dana prefab");
		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(vehicle);
		ctx.NotNull(vehicleLock, "the Dana carries LF_VehicleLockComponent");
		if (vehicleLock)
			ctx.True(vehicleLock.IsVehicleLocked(), "the Dana spawns locked");
		ctx.True(LF_VehicleOwnership.GetKey(vehicleId) != null, "the buyer gets a key");
	}
}
