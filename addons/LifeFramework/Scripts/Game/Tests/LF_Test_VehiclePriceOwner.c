// red-proof: observed 2026-11-12 with `tools\cli test --no-build --only LF_Test_VehiclePriceOwner`: LF_VehicleOffer.PriceIn ignoring the config failed `- an owner base price changes the lot price (expected 1500, got 1000)`; dropping the PRICE_MIN/PRICE_MAX range from the basePrice read failed `- a base price above 1000000 is refused`. Restored; green.
// tier: LOGIC
//! The owner's `vehicles` section (#283): a vehicle's price in config.json changes what the car lot
//! charges, merged field by field per vehicle (keyed by prefab GUID), and a number outside the
//! schema's range is refused with the shipped value kept.
class LF_Test_VehiclePriceOwner : LF_Test
{
	override string GetName()
	{
		return "config/vehicle-prices-owner";
	}

	//------------------------------------------------------------------------------------------------
	//! A core vehicle prices config built in script: the S105 at 1000 (with its locked child) and the UAZ at 2500.
	protected LF_VehiclePricesConfig MakeCore()
	{
		LF_VehiclePricesConfig config = new LF_VehiclePricesConfig();
		config.m_iBuyPercent = 100;
		config.m_iSellBackPercent = 50;
		config.m_iChopPercent = 30;
		config.m_iChopValueCap = 3000;
		config.m_iImpoundPercent = 10;
		config.m_iGaragePercent = 0;
		config.m_aVehicles = {};
		config.m_aVehicles.Insert(LF_VehiclePriceEntry.Make(LF_VehicleCatalog.S105, LF_VehicleCatalog.S105_LOCKED, 1000));
		LF_VehiclePriceEntry uaz = LF_VehiclePriceEntry.Make(LF_VehicleCatalog.UAZ469, LF_VehicleCatalog.UAZ469_LOCKED, 2500);
		uaz.m_iImpoundOverride = 250;
		config.m_aVehicles.Insert(uaz);
		return config;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_VehiclePricesConfig Merge(string sectionJson)
	{
		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"vehicles\":" + sectionJson + "}", "test");
		LF_VehiclePricesConfig config = MakeCore();
		config.ApplyLayers();
		return config;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		string s105 = LF_VehiclePriceModel.GuidOf(LF_VehicleCatalog.S105);
		string s105Locked = LF_VehiclePriceModel.GuidOf(LF_VehicleCatalog.S105_LOCKED);
		string uazGuid = LF_VehiclePriceModel.GuidOf(LF_VehicleCatalog.UAZ469);
		LF_VehicleOffer lotOffer = LF_VehicleOffer.Make(LF_VehicleCatalog.S105, 1000, "S105");
		LF_VehicleOffer uazOffer = LF_VehicleOffer.Make(LF_VehicleCatalog.UAZ469, 2500, "UAZ");

		// An owner base price for one vehicle changes what the lot charges for it, and only it.
		LF_VehiclePricesConfig config = Merge("{\"vehicles\":{\"" + s105 + "\":{\"basePrice\":1500}}}");
		ctx.Equal(1500, lotOffer.PriceIn(config), "an owner base price changes the lot price");
		ctx.Equal(2500, uazOffer.PriceIn(config), "a vehicle the owner left out keeps its shipped price");
		ctx.Equal(150, config.PriceFor(LF_EVehiclePriceAction.IMPOUND, LF_VehicleCatalog.S105_LOCKED), "the actions derive from the new base price");
		ctx.Equal(250, config.PriceFor(LF_EVehiclePriceAction.IMPOUND, LF_VehicleCatalog.UAZ469), "an override the owner left out stays");
		ctx.Equal(2, config.m_aVehicles.Count(), "a known vehicle is merged, not added");

		// The locked child's GUID, braces and lower case reach the same entry.
		string braced = "{" + s105Locked + "}";
		braced.ToLower();
		config = Merge("{\"vehicles\":{\"" + braced + "\":{\"basePrice\":1700,\"impoundOverride\":40}}}");
		ctx.Equal(1700, lotOffer.PriceIn(config), "the locked child's GUID finds the lot's entry");
		ctx.Equal(40, config.PriceFor(LF_EVehiclePriceAction.IMPOUND, LF_VehicleCatalog.S105_LOCKED), "an owner impound override wins");
		ctx.Equal(510, config.PriceFor(LF_EVehiclePriceAction.CHOP, LF_VehicleCatalog.S105_LOCKED), "chop follows the new base price");

		// A buy override pins the price; the section's percents apply to every vehicle.
		config = Merge("{\"buyPercent\":150,\"vehicles\":{\"" + uazGuid + "\":{\"buyOverride\":1}}}");
		ctx.Equal(1500, lotOffer.PriceIn(config), "the owner buy percent scales the base price");
		ctx.Equal(1, uazOffer.PriceIn(config), "a buy override wins over the percent");
		ctx.Equal(30, config.m_iChopPercent, "a percent the owner left out stays shipped");

		// A number outside the schema's range is refused; the shipped value stays.
		config = Merge("{\"buyPercent\":5000,\"vehicles\":{\"" + s105 + "\":{\"basePrice\":2000000,\"buyOverride\":-2,\"impoundOverride\":-5}}}");
		ctx.Equal(100, config.m_iBuyPercent, "a percent above the range is refused");
		ctx.Equal(1000, lotOffer.PriceIn(config), "a base price above 1000000 is refused");
		LF_VehiclePriceEntry kept = config.Find(LF_VehicleCatalog.S105);
		ctx.NotNull(kept, "the S105 entry is still there");
		if (kept)
			ctx.Equal(-1, kept.m_iBuyOverride, "an override below -1 is refused");
		config = Merge("{\"vehicles\":{\"" + s105 + "\":{\"basePrice\":-1}}}");
		ctx.Equal(1000, lotOffer.PriceIn(config), "a negative base price is refused");
		config = Merge("{\"vehicles\":{\"" + s105 + "\":{\"basePrice\":1000000}}}");
		ctx.Equal(1000000, lotOffer.PriceIn(config), "the top of the range is accepted");

		// A new vehicle needs a GUID key and a base price; an unpriced one would sell for nothing.
		config = Merge("{\"vehicles\":{\"1111222233334444\":{\"buyOverride\":5},\"not-a-guid\":{\"basePrice\":9},\"AAAABBBBCCCCDDDD\":{\"basePrice\":777}}}");
		ctx.Equal(3, config.m_aVehicles.Count(), "only the vehicle with a GUID key and a base price is added");
		ctx.Equal(777, config.PriceFor(LF_EVehiclePriceAction.BUY, "{AAAABBBBCCCCDDDD}"), "the added vehicle is priced by its GUID");
		ctx.Equal(-1, config.PriceFor(LF_EVehiclePriceAction.BUY, "{1111222233334444}"), "a vehicle with no base price is not added");

		// No owner section: the shipped prices stand.
		LF_ConfigService.Reset();
		config = MakeCore();
		config.ApplyLayers();
		ctx.Equal(1000, lotOffer.PriceIn(config), "no owner layer, no change");
		LF_ConfigService.Reset();
	}
}
