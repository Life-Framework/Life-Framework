// red-proof: observed 2026-11-16 with `tools\cli test --no-build --only LF_Test_VehicleRemoveByBaseGuid` on the unfixed behaviour (IsRemoved false, IsOfferedIn true): failed `- the UAZ is recorded as removed` and `- the lot does not offer the removed UAZ`; green after the fix.
// tier: LOGIC
//! #1741: `op: remove` keyed by the base prefab GUID (the key LF_VehiclePrices.conf lists) takes the
//! vehicle off the lot. A removed vehicle is unpriced, and the lot used to fall back to the price
//! written on the offer, so it was still shown and sold.
class LF_Test_VehicleRemoveByBaseGuid : LF_Test
{
	override string GetName()
	{
		return "config/vehicle-remove-by-base-guid";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_VehiclePricesConfig config = new LF_VehiclePricesConfig();
		config.m_iBuyPercent = 100;
		config.m_aVehicles = {};
		config.m_aVehicles.Insert(LF_VehiclePriceEntry.Make(LF_VehicleCatalog.S105, LF_VehicleCatalog.S105_LOCKED, 1000));
		config.m_aVehicles.Insert(LF_VehiclePriceEntry.Make(LF_VehicleCatalog.UAZ469, LF_VehicleCatalog.UAZ469_LOCKED, 2500));

		LF_VehicleOffer s105Offer = LF_VehicleOffer.Make(LF_VehicleCatalog.S105, 1000, "#S105");
		LF_VehicleOffer uazOffer = LF_VehicleOffer.Make(LF_VehicleCatalog.UAZ469, 2500, "#UAZ");
		ctx.True(uazOffer.IsOfferedIn(config), "before the override the UAZ is offered");

		string uazGuid = LF_VehiclePriceModel.GuidOf(LF_VehicleCatalog.UAZ469);
		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"vehicles\":{\"vehicles\":{\"" + uazGuid + "\":{\"op\":\"remove\"}}}}", "test");
		LF_ConfigReader reader = LF_ConfigService.OpenOwnerSection("vehicles");
		ctx.NotNull(reader, "the vehicles section opens");
		if (!reader)
			return;

		config.ApplyOwner(reader, "test-owner.json");
		ctx.Equal(0, reader.GetRejectedCount(), "remove by the base GUID: nothing refused");
		ctx.Equal(1, config.m_aVehicles.Count(), "remove by the base GUID: one vehicle left");
		ctx.True(config.IsRemoved(LF_VehicleCatalog.UAZ469), "the UAZ is recorded as removed");
		ctx.False(uazOffer.IsOfferedIn(config), "the lot does not offer the removed UAZ");
		ctx.True(s105Offer.IsOfferedIn(config), "the S105 is still offered");

		// An unlisted vehicle that was never removed keeps the offer's own price and stays offered.
		LF_VehicleOffer other = LF_VehicleOffer.Make("{AAAABBBBCCCCDDDD}Prefabs/Vehicles/X.et", 900, "#X");
		ctx.True(other.IsOfferedIn(config), "a never-listed vehicle is still offered");
		ctx.Equal(900, other.PriceIn(config), "a never-listed vehicle keeps the lot's own price");

		LF_ConfigService.Reset();
	}
}
