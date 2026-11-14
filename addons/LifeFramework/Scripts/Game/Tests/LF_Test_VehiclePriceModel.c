// red-proof: 2026-11-12 `tools/cli test --no-build --only LF_Test_VehiclePriceModel` with the impound-override expectation 99 changed to 98 failed `- the impound override wins`. Restored; green.
// tier: LOGIC
//! Vehicle prices (#688): each action price is base times its multiplier, an override wins, a vehicle
//! is found by GUID whatever the path casing, and the shipped config reproduces today's car-lot,
//! chop-shop and impound numbers.
class LF_Test_VehiclePriceModel : LF_Test
{
	override string GetName()
	{
		return "vehicles/price-model";
	}

	override void Run(LF_TestContext ctx)
	{
		// The arithmetic.
		ctx.Equal(1000, LF_VehiclePriceModel.Price(1000, 100, -1), "100 percent is the base price");
		ctx.Equal(500, LF_VehiclePriceModel.Price(1000, 50, -1), "50 percent of 1000");
		ctx.Equal(900, LF_VehiclePriceModel.Price(8000, 30, -1, 3000), "the cap applies before the multiplier");
		ctx.Equal(7, LF_VehiclePriceModel.Price(1000, 50, 7, 3000), "an override wins over base, multiplier and cap");
		ctx.Equal(0, LF_VehiclePriceModel.Price(1000, 50, 0), "an override of 0 is free, not unset");
		ctx.Equal(0, LF_VehiclePriceModel.Price(-5, 50, -1), "a negative base never pays out");

		// A config built in script: every action is base times its multiplier; an override wins.
		LF_VehiclePricesConfig config = new LF_VehiclePricesConfig();
		config.m_iBuyPercent = 100;
		config.m_iSellBackPercent = 50;
		config.m_iChopPercent = 30;
		config.m_iChopValueCap = 0;
		config.m_iImpoundPercent = 10;
		config.m_iGaragePercent = 5;
		config.m_aVehicles = {};
		ResourceName car = LF_VehicleCatalog.S105;
		ResourceName spawnChild = LF_VehicleCatalog.S105_LOCKED;
		ResourceName pinned = LF_VehicleCatalog.UAZ469;
		string lowerText = car;
		lowerText.ToLower();
		ResourceName lowerCar = lowerText;
		config.m_aVehicles.Insert(LF_VehiclePriceEntry.Make(car, spawnChild, 2000));
		LF_VehiclePriceEntry pin = LF_VehiclePriceEntry.Make(pinned, "", 2000);
		pin.m_iImpoundOverride = 99;
		config.m_aVehicles.Insert(pin);

		ctx.Equal(2000, config.PriceFor(LF_EVehiclePriceAction.BUY, car), "buy is base times 100 percent");
		ctx.Equal(1000, config.PriceFor(LF_EVehiclePriceAction.SELL_BACK, car), "sell-back is base times 50 percent");
		ctx.Equal(600, config.PriceFor(LF_EVehiclePriceAction.CHOP, car), "chop is base times 30 percent");
		ctx.Equal(200, config.PriceFor(LF_EVehiclePriceAction.IMPOUND, car), "impound is base times 10 percent");
		ctx.Equal(100, config.PriceFor(LF_EVehiclePriceAction.GARAGE, car), "garage is base times 5 percent");
		ctx.Equal(99, config.PriceFor(LF_EVehiclePriceAction.IMPOUND, pinned), "the impound override wins");
		ctx.Equal(600, config.PriceFor(LF_EVehiclePriceAction.CHOP, pinned), "an override moves only its own action");
		ctx.Equal(600, config.PriceFor(LF_EVehiclePriceAction.CHOP, spawnChild), "the locked child finds the same entry");
		ctx.Equal(600, config.PriceFor(LF_EVehiclePriceAction.CHOP, lowerCar), "the GUID decides, not the path casing");
		ctx.Equal(-1, config.PriceFor(LF_EVehiclePriceAction.BUY, LF_VehicleCatalog.GARAGE_SHELL), "an unlisted vehicle is -1, never free");

		// Backcompat: the shipped config reproduces today's prices.
		LF_VehiclePricesConfig shipped = LF_VehiclePricesConfig.Get();
		ctx.True(shipped != null, "the shipped vehicle prices config loads");
		if (!shipped)
			return;

		array<ResourceName> lot = {LF_VehicleCatalog.S105, LF_VehicleCatalog.UAZ469, LF_VehicleCatalog.S1203, LF_VehicleCatalog.UAZ452, LF_VehicleCatalog.M923, LF_VehicleCatalog.DANA2101, LF_VehicleCatalog.SPUTNIK_T601};
		array<ResourceName> spawn = {LF_VehicleCatalog.S105_LOCKED, LF_VehicleCatalog.UAZ469_LOCKED, LF_VehicleCatalog.S1203_LOCKED, LF_VehicleCatalog.UAZ452_LOCKED, LF_VehicleCatalog.M923_LOCKED, LF_VehicleCatalog.DANA2101_LOCKED, LF_VehicleCatalog.SPUTNIK_T601_LOCKED};
		array<int> lotPrice = {1000, 2500, 4000, 3500, 8000, 1800, 700}; // the Street_Vehicles.layer offers
		for (int i = 0; i < lot.Count(); i++)
		{
			string tag = lot[i];
			ctx.Equal(lotPrice[i], shipped.PriceFor(LF_EVehiclePriceAction.BUY, lot[i]), "car lot price unchanged: " + tag);
			int chopNow = LF_ChopShopRules.Payout(LF_ChopShopConfig.ValueOf(spawn[i]), LF_ChopShopConfig.Cap(), LF_ChopShopConfig.Percent());
			ctx.Equal(chopNow, shipped.PriceFor(LF_EVehiclePriceAction.CHOP, spawn[i]), "chop payout unchanged: " + tag);
			ctx.Equal(LF_GarageConfig.ImpoundFee(), shipped.PriceFor(LF_EVehiclePriceAction.IMPOUND, spawn[i]), "impound fee unchanged: " + tag);
			ctx.Equal(0, shipped.PriceFor(LF_EVehiclePriceAction.GARAGE, spawn[i]), "the garage stays free: " + tag);
		}
	}
}
