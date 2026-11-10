// red-proof: 2026-11-10 `tools/cli test --no-build --only LF_Test_ChopShopPayout` with the keyed-owner expectation flipped to NONE failed `- an owned car whose key the worker holds is refused`. Restored; green.
// tier: LOGIC
//! Chop shop rules (#415): the payout is a capped fraction of the vehicle value, an owned car whose
//! key the worker holds is refused, only a stolen car is taken, the cooldown holds, and a hot car
//! scrapped from the ownership record leaves no record behind.
class LF_Test_ChopShopPayout : LF_Test
{
	override string GetName()
	{
		return "chopshop/payout";
	}

	override void Run(LF_TestContext ctx)
	{
		// The payout is a fraction of the value, never more than the value.
		ctx.Equal(300, LF_ChopShopRules.Payout(1000, 3000, 30), "30 percent of a 1000 car");
		ctx.Equal(900, LF_ChopShopRules.Payout(8000, 3000, 30), "a truck counts for the cap only: 30 percent of 3000");
		ctx.Equal(1000, LF_ChopShopRules.Payout(1000, 0, 100), "no cap and 100 percent never pays above the value");
		ctx.Equal(1000, LF_ChopShopRules.Payout(1000, 3000, 250), "a percent above 100 is held at the value");
		ctx.Equal(0, LF_ChopShopRules.Payout(0, 3000, 30), "a worthless car pays nothing");
		ctx.Equal(0, LF_ChopShopRules.Payout(1000, 3000, 0), "0 percent pays nothing");

		// Who may start a car.
		ctx.Equal(LF_EChopCheck.NONE, LF_ChopShopRules.CheckLoad(true, false, true, false, false, false, false), "a stolen, empty, unowned car is taken");
		ctx.Equal(LF_EChopCheck.NONE, LF_ChopShopRules.CheckLoad(true, false, true, true, false, false, false), "a stolen owned car is taken by a thief without its key");
		ctx.Equal(LF_EChopCheck.KEYED_OWNER, LF_ChopShopRules.CheckLoad(true, false, true, true, true, false, false), "an owned car whose key the worker holds is refused");
		ctx.Equal(LF_EChopCheck.NOT_STOLEN, LF_ChopShopRules.CheckLoad(true, false, false, false, false, false, false), "a car nobody stole is refused");
		ctx.Equal(LF_EChopCheck.NO_VEHICLE, LF_ChopShopRules.CheckLoad(false, false, false, false, false, false, false), "an empty drop area is refused");
		ctx.Equal(LF_EChopCheck.OCCUPIED, LF_ChopShopRules.CheckLoad(true, true, true, false, false, false, false), "a car with someone inside is refused");
		ctx.Equal(LF_EChopCheck.COOLDOWN, LF_ChopShopRules.CheckLoad(true, false, true, false, false, true, false), "the yard cooling down refuses");
		ctx.Equal(LF_EChopCheck.BUSY, LF_ChopShopRules.CheckLoad(true, false, true, false, false, false, true), "a busy bench refuses");
		ctx.True(LF_ChopShopRules.RefusalKey(LF_EChopCheck.KEYED_OWNER) != "", "every refusal has a line to print");

		// The cooldown holds, then lifts.
		ctx.False(LF_ChopShopRules.OnCooldown(100, -1, 300), "no payout yet: no cooldown");
		ctx.True(LF_ChopShopRules.OnCooldown(100, 50, 300), "50 seconds after a payout the yard is cooling");
		ctx.False(LF_ChopShopRules.OnCooldown(400, 50, 300), "350 seconds after, it takes a car again");

		// The shipped config pays less than the value and caps it.
		ctx.True(LF_ChopShopConfig.Percent() > 0 && LF_ChopShopConfig.Percent() < 100, "the yard pays a fraction");
		ctx.True(LF_ChopShopConfig.Cap() > 0, "the shipped config caps the value");
		ctx.Equal(1000, LF_ChopShopConfig.ValueOf(LF_VehicleCatalog.S105_LOCKED), "the S105 has its listed value");

		// A scrapped vehicle leaves no ownership record, so a restart cannot impound it back.
		string id = "chop-test-car";
		LF_VehicleOwnership.Register(id, "chop-test-owner", id, "{54C3CC22DEBD57BE}Prefabs/Vehicles/Wheeled/S105/S105_beige.et", "", "chop-test-shop", "CH AA 7777", "#EL-Vehicle_S105", true);
		ctx.True(LF_VehicleOwnership.GetOwner(id) == "chop-test-owner", "the test car is registered");
		ctx.True(LF_VehicleOwnership.Scrap(id), "a registered car can be scrapped");
		ctx.True(LF_VehicleOwnership.GetOwner(id) == "", "a scrapped car has no owner on record");
		ctx.False(LF_VehicleOwnership.Scrap(id), "a second scrap finds nothing");
		ctx.False(LF_LicensePlateRegistry.IsLive("CH AA 7777"), "the scrapped plate is free again");
	}
}
