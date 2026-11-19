// red-proof: 2026-11-19 `tools\cli test --no-build --red LF_Test_HouseMarker,LF_Test_EveronHomePrices` printed `RED PROOF OK` (#3584): RedProof() reads the core catalogue alone, so `saint-philippe.home.sp-b83 is for sale at a real price` fails. Green in the pack leg without the hook.
//! #3584: under the Everon 1989 header (the pack leg of `tools\cli test`), the homes of Saint-Philippe are priced by the
//! pack's property overlay folder (`LF_Property_Everon1989/LF_Property_Homes_SaintPhilippe.conf`), one row per tier sampled
//! here, so a For sale sign there offers Buy. The core catalogue ships two neutral example houses only.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronHomePrices : LF_Test
{
	override string GetName()
	{
		return "everon/home-prices";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_PropertyConfig.Reload();
		if (RedProof())
			LF_PropertyConfig.Reload("{D9770AC8486445C3}Configs/Property/LF_Property.conf");

		array<string> ids = {"saint-philippe.home.sp-b83", "saint-philippe.home.sp-b63", "saint-philippe.home.sp-b10", "saint-philippe.home.sp-b16", "saint-philippe.home.sp-b01", "saint-philippe.home.sp-b75", "saint-philippe.home.dk-b45"};
		array<int> prices = {4000, 6000, 8000, 10000, 13000, 20000, 20000};
		foreach (int i, string id : ids)
		{
			ctx.True(LF_PropertyConfig.HousePrice(id) > 0, id + " is for sale at a real price");
			ctx.True(!LF_PropertyConfig.HouseTierLabel(id).IsEmpty(), id + " has a tier label");
			ctx.Equal(prices[i], LF_PropertyConfig.HousePrice(id), id + " costs its tier's price");
		}

		ctx.Equal(0, LF_PropertyConfig.PlotPrice("saint-philippe.home.sp-b83"), "a house id is no plot key");
		ctx.True(LF_PropertyConfig.HousePrice("sale-house-a") > 0, "the core example house stays in the catalogue");

		LF_PropertyConfig.Reload();
	}
}
