// red-proof: 2026-11-22 `tools\cli test --no-build --red LF_Test_HouseSalePercent`: `RED-OK property/house-sale-percent (fails with the hook on, 10 assertions)`; RedProof leaves the owner key out.

//! rc 0.2.0 pacing (overnight smoke: the cheapest island home is 4000 while a newcomer holds 1100): the owner key
//! houses.salePercent sells every house and plot at that percent of its listed price, server and client alike (the
//! config stream carries it, so the "Buy house ($price)" label shows what the server charges). 100 is the shipped price.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_HouseSalePercent : LF_Test
{
	override string GetName()
	{
		return "property/house-sale-percent";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The pure rule.
		ctx.Equal(4000, LF_HousesConfig.PriceAt(4000, 100), "100 percent is the listed price");
		ctx.Equal(2000, LF_HousesConfig.PriceAt(4000, 50), "50 percent halves a cottage");
		ctx.Equal(1400, LF_HousesConfig.PriceAt(2800, 50), "50 percent halves the cheapest plot");
		ctx.Equal(1234, LF_HousesConfig.PriceAt(2468, 50), "the price stays exact in whole cash");
		ctx.Equal(400, LF_HousesConfig.PriceAt(4000, 1), "a percent below the minimum is clamped to 10");
		ctx.Equal(0, LF_HousesConfig.PriceAt(0, 50), "a house with no price stays unpriced");

		// The owner section.
		LF_ConfigService.Reset();
		LF_ConfigStream.Reset();
		string section = "{\"salePercent\":50}";
		if (RedProof())
			section = "{}";

		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"houses\":" + section + "}", "test");
		LF_HousesConfig.Reload();
		ctx.Equal(2000, LF_HousesConfig.SalePrice(4000), "houses.salePercent 50 sells a 4000 home for 2000");

		// The stream carries it to a client.
		string payload = LF_ConfigStream.BuildPayload(null, true, null, null, null, null, null, false, null, LF_HousesConfig.Get());
		ctx.True(payload.Contains("\"salePercent\":50") || RedProof(), "the config stream carries the sale percent");

		LF_ConfigService.Reset();
		LF_ConfigStream.Reset();
		LF_HousesConfig.Reload();
		ctx.Equal(4000, LF_HousesConfig.SalePrice(4000), "with no owner key the listed price stands");
	}
}
